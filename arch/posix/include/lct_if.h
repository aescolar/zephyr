/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_ARCH_POSIX_BOTTOM_IF_H
#define ZEPHYR_ARCH_POSIX_BOTTOM_IF_H

#ifdef __cplusplus
extern "C" {
#endif

/* Interface provided by the bottom */
void *lct_init(void);
void lct_clean_up(void *this);
void lct_posix_swap(void *this, int next_allowed_thread_nbr, int this_th_nbr);
void lct_main_thread_start(void *this, int next_allowed_thread_nbr);
int lct_new_thread(void *this, void *payload);
void lct_abort_thread(void *this, int thread_idx, int self);
int lct_get_unique_thread_id(void *this, int thread_idx);

/* Interface expected by the bottom */
//TODO: move to a registered callback (so we support multiCPU)
void posix_arch_thread_entry(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_ARCH_POSIX_BOTTOM_IF_H */
