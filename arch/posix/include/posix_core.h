/*
 * Copyright (c) 2017 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_ARCH_POSIX_INCLUDE_POSIX_CORE_H_
#define ZEPHYR_ARCH_POSIX_INCLUDE_POSIX_CORE_H_

#include <zephyr/kernel.h>
#include "bottom_if.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	k_thread_entry_t entry_point;
	void *arg1;
	void *arg2;
	void *arg3;

	int thread_idx;

#if defined(CONFIG_ARCH_HAS_THREAD_ABORT)
	/* The kernel may indicate that a thread has been aborted several */
	/* times */
	int aborted;
#endif

	/*
	 * Note: If more elements are added to this structure, remember to
	 * update ARCH_POSIX_RECOMMENDED_STACK_SIZE in the configuration.
	 *
	 * Currently there are 4 pointers + 2 ints, on a 32-bit native posix
	 * implementation this will result in 24 bytes ( 4*4 + 2*4).
	 * For a 64-bit implementation the recommended stack size will be
	 * 40 bytes ( 4*8 + 2*4 ).
	 */
} posix_thread_status_t;


void posix_irq_check_idle_exit(void);

#if POSIX_ARCH_DEBUG_PRINTS
#define PC_DEBUG(fmt, ...) posix_print_trace(PREFIX fmt, __VA_ARGS__)
#else
#define PC_DEBUG(...)
#endif

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_ARCH_POSIX_INCLUDE_POSIX_CORE_H_ */
