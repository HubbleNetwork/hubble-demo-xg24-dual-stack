/*
 * Copyright (c) 2026 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <hubble/hubble.h>
#include <hubble/sat/packet.h>
#include <hubble/sat/pass_prediction.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/base64.h>

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "app_ble.h"
#include "board.h"
#include "payload.h"

LOG_MODULE_REGISTER(main, CONFIG_APP_LOG_LEVEL);

#define HUBBLE_MAX_SAT 6U

struct hubble_sat_orbital_params orb_params[HUBBLE_MAX_SAT];
struct hubble_sat_device_pos device_pos;
uint8_t orb_params_count;
uint64_t unix_time_ms;

/* Hubble device key, decoded from CONFIG_HUBBLE_DEVICE_KEY. */
static uint8_t _hubble_key[CONFIG_HUBBLE_KEY_SIZE];

/* Given by app_ble.c once provisioning (time + orbital params) completes. */
K_SEM_DEFINE(sync_sem, 0, 1);

/* Given by the satellite-pass timer when it is time to transmit. */
static K_SEM_DEFINE(sat_tx_sem, 0, 1);

static void _sat_timer_cb(struct k_timer *timer)
{
	ARG_UNUSED(timer);

	k_sem_give(&sat_tx_sem);
}

static K_TIMER_DEFINE(sat_timer, _sat_timer_cb, NULL);

/*
 * Compute the time until the next satellite pass over the device location.
 * A pass start in the past means we are in the middle of a pass; use the one
 * after it.
 */
static int _next_pass_wait_get(uint64_t *wait_ms)
{
	struct hubble_sat_pass_info pass_info = {0};
	uint64_t now_ms = hubble_time_get();
	int err;

	err = hubble_sat_next_pass_get(now_ms, &device_pos, &pass_info);
	if (err != 0) {
		return err;
	}

	if (pass_info.start <= now_ms) {
		LOG_INF("Pass ongoing or in the past, finding next...");

		err = hubble_sat_next_pass_get(
			pass_info.start + pass_info.duration, &device_pos,
			&pass_info);
		if (err != 0) {
			return err;
		}
	}

	LOG_INF("Next pass at %llu (unix epoch seconds)",
		pass_info.start / MSEC_PER_SEC);
	*wait_ms = pass_info.start - now_ms;

	return 0;
}

/*
 * Satellite packets carry only these payload sizes (see
 * hubble_sat_packet_get()). Any other length is rejected with -EINVAL.
 */
static const uint8_t _sat_payload_sizes[] = {0, 4, 9, 13};

BUILD_ASSERT(APP_PAYLOAD_MAX_LEN <= HUBBLE_SAT_PAYLOAD_MAX);

/* Smallest supported satellite payload size that holds @p len bytes. */
static size_t _sat_payload_size_get(size_t len)
{
	for (size_t i = 0; i < ARRAY_SIZE(_sat_payload_sizes); i++) {
		if (len <= _sat_payload_sizes[i]) {
			return _sat_payload_sizes[i];
		}
	}

	return HUBBLE_SAT_PAYLOAD_MAX;
}

static void _sat_transmit(void)
{
	struct hubble_sat_packet packet = {0};
	/* Zeroed, so the bytes after the app payload pad it out. */
	uint8_t payload[HUBBLE_SAT_PAYLOAD_MAX] = {0};
	size_t payload_len;
	int err;

	payload_len = app_payload_get(payload, sizeof(payload));
	payload_len = _sat_payload_size_get(payload_len);

	err = hubble_sat_packet_get(&packet, payload, payload_len);
	if (err != 0) {
		LOG_ERR("Failed to get sat packet (err %d)", err);
		return;
	}

	LOG_INF("Transmitting to satellite...");
	err = hubble_sat_packet_send(&packet, HUBBLE_SAT_RELIABILITY_NORMAL);
	if (err != 0) {
		LOG_ERR("Failed to send sat packet (err %d)", err);
	}
}

int main(void)
{
	uint64_t wait_ms;
	bool sat_tx;
	int err;

	LOG_INF("Hubble xG24 dual-mode demo started");

	/* Release the display and turn off the sensor rail before anything else. */
	err = board_low_power_init();
	if (err != 0) {
		LOG_WRN("Failed to turn off board peripherals (err %d)", err);
	}

	/* Decode the device key, if one was provided at build time. */
	if (strlen(CONFIG_HUBBLE_DEVICE_KEY) != 0) {
		size_t olen;

		err = base64_decode(_hubble_key, sizeof(_hubble_key), &olen,
				    CONFIG_HUBBLE_DEVICE_KEY,
				    strlen(CONFIG_HUBBLE_DEVICE_KEY));
		if (err != 0) {
			LOG_ERR("Invalid key provided!");
			return -EINVAL;
		}
	}

	/*
	 * Enable BLE and start connectable advertising.
	 */
	err = ble_init();
	if (err != 0) {
		LOG_ERR("Failed to init BLE (err %d)", err);
		return err;
	}

	LOG_INF("Waiting for provisioning over BLE...");
	k_sem_take(&sync_sem, K_FOREVER);

	err = hubble_init(unix_time_ms, _hubble_key);
	if (err != 0) {
		LOG_ERR("Failed to initialize Hubble Network (err %d)", err);
		return err;
	}

	err = hubble_sat_satellites_set(orb_params, orb_params_count);
	if (err != 0) {
		LOG_ERR("Failed to set satellite orbital params (err %d)", err);
		return err;
	}

	LOG_INF("Hubble Network initialized with %u satellite(s)",
		orb_params_count);

	/*
	 * From here on, errors skip one satellite transmission instead of
	 * stopping the device, so a demo kit keeps beaconing unattended.
	 */
	for (;;) {
		err = _next_pass_wait_get(&wait_ms);
		sat_tx = (err == 0);
		if (!sat_tx) {
			LOG_ERR("Failed to get next pass info (err %d)", err);
			wait_ms = CONFIG_APP_SAT_RETRY_SEC * MSEC_PER_SEC;
		}

#ifdef CONFIG_APP_SAT_DEBUG
		LOG_INF("Debug mode: next pass in 120 seconds");
		wait_ms = 120 * MSEC_PER_SEC;
		sat_tx = true;
#endif

		k_timer_start(&sat_timer, K_MSEC(wait_ms), K_NO_WAIT);

		err = ble_adv_start();
		if (err != 0) {
			LOG_ERR("Failed to start beacon adv (err %d)", err);
		} else {
			LOG_INF("Beaconing until next pass...");
		}

		k_sem_take(&sat_tx_sem, K_FOREVER);

		/* Stop the beacon and transmit to the satellite. */
		err = ble_adv_stop();
		if (err != 0) {
			LOG_ERR("Failed to stop beacon adv (err %d)", err);
			continue;
		}

		if (sat_tx) {
			_sat_transmit();
		}
	}

	return 0;
}
