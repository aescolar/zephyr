/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lct_if.h"

static void* ts_state;

void posix_arch_init(void)
{
	extern void posix_arch_thread_entry(void *pa_thread_status);
	ts_state = lct_init(posix_arch_thread_entry);
}

void posix_arch_clean_up(void)
{
	lct_clean_up(ts_state);
}

void posix_swap(int next_allowed_thread_nbr, int this_th_nbr)
{
	lct_posix_swap(ts_state, next_allowed_thread_nbr, this_th_nbr);
}

void posix_main_thread_start(int next_allowed_thread_nbr)
{
	lct_main_thread_start(ts_state, next_allowed_thread_nbr);
}

int posix_new_thread(void *payload)
{
	return lct_new_thread(ts_state, payload);
}

void posix_abort_thread(int thread_idx, int self)
{
	lct_abort_thread(ts_state, thread_idx, self);
}
