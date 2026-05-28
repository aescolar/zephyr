/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_SYS_HEAP_ASAN_H_
#define ZEPHYR_INCLUDE_SYS_HEAP_ASAN_H_

#include <stdint.h>
#include <stddef.h>
#include <zephyr/sys/sys_heap.h>
#include <zephyr/toolchain.h>
#include <zephyr/init.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_HEAP_ASAN

/* 1 shadow bit per HEAP_ASAN_GRANULE bytes; HEAP_ASAN_SHADOW_RATIO heap bytes per shadow byte. */
#define HEAP_ASAN_GRANULE       4U
#define HEAP_ASAN_SHADOW_RATIO  (HEAP_ASAN_GRANULE * 8U)

/* Number of shadow bytes required for a heap of _sz bytes. */
#define HEAP_ASAN_SHADOW_SIZE(_sz)  \
	(((_sz) + HEAP_ASAN_SHADOW_RATIO - 1) / HEAP_ASAN_SHADOW_RATIO)

/**
 * @brief Report a heap ASAN violation.
 *
 * Called when a write to a poisoned address is detected.  The weak default
 * calls k_panic().  Override in test code to intercept via setjmp/longjmp.
 *
 * @param addr  Violating address.
 * @param size  Write size in bytes.
 */
void heap_asan_report(uintptr_t addr, size_t size);

/**
 * @brief Register a heap for ASAN shadow tracking.
 *
 * @param heap    Pointer to the sys_heap struct.
 * @param shadow  Pre-allocated shadow buffer (from __noinit section).
 */
void heap_asan_register(struct sys_heap *heap, uint8_t *shadow);

/**
 * @brief Enable ASAN shadow tracking on a struct sys_heap.
 *
 * Place at file scope after the sys_heap declaration.
 * @param _heap_name  sys_heap variable name.
 * @param _heap_sz    Heap buffer size in bytes (compile-time constant).
 */
#define SYS_HEAP_ASAN_ENABLE(_heap_name, _heap_sz)                                            \
	static uint8_t _heap_name##_asan_sh[HEAP_ASAN_SHADOW_SIZE(_heap_sz)] __noinit;       \
	static int _heap_name##_asan_init_fn(void)                                            \
	{                                                                                      \
		heap_asan_register(&(_heap_name), _heap_name##_asan_sh);                          \
		return 0;                                                                          \
	}                                                                                      \
	SYS_INIT(_heap_name##_asan_init_fn, PRE_KERNEL_1, 0)

/**
 * @brief Enable ASAN shadow tracking on a struct k_heap (K_HEAP_DEFINE).
 *
 * Place at file scope after the K_HEAP_DEFINE line.
 * @param _heap_name  k_heap variable name.
 * @param _heap_sz    Same size passed to K_HEAP_DEFINE (compile-time constant).
 */
#define K_HEAP_ASAN_ENABLE(_heap_name, _heap_sz)                                              \
	static uint8_t _heap_name##_asan_sh[HEAP_ASAN_SHADOW_SIZE(_heap_sz)] __noinit;       \
	static int _heap_name##_asan_init_fn(void)                                            \
	{                                                                                      \
		heap_asan_register(&(_heap_name).heap, _heap_name##_asan_sh);                     \
		return 0;                                                                          \
	}                                                                                      \
	SYS_INIT(_heap_name##_asan_init_fn, PRE_KERNEL_1, 0)

#else /* !CONFIG_HEAP_ASAN */

#define SYS_HEAP_ASAN_ENABLE(_heap_name, _heap_sz)  /* no-op */
#define K_HEAP_ASAN_ENABLE(_heap_name, _heap_sz)    /* no-op */

#endif /* CONFIG_HEAP_ASAN */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_SYS_HEAP_ASAN_H_ */
