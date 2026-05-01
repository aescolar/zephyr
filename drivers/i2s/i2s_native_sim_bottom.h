/*
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef DRIVERS_I2S_NATIVE_SIM_BOTTOM_H
#define DRIVERS_I2S_NATIVE_SIM_BOTTOM_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int ns_i2c_open_file_bottom(const char *pathname, bool read);

#ifdef __cplusplus
}
#endif

#endif /* DRIVERS_I2S_NATIVE_SIM_BOTTOM_H */
