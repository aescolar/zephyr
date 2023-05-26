/*
 * Copyright (c) 2017 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NSI_COMMON_SRC_INCL_HW_MODELS_TOP_H
#define NSI_COMMON_SRC_INCL_HW_MODELS_TOP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NEVER UINT64_MAX

//TODO: rename to nsi_hws_ (HW Scheduler)

void hwm_one_event(void);
void hwm_init(void);
void hwm_cleanup(void);
void hwm_set_end_of_time(uint64_t new_end_of_time);
uint64_t hwm_get_time(void);
void hwm_find_next_timer(void);

#ifdef __cplusplus
}
#endif

#endif /* NSI_COMMON_SRC_INCL_HW_MODELS_TOP_H */
