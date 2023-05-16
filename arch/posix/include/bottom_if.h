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
void posix_swap(int next_allowed_thread_nbr, int this_thread_nbr);
void posix_main_thread_start(int next_allowed_thread_nbr);
int posix_new_thread(void *ptr);
void posix_init_multithreading(void);
void posix_core_clean_up(void);
void posix_abort_thread(int thread_idx, int self);
int posix_get_unique_thread_id(int thread_idx);

/* Interface expected by the bottom */
void posix_new_thread_pre_start(void);
void posix_arch_thread_entry(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_ARCH_POSIX_BOTTOM_IF_H */
