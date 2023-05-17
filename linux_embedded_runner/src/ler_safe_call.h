/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LINUX_EMBEDDED_RUNNER_LER_SAFE_CALL_H
#define LINUX_EMBEDDED_RUNNER_LER_SAFE_CALL_H

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

#ifdef __cplusplus
}
#endif

#endif /* LINUX_EMBEDDED_RUNNER_LER_SAFE_CALL_H */
