/*
 * Copyright (c) 2026 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

/**
 * @brief Turn off the board peripherals that the demo does not use.
 *
 * Releases the Sharp memory LCD to the kit's board controller and turns off
 * the power rail of the Si7021 sensor on the xG24 Pro Kit mainboard. On
 * battery power the LCD is then blank; on USB power the board controller
 * draws its own screen. Call once at boot, before Bluetooth starts.
 *
 * @return 0 on success, negative errno on failure.
 */
int board_power_down_peripherals(void);

/**
 * @brief Read the EFR32 die temperature.
 *
 * The EMU samples the on-chip temperature sensor in hardware, so a read does
 * not wake anything or draw extra current.
 *
 * @return Temperature in hundredths of a degree Celsius.
 */
int16_t board_temperature_get(void);

#endif /* BOARD_H */
