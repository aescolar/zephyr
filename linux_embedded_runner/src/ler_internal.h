/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LINUX_EMBEDDED_RUNNER_LER_INTERNAL_H
#define LINUX_EMBEDDED_RUNNER_LER_INTERNAL_H

#include <stdbool.h>
#include "ler_tracing.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef unlikely
#define unlikely(x) (__builtin_expect((bool)!!(x), false) != 0L)
#endif

#define LER_SAFE_CALL(a) ler_safe_call(a, #a)

static inline void ler_safe_call(int test, const char *test_str)
{
	/* LCOV_EXCL_START */ /* See Note1 */
	if (unlikely(test)) {
		ler_print_error_and_exit("Error on: %s\n",
					   test_str);
	}
	/* LCOV_EXCL_STOP */
}

/**
 *
 * @brief find least significant bit set in a 32-bit word
 *
 * This routine finds the first bit set starting from the least significant bit
 * in the argument passed in and returns the index of that bit. Bits are
 * numbered starting at 1 from the least significant bit.  A return value of
 * zero indicates that the value passed is zero.
 *
 * @return least significant bit set, 0 if @a op is 0
 */

static inline unsigned int find_lsb_set(uint32_t op)
{
	return __builtin_ffs(op);
}

#ifdef __cplusplus
}
#endif

#endif /* LINUX_EMBEDDED_RUNNER_LER_INTERNAL_H */
