/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Lightweight heap ASAN for Zephyr sys_heap.
 *
 * 1 shadow bit per 4-byte granule (bit=1 -> poisoned).  heap.c calls
 * heap_asan_on_alloc/free at each alloc/free site.
 *
 * Instrumented code reaches __asan_store* callbacks for per-store checks and
 * __asan_memset/memcpy/memmove for bulk writes (via -Dmemset=__asan_memset).
 * Non-instrumented code (heap.c, drivers) bypasses all checks - intentional.
 */

#undef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L /* For strlcpy, stpcpy, strlcat, mempcpy,... */
#undef _GNU_SOURCE
#define _GNU_SOURCE /* For mempcpy */

#include <zephyr/kernel.h>
#include <zephyr/sys/sys_heap.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "heap.h"

#include <zephyr/sys/heap_asan.h>

#define ASAN_GRANULE         HEAP_ASAN_GRANULE
#define ASAN_SLOTS_PER_CHUNK (CHUNK_UNIT / ASAN_GRANULE)

/* Global bounds used as a fast-path filter. */
static uintptr_t __noasan asan_heap_min = UINTPTR_MAX;
static uintptr_t __noasan asan_heap_max;

/* Last-hit cache - exploits access locality. */
static struct sys_heap *__noasan asan_last_heap;

static struct {
	struct sys_heap *heap;
	uint8_t        *shadow;
} __noasan asan_reg[CONFIG_HEAP_ASAN_MAX_HEAPS];

static int __noasan asan_reg_count;

static void __noasan heap_asan_init_heap(struct sys_heap *heap);

void __noasan heap_asan_register(struct sys_heap *heap, uint8_t *shadow)
{
	if (asan_reg_count < CONFIG_HEAP_ASAN_MAX_HEAPS) {
		asan_reg[asan_reg_count].heap   = heap;
		asan_reg[asan_reg_count].shadow = shadow;
		asan_reg_count++;
	} else {
		__ASSERT_NO_MSG(false); /* increase CONFIG_HEAP_ASAN_MAX_HEAPS */
	}
}

void __noasan heap_asan_on_sys_heap_init(struct sys_heap *heap)
{
	for (int i = 0; i < asan_reg_count; i++) {
		if (asan_reg[i].heap == heap) {
			heap->asan_shadow = asan_reg[i].shadow;
			heap_asan_init_heap(heap);
			return;
		}
	}
}

static __noasan struct sys_heap *find_heap_for_addr(uintptr_t addr)
{
	if (addr < asan_heap_min || addr >= asan_heap_max) {
		return NULL;
	}

	struct sys_heap *last = asan_last_heap;

	if (last != NULL && last->asan_base != 0 &&
	    addr >= last->asan_base &&
	    (addr - last->asan_base) < last->asan_slots * ASAN_GRANULE) {
		return last;
	}

	for (int i = 0; i < asan_reg_count; i++) {
		struct sys_heap *h = asan_reg[i].heap;

		if (h->asan_base != 0 &&
		    addr >= h->asan_base &&
		    (addr - h->asan_base) < h->asan_slots * ASAN_GRANULE) {
			asan_last_heap = h;
			return h;
		}
	}
	return NULL;
}

static inline __noasan size_t addr_to_slot(struct sys_heap *heap, uintptr_t addr)
{
	return (addr - heap->asan_base) / ASAN_GRANULE;
}

/* Set (fill=0xFF) or clear (fill=0x00) bits [first_slot, first_slot+count). */
static __noasan void shadow_update(struct sys_heap *heap,
				   size_t first_slot, size_t count, uint8_t fill)
{
	size_t end_slot = first_slot + count;
	size_t i = first_slot;

	for (; i < end_slot && (i & 7u) != 0u; i++) {
		uint8_t bit = (uint8_t)(1u << (i & 7u));

		heap->asan_shadow[i >> 3] = (heap->asan_shadow[i >> 3] & ~bit) |
					    (fill & bit);
	}

	size_t full_end = end_slot & ~7u;

	if (i < full_end) {
		__builtin_memset(&heap->asan_shadow[i >> 3], fill,
				 (full_end - i) >> 3);
		i = full_end;
	}

	for (; i < end_slot; i++) {
		uint8_t bit = (uint8_t)(1u << (i & 7u));

		heap->asan_shadow[i >> 3] = (heap->asan_shadow[i >> 3] & ~bit) |
					    (fill & bit);
	}
}

static __noasan void shadow_set(struct sys_heap *heap,
				size_t first_slot, size_t count)
{
	shadow_update(heap, first_slot, count, 0xFFu);
}

static __noasan void shadow_clear(struct sys_heap *heap,
				  size_t first_slot, size_t count)
{
	shadow_update(heap, first_slot, count, 0x00u);
}

static void __noasan heap_asan_init_heap(struct sys_heap *heap)
{
	struct z_heap *h = heap->heap;

	heap->asan_base  = (uintptr_t)chunk_buf(h);
	heap->asan_slots = h->end_chunk * ASAN_SLOTS_PER_CHUNK;

	size_t shadow_bytes = (heap->asan_slots + 7) / 8;

	__builtin_memset(heap->asan_shadow, 0xFF, shadow_bytes);

	uintptr_t heap_end = heap->asan_base + heap->asan_slots * ASAN_GRANULE;

	if (heap->asan_base < asan_heap_min) {
		asan_heap_min = heap->asan_base;
	}
	if (heap_end > asan_heap_max) {
		asan_heap_max = heap_end;
	}
}

/* Poison whole chunk, then unpoison user bytes.  Padding stays poisoned. */
void __noasan heap_asan_on_alloc(struct sys_heap *heap, chunkid_t c, void *mem,
				 size_t bytes)
{
	if (heap->asan_shadow == NULL || heap->asan_slots == 0) {
		return;
	}

	struct z_heap *h = heap->heap;

	size_t total_slots     = chunk_size(h, c) * ASAN_SLOTS_PER_CHUNK;
	size_t first_slot      = c * ASAN_SLOTS_PER_CHUNK;
	size_t data_first_slot = ((uintptr_t)mem - heap->asan_base) / ASAN_GRANULE;
	size_t data_slots      = (bytes + ASAN_GRANULE - 1) / ASAN_GRANULE;

	shadow_set(heap, first_slot, total_slots);
	shadow_clear(heap, data_first_slot, data_slots);
}

void __noasan heap_asan_on_free(struct sys_heap *heap, chunkid_t c)
{
	if (heap->asan_shadow == NULL || heap->asan_slots == 0) {
		return;
	}

	shadow_set(heap, c * ASAN_SLOTS_PER_CHUNK,
		   chunk_size(heap->heap, c) * ASAN_SLOTS_PER_CHUNK);
}

void __weak __noasan heap_asan_report(uintptr_t addr, size_t size)
{
	ARG_UNUSED(addr);
	ARG_UNUSED(size);
	k_panic();
}

static __noasan void check_write_range(uintptr_t addr, size_t size)
{
	if (size == 0) {
		return;
	}

	struct sys_heap *heap = find_heap_for_addr(addr);

	if (!heap) {
		return;
	}

	uintptr_t heap_end = heap->asan_base + heap->asan_slots * ASAN_GRANULE;

	if (addr + size > heap_end) {
		printk("ASAN POISON: addr=0x%lx size=%zu (write past heap end 0x%lx)\n",
		       (unsigned long)addr, size, (unsigned long)heap_end);
		heap_asan_report(addr, size);
		return;
	}

	size_t first_slot = addr_to_slot(heap, addr);
	size_t last_slot  = addr_to_slot(heap, addr + size - 1);
	size_t first_byte = first_slot >> 3;
	size_t last_byte  = last_slot  >> 3;

	for (size_t bi = first_byte; bi <= last_byte; bi++) {
		uint8_t sb = heap->asan_shadow[bi];

		if (sb == 0u) {
			continue;
		}

		if (bi == first_byte) {
			sb &= (uint8_t)(0xFFu << (first_slot & 7u));
		}
		if (bi == last_byte) {
			sb &= (uint8_t)(0xFFu >> (7u - (last_slot & 7u)));
		}
		if (sb == 0u) {
			continue;
		}

		size_t bit = (size_t)__builtin_ctz((unsigned int)sb);
		uintptr_t viol_addr = heap->asan_base +
				      ((bi << 3) | bit) * ASAN_GRANULE;

		printk("ASAN POISON: addr=0x%lx size=%zu si=%zu "
		       "shadow_byte=0x%02x base=0x%lx\n",
		       (unsigned long)viol_addr, size, (bi << 3) | bit,
		       heap->asan_shadow[bi],
		       (unsigned long)heap->asan_base);
		heap_asan_report(viol_addr, size);
		return;
	}
}

static ALWAYS_INLINE __noasan void check_write(uintptr_t addr, size_t size)
{
	struct sys_heap *heap = find_heap_for_addr(addr);

	if (!heap) {
		return;
	}

	uintptr_t heap_end = heap->asan_base + heap->asan_slots * ASAN_GRANULE;

	if (addr + size > heap_end) {
		printk("ASAN POISON: addr=0x%lx size=%u (write past heap end 0x%lx)\n",
		       (unsigned long)addr, (unsigned)size,
		       (unsigned long)heap_end);
		heap_asan_report(addr, size);
		return;
	}

	size_t si       = addr_to_slot(heap, addr);
	size_t si_end   = addr_to_slot(heap, addr + size - 1);
	size_t n_slots  = si_end - si + 1;
	size_t byte_idx = si >> 3;
	size_t bit_off  = si & 7u;

	if (bit_off + n_slots <= 8u) {
		uint8_t mask = (uint8_t)(((1u << n_slots) - 1u) << bit_off);

		if (heap->asan_shadow[byte_idx] & mask) {
			printk("ASAN POISON: addr=0x%lx size=%u si=%u "
			       "shadow_byte=0x%02x base=0x%lx\n",
			       (unsigned long)addr, (unsigned)size, (unsigned)si,
			       heap->asan_shadow[byte_idx],
			       (unsigned long)heap->asan_base);
			heap_asan_report(addr, size);
		}
	} else {
		/* Store spans two shadow bytes. */
		size_t overflow = bit_off + n_slots - 8u;
		uint8_t mask0   = (uint8_t)(0xFFu << bit_off);
		uint8_t mask1   = (uint8_t)((1u << overflow) - 1u);

		if ((heap->asan_shadow[byte_idx] & mask0) ||
			    (heap->asan_shadow[byte_idx + 1] & mask1)) {
			printk("ASAN POISON: addr=0x%lx size=%u si=%u "
			       "shadow_byte=0x%02x base=0x%lx\n",
			       (unsigned long)addr, (unsigned)size, (unsigned)si,
			       heap->asan_shadow[byte_idx],
			       (unsigned long)heap->asan_base);
			heap_asan_report(addr, size);
		}
	}
}

/* Per-store callbacks - writes only; loads not instrumented (-asan-instrument-reads=0). */
void __noasan __asan_store1(uintptr_t addr)  { check_write(addr, 1); }
void __noasan __asan_store2(uintptr_t addr)  { check_write(addr, 2); }
void __noasan __asan_store4(uintptr_t addr)  { check_write(addr, 4); }
void __noasan __asan_store8(uintptr_t addr)  { check_write(addr, 8); }
void __noasan __asan_store16(uintptr_t addr) { check_write(addr, 16); }
void __noasan __asan_storeN(uintptr_t addr, size_t size) { check_write_range(addr, size); }

/* _noabort variants: -fsanitize=kernel-address emits these in outline mode. */
void __noasan __asan_store1_noabort(uintptr_t addr)  { check_write(addr, 1); }
void __noasan __asan_store2_noabort(uintptr_t addr)  { check_write(addr, 2); }
void __noasan __asan_store4_noabort(uintptr_t addr)  { check_write(addr, 4); }
void __noasan __asan_store8_noabort(uintptr_t addr)  { check_write(addr, 8); }
void __noasan __asan_store16_noabort(uintptr_t addr) { check_write(addr, 16); }
void __noasan __asan_storeN_noabort(uintptr_t addr, size_t size) { check_write_range(addr, size); }

void __noasan __asan_init(void) { }
/* Stubs for GCC ASAN ABI version checks - provide v6/v7/v8 to cover all GCC releases. */
void __noasan __asan_version_mismatch_check_v6(void) { }
void __noasan __asan_version_mismatch_check_v7(void) { }
void __noasan __asan_version_mismatch_check_v8(void) { }
void __noasan __asan_handle_no_return(void) { }

/*
 * Bulk-write interceptors.  Instrumented code reaches these via
 * -Dmemset=__asan_memset (both Clang and GCC); __builtin_* inside these
 * __noasan functions generates plain stores with no further checking.
 */
void __noasan *__asan_memset(void *s, int c, size_t n)
{
	check_write_range((uintptr_t)s, n);
	return __builtin_memset(s, c, n);
}

void __noasan *__asan_memcpy(void *dst, const void *src, size_t n)
{
	check_write_range((uintptr_t)dst, n);
	return __builtin_memcpy(dst, src, n);
}

void __noasan *__asan_memmove(void *dst, const void *src, size_t n)
{
	check_write_range((uintptr_t)dst, n);
	return __builtin_memmove(dst, src, n);
}

/* memccpy: copies up to n bytes stopping after first occurrence of c.
 * Conservatively checks n bytes (actual write may be less if c is found early). */
#if defined(CONFIG_HEAP_ASAN_EXTENSIONS)
void __noasan *__asan_memccpy(void *dst, const void *src, int c, size_t n)
{
	check_write_range((uintptr_t)dst, n);
	return memccpy(dst, src, c, n);
}

void __noasan *__asan_mempcpy(void *dst, const void *src, size_t n)
{
	check_write_range((uintptr_t)dst, n);
	__builtin_memcpy(dst, src, n);
	return (uint8_t *)dst + n;
}

char __noasan *__asan_fgets(char *buf, int size, FILE *stream)
{
	if (size > 0) {
		check_write_range((uintptr_t)buf, (size_t)size);
	}
	return fgets(buf, size, stream);
}
#endif /* CONFIG_HEAP_ASAN_EXTENSIONS */

/* Stack-ASAN stubs - we only track heap memory. */
void __noasan __asan_alloca_poison(uintptr_t addr, size_t size)
{
	ARG_UNUSED(addr);
	ARG_UNUSED(size);
}

void __noasan __asan_allocas_unpoison(uintptr_t top, uintptr_t bottom)
{
	ARG_UNUSED(top);
	ARG_UNUSED(bottom);
}

/*
 * String and printf bulk-write interceptors.
 * heap_asan.c is compiled without the -D redirects, so calls below go to the
 * real library implementations.
 */

char __noasan *__asan_strcpy(char *dst, const char *src)
{
	check_write_range((uintptr_t)dst, strlen(src) + 1);
	return strcpy(dst, src);
}

#if defined(CONFIG_HEAP_ASAN_EXTENSIONS)
char __noasan *__asan_stpcpy(char *dst, const char *src)
{
	check_write_range((uintptr_t)dst, strlen(src) + 1);
	return stpcpy(dst, src);
}

char __noasan *__asan_stpncpy(char *dst, const char *src, size_t n)
{
	check_write_range((uintptr_t)dst, n);
	return stpncpy(dst, src, n);
}
#endif /* CONFIG_HEAP_ASAN_EXTENSIONS */

char __noasan *__asan_strncpy(char *dst, const char *src, size_t n)
{
	check_write_range((uintptr_t)dst, n);
	return strncpy(dst, src, n);
}

/* strcat: appends starting at end of dst; check only the appended region. */
char __noasan *__asan_strcat(char *dst, const char *src)
{
	check_write_range((uintptr_t)(dst + strlen(dst)), strlen(src) + 1);
	return strcat(dst, src);
}

char __noasan *__asan_strncat(char *dst, const char *src, size_t n)
{
	check_write_range((uintptr_t)(dst + strlen(dst)), n + 1);
	return strncat(dst, src, n);
}

#if defined(CONFIG_HEAP_ASAN_EXTENSIONS)
/* strlcpy/strlcat: bounded variants; check the full declared capacity. */
size_t __noasan __asan_strlcpy(char *dst, const char *src, size_t siz)
{
	check_write_range((uintptr_t)dst, siz);
	return strlcpy(dst, src, siz);
}

size_t __noasan __asan_strlcat(char *dst, const char *src, size_t siz)
{
	check_write_range((uintptr_t)dst, siz);
	return strlcat(dst, src, siz);
}
#endif /* defined(CONFIG_HEAP_ASAN_EXTENSIONS) */

int __noasan __asan_vsnprintf(char *dst, size_t n, const char *fmt, va_list ap)
{
	check_write_range((uintptr_t)dst, n);
	return vsnprintf(dst, n, fmt, ap);
}

int __noasan __asan_snprintf(char *dst, size_t n, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	int ret = __asan_vsnprintf(dst, n, fmt, ap);

	va_end(ap);
	return ret;
}

/*
 * sprintf/vsprintf: write size unknown at call time - compute it first with a
 * vsnprintf(NULL, 0, ...) dry run, which returns the would-be byte count.
 */
int __noasan __asan_vsprintf(char *dst, const char *fmt, va_list ap)
{
	va_list ap2;

	va_copy(ap2, ap);
	int needed = vsnprintf(NULL, 0, fmt, ap2);

	va_end(ap2);
	if (needed >= 0) {
		check_write_range((uintptr_t)dst, (size_t)needed + 1);
	}
	return vsprintf(dst, fmt, ap);
}

int __noasan __asan_sprintf(char *dst, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	int ret = __asan_vsprintf(dst, fmt, ap);

	va_end(ap);
	return ret;
}
