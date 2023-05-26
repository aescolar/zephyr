/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef NSI_COMMON_SRC_INCL_NCT_IF_H
#define NSI_COMMON_SRC_INCL_NCT_IF_H

#ifdef __cplusplus
extern "C" {
#endif

/* Interface provided by the Native simulator CPU threading emulation */
void *nct_init(void (*fptr)(void *));
void nct_clean_up(void *this);
void nct_swap_threads(void *this, int next_allowed_thread_nbr, int this_th_nbr);
void nct_first_thread_start(void *this, int next_allowed_thread_nbr);
int nct_new_thread(void *this, void *payload);
void nct_abort_thread(void *this, int thread_idx, int self);
int nct_get_unique_thread_id(void *this, int thread_idx);

#ifdef __cplusplus
}
#endif

#endif /* NSI_COMMON_SRC_INCL_NCT_IF_H */
