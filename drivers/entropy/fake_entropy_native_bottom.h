/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef DRIVERS_ENTROPY_FAKE_ENTROPY_NATIVE_POSIX_H
#define DRIVERS_ENTROPY_FAKE_ENTROPY_NATIVE_POSIX_H

#ifdef __cplusplus
extern "C" {
#endif

long fenb_host_random(void);
void fenb_host_srandom(unsigned int seed);

#ifdef __cplusplus
}
#endif

#endif /* DRIVERS_ENTROPY_FAKE_ENTROPY_NATIVE_POSIX_H */
