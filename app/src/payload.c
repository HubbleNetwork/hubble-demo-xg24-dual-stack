/*
 * Copyright (c) 2026 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <hubble/ble.h>
#include <hubble/sat/packet.h>

#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include "board.h"
#include "payload.h"

LOG_MODULE_REGISTER(payload, CONFIG_APP_LOG_LEVEL);

/*
 * Type byte from the Hubble Dash sensor widget registry. The registry fixes
 * the value type and scale for each entry, see
 * https://hubble.com/docs/guides/dashboard/sensor-widget
 */
#define PAYLOAD_TYPE_TEMPERATURE_HIGH_RES 0x01U

BUILD_ASSERT(APP_PAYLOAD_MAX_LEN <= HUBBLE_BLE_MAX_DATA_LEN);
BUILD_ASSERT(APP_PAYLOAD_MAX_LEN <= HUBBLE_SAT_PAYLOAD_MAX);

size_t app_payload_get(uint8_t *buf, size_t len)
{
	int16_t centi_c;
	int32_t magnitude;

	if (len < APP_PAYLOAD_MAX_LEN) {
		return 0;
	}

	centi_c = board_temperature_get();

	buf[0] = PAYLOAD_TYPE_TEMPERATURE_HIGH_RES;
	sys_put_le16((uint16_t)centi_c, &buf[1]);

	magnitude = (centi_c < 0) ? -(int32_t)centi_c : centi_c;
	LOG_INF("Temperature: %s%d.%02d C", (centi_c < 0) ? "-" : "",
		magnitude / 100, magnitude % 100);

	return APP_PAYLOAD_MAX_LEN;
}
