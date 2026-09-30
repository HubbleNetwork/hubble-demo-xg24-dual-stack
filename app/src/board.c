/*
 * Copyright (c) 2026 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>

#include <em_device.h>

#include "board.h"

LOG_MODULE_REGISTER(board, CONFIG_APP_LOG_LEVEL);

/*
 * The display is a Sharp LS013B7DH03 memory LCD on EUSART1. The app does not
 * enable the display driver, so these pins stay in their reset state unless
 * they are driven here:
 *   disp-en   - display ownership (PC9)
 *   extcomin  - VCOM toggle input (PC6)
 *   cs-gpios  - chip select, active high (PC8)
 *
 * The display is shared with the kit's board controller. Driving disp-en low
 * gives it to the board controller and powers it from the board controller's
 * rail, not VMCU (UG573). On USB power the board controller shows its own
 * screen. On battery the board controller is unpowered, so the display is
 * blank and draws nothing. Keeping it blank on USB would mean taking the
 * display and pulsing EXTCOMIN at 54-65 Hz from VMCU, which costs battery.
 */
#define DISPLAY_NODE   DT_NODELABEL(ls0xx_ls013b7dh03)

/* Power rail of the Si7021 (PD3). The app uses the die sensor instead. */
#define SENSOR_EN_NODE DT_NODELABEL(sensor_enable)

BUILD_ASSERT(DT_NODE_EXISTS(DISPLAY_NODE) && DT_NODE_EXISTS(SENSOR_EN_NODE),
	     "This app targets the xG24 Pro Kit (xg24_rb4187c)");

static const struct gpio_dt_spec _off_pins[] = {
	GPIO_DT_SPEC_GET(DISPLAY_NODE, disp_en_gpios),
	GPIO_DT_SPEC_GET(DISPLAY_NODE, extcomin_gpios),
	SPI_CS_GPIOS_DT_SPEC_GET(DISPLAY_NODE),
	GPIO_DT_SPEC_GET(SENSOR_EN_NODE, enable_gpios),
};

/* 0 degrees Celsius in hundredths of a Kelvin. */
#define ZERO_C_IN_CENTI_K 27315

int board_low_power_init(void)
{
	int err;

	for (size_t i = 0; i < ARRAY_SIZE(_off_pins); i++) {
		if (!gpio_is_ready_dt(&_off_pins[i])) {
			return -ENODEV;
		}

		err = gpio_pin_configure_dt(&_off_pins[i], GPIO_OUTPUT_INACTIVE);
		if (err != 0) {
			LOG_ERR("Failed to drive pin %u low (err %d)",
				_off_pins[i].pin, err);
			return err;
		}
	}

	LOG_INF("Display released, sensor rail off");

	return 0;
}

int16_t board_temperature_get(void)
{
	/*
	 * EMU_TEMP holds the last hardware sample in quarter-Kelvin: integer
	 * Kelvin in TEMP, the fraction in TEMPLSB. 0.25 K is 25 centi-K.
	 */
	uint32_t quarter_k =
		(EMU->TEMP & (_EMU_TEMP_TEMP_MASK | _EMU_TEMP_TEMPLSB_MASK)) >>
		_EMU_TEMP_TEMPLSB_SHIFT;

	return (int16_t)((int32_t)quarter_k * 25 - ZERO_C_IN_CENTI_K);
}
