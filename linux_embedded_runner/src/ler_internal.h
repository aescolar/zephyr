/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LINUX_EMBEDDED_RUNNER_LER_INTERNAL_H
#define LINUX_EMBEDDED_RUNNER_LER_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>
#include "ler_tracing.h"
#include "ler_safe_call.h"

#ifdef __cplusplus
extern "C" {
#endif

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
