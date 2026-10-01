/*
 * Copyright (c) 2026 Hubble Network, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PAYLOAD_H
#define PAYLOAD_H

#include <stddef.h>
#include <stdint.h>

/** Largest payload app_payload_get() writes, in bytes. */
#define APP_PAYLOAD_MAX_LEN 3U

/**
 * @brief Build the application payload from a fresh sensor reading.
 *
 * The same payload goes in the BLE beacon and in the satellite packet. The
 * layout follows the Hubble Dash sensor widget:
 *
 * @code
 * offset  size  field
 *   0      1    type: 0x01, Temperature High Res
 *   1      2    temperature, int16 little-endian, 0.01 degC
 * @endcode
 *
 * Satellite packets carry only 0, 4, 9 or 13 payload bytes, so the satellite
 * path in main.c zero-pads this payload to 4 bytes. The BLE beacon carries it
 * as is.
 *
 * @param buf Output buffer.
 * @param len Size of @p buf, at least @ref APP_PAYLOAD_MAX_LEN.
 *
 * @return Number of bytes written, or -EINVAL if @p buf is too small.
 */
int app_payload_get(uint8_t *buf, size_t len);

#endif /* PAYLOAD_H */
