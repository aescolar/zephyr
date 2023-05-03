/*
 * Copyright (c) 2017 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _NATIVE_POSIX_HW_MODELS_H
#define _NATIVE_POSIX_HW_MODELS_H

#include "zephyr/types.h"
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NEVER UINT64_MAX

__attribute__ ((visibility ("default"))) void hwm_one_event(void);
__attribute__ ((visibility ("default"))) void hwm_init(void);
__attribute__ ((visibility ("default"))) void hwm_cleanup(void);
__attribute__ ((visibility ("default"))) void hwm_set_end_of_time(uint64_t new_end_of_time);
__attribute__ ((visibility ("default"))) uint64_t hwm_get_time(void);
__attribute__ ((visibility ("default"))) void hwm_find_next_timer(void);

#ifdef __cplusplus
}
#endif

#endif /* _NATIVE_POSIX_HW_MODELS_H */
