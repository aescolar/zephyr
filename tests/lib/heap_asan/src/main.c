/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/sys_heap.h>
#include <zephyr/sys/heap_asan.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>

extern size_t strlcpy(char *dst, const char *src, size_t siz);
extern size_t strlcat(char *dst, const char *src, size_t siz);
extern void *mempcpy(void *dst, const void *src, size_t n);


static jmp_buf   g_panic_jmp;
static volatile bool g_expect_panic;

/*
 * Override the weak heap_asan_report() to intercept ASAN violations via
 * longjmp before k_panic() and Zephyr's fatal-error machinery are entered.
 */
void __noasan heap_asan_report(uintptr_t addr, size_t size)
{
	ARG_UNUSED(addr);
	ARG_UNUSED(size);

	if (g_expect_panic) {
		g_expect_panic = false;
		longjmp(g_panic_jmp, 1);
	}
	TC_PRINT("UNEXPECTED ASAN fault addr=0x%lx size=%zu — halting\n",
		 (unsigned long)addr, size);
	for (;;) {
		k_sleep(K_FOREVER);
	}
}

#define EXPECT_PANIC(body)                        \
	({                                        \
		g_expect_panic = true;            \
		bool _fired = false;              \
		if (setjmp(g_panic_jmp) == 0) {   \
			body;                     \
		} else {                          \
			_fired = true;            \
		}                                 \
		g_expect_panic = false;           \
		_fired;                           \
	})



__attribute__((noinline))
void do_write(volatile uint8_t *p, size_t off, uint8_t val)
{
	p[off] = val;
}

/* Eight-byte store → __asan_store8 (spans two shadow bytes when misaligned).
 * Use aligned(1) so ARM32 strict-alignment targets don't fault on unaligned
 * accesses; the compiler still emits __asan_store8 because the type width is 8.
 */
__attribute__((noinline))
void do_write8(volatile uint8_t *p, size_t off, uint64_t val)
{
	typedef uint64_t __attribute__((aligned(1))) u64_unaligned_t;

	*(volatile u64_unaligned_t *)(p + off) = val;
}

static const uint8_t g_src_bytes[64];

__attribute__((noinline))
static void do_memset(void *p, int c, size_t n)  { memset(p, c, n); }

__attribute__((noinline))
static void do_memcpy(void *dst, const void *src, size_t n) { memcpy(dst, src, n); }

__attribute__((noinline))
static void do_memmove(void *dst, const void *src, size_t n) { memmove(dst, src, n); }


static const char g_str_overflow[]        = "ABCDEFGHIJKLMNOPQ"; /* 17 chars */
static const char g_str_exact[]           = "ABCDEFGHIJKLMNO";   /* 15 chars */
static const char g_str_half[]            = "abcdefgh";          /*  8 chars */
static const char g_str_append_overflow[] = "IJKLMNOPQ";         /*  9 chars */
static const char g_str_append_exact[]    = "IJKLMNO";           /*  7 chars */

__attribute__((noinline))
static void do_strcpy(char *dst, const char *src)  { strcpy(dst, src); }

__attribute__((noinline))
static void do_strncpy(char *dst, const char *src, size_t n) { strncpy(dst, src, n); }

__attribute__((noinline))
static void do_strcat(char *dst, const char *src)  { strcat(dst, src); }

__attribute__((noinline))
static void do_strncat(char *dst, const char *src, size_t n) { strncat(dst, src, n); }

__attribute__((noinline))
static void do_strlcpy(char *dst, const char *src, size_t siz) { strlcpy(dst, src, siz); }

__attribute__((noinline))
static void do_strlcat(char *dst, const char *src, size_t siz) { strlcat(dst, src, siz); }

__attribute__((noinline))
static void do_sprintf(char *dst, const char *src) { sprintf(dst, "%s", src); }

__attribute__((noinline))
static void do_snprintf(char *dst, size_t n, const char *src) { snprintf(dst, n, "%s", src); }

__attribute__((noinline))
static char *do_stpcpy(char *dst, const char *src)  { return stpcpy(dst, src); }

__attribute__((noinline))
static char *do_stpncpy(char *dst, const char *src, size_t n) { return stpncpy(dst, src, n); }

__attribute__((noinline))
static void *do_memccpy(void *dst, const void *src, int c, size_t n)
{
	return memccpy(dst, src, c, n);
}

__attribute__((noinline))
static void *do_mempcpy(void *dst, const void *src, size_t n) { return mempcpy(dst, src, n); }

__attribute__((noinline))
static char *do_fgets(char *buf, int size, FILE *stream) { return fgets(buf, size, stream); }


struct heap_ops {
	void *(*alloc)(size_t bytes);
	void  (*free)(void *ptr);
	void *(*realloc)(void *ptr, size_t bytes);
	int    underflow_off;
};

struct asan_malloc_fixture   { struct heap_ops ops; };
struct asan_sys_heap_fixture { struct heap_ops ops; };

static void *w_malloc(size_t b)           { return malloc(b); }
static void  w_free(void *p)              { free(p); }
static void *w_realloc(void *p, size_t b) { return realloc(p, b); }

static void *w_k_malloc(size_t b)           { return k_malloc(b); }
static void  w_k_free(void *p)              { k_free(p); }
static void *w_k_realloc(void *p, size_t b) { return k_realloc(p, b); }

static void *malloc_setup(void)
{
	static struct asan_malloc_fixture f = {
		.ops = { .alloc = w_malloc, .free = w_free,
			 .realloc = w_realloc, .underflow_off = -1 },
	};
	return &f;
}

static void *sys_heap_setup(void)
{
	/*
	 * k_malloc() prepends a sizeof(struct k_heap *) pointer before the
	 * returned user address, so the first poisoned byte is further back:
	 *   32-bit: underflow_off = -(4+1) = -5
	 *   64-bit: underflow_off = -(8+1) = -9
	 */
	static struct asan_sys_heap_fixture f = {
		.ops = { .alloc = w_k_malloc, .free = w_k_free,
			 .realloc = w_k_realloc,
			 .underflow_off = -(int)(sizeof(struct k_heap *) + 1) },
	};
	return &f;
}


static void run_normal(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	for (int i = 0; i < 16; i++) {
		do_write(p, i, (uint8_t)i);
	}
	ops->free(p);
}


static void run_overflow(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC(do_write(p, 16, 0xFF));

	ops->free(p);
	zassert_true(fired, "1-byte overflow not detected");
}

static void run_overflow_no_false_positive(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC(do_write(p, 15, 0xFE));

	ops->free(p);
	zassert_false(fired, "false positive at last valid byte p[size-1]");
}

/* 8-byte store spans two shadow bytes — tests the two-byte mask path. */
static void run_overflow_8byte(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC(do_write8(p, 16, 0xDEADC0DEULL));

	ops->free(p);
	zassert_true(fired, "8-byte overflow not detected");
}

static void run_overflow_8byte_no_false_positive(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC(do_write8(p, 8, 0xCAFEBABEULL));

	ops->free(p);
	zassert_false(fired, "false positive: 8-byte store at last valid position");
}

/*
 * 8-byte write starting at the 8th granule of an allocation (byte offset 28).
 * When the allocation's data_first_slot is aligned to a shadow-byte boundary
 * (data_first_slot & 7 == 0), si & 7 == 7 and the store spans two shadow
 * bytes, exercising the else-branch mask logic in check_write.
 * Overflow: alloc(32) — bytes 28-35, bytes 32-35 are in the poisoned header.
 */
static void run_overflow_8byte_at_slot7(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(32);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC(do_write8(p, 28, 0xDEADBEEFULL));

	ops->free(p);
	zassert_true(fired,
		     "8-byte overflow at slot-7 offset not detected "
		     "(possible two-shadow-byte path)");
}

/* No-FP counterpart: alloc(64) keeps bytes 28-35 within the allocation. */
static void run_overflow_8byte_at_slot7_no_false_positive(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(64);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC(do_write8(p, 28, 0xCAFEBABEULL));

	ops->free(p);
	zassert_false(fired,
		      "false positive: 8-byte store at offset 28 in 64-byte alloc");
}

/*
 * do_write8(p, 3, ...) writes p[3..10], spanning granules 0, 1 (accessible)
 * and 2 (poisoned).  n_slots must be computed as si_end-si+1, not size/4,
 * to account for the intra-granule offset.
 */
static void run_overflow_misaligned_granule_span(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(8);

	zassert_not_null(p, "alloc(8) failed");
	bool fired = EXPECT_PANIC(do_write8(p, 3, 0xDEADC0DEULL));

	ops->free(p);
	zassert_true(fired,
		     "overflow at misaligned granule boundary not detected "
		     "(check_write n_slots must account for intra-granule offset)");
}

/* No-FP: alloc(16) opens granules 0-3, so p[3..10] is fully accessible. */
static void run_overflow_misaligned_granule_span_no_false_positive(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(16);

	zassert_not_null(p, "alloc(16) failed");
	bool fired = EXPECT_PANIC(do_write8(p, 3, 0xCAFEBABEULL));

	ops->free(p);
	zassert_false(fired,
		      "false positive: misaligned 8-byte store p[3..10] in alloc(16) "
		      "must not trigger ASAN");
}


static void run_underflow(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	volatile uint8_t *vp = p;
	bool fired = EXPECT_PANIC(do_write(vp + ops->underflow_off, 0, 0xDD));

	ops->free(p);
	zassert_true(fired, "underflow not detected");
}


static void run_use_after_free(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	ops->free(p);
	bool fired = EXPECT_PANIC(do_write(p, 0, 0xBB));

	zassert_true(fired, "use-after-free not detected");
}

static void run_use_after_free_memset(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	ops->free(p);
	bool fired = EXPECT_PANIC(do_memset(p, 0, 1));

	zassert_true(fired, "memset use-after-free not detected");
}

static void run_use_after_free_memcpy(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	ops->free(p);
	bool fired = EXPECT_PANIC(do_memcpy(p, g_src_bytes, 1));

	zassert_true(fired, "memcpy use-after-free not detected");
}

/* UAF via interior pointer: free() must poison the entire chunk, not just byte 0. */
static void run_use_after_free_interior_ptr(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");

	bool pre_free = EXPECT_PANIC(do_write(p, 12, 0xAA));

	ops->free(p);

	bool fired = EXPECT_PANIC(do_write(p, 12, 0xDE));

	zassert_false(pre_free,
		      "false positive: p[12] must be accessible in live 16-byte alloc");
	zassert_true(fired,
		     "UAF via interior pointer not detected: "
		     "full chunk must be poisoned on free, not just byte 0");
}

/*
 * realloc(p, 1): q[3] (granule 0 tail) is accessible — 1:32 blind spot;
 * q[4] (granule 1) must be poisoned by the shrink.
 */
static void run_realloc_shrink_to_one_granule_boundary(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(64);

	zassert_not_null(p, "alloc(64) failed");

	uint8_t *q = ops->realloc(p, 1);

	zassert_not_null(q, "realloc(p, 1) failed");

	bool no_fp = EXPECT_PANIC(do_write(q, 3, 0xAA));
	bool fired  = EXPECT_PANIC(do_write(q, 4, 0xBB));

	ops->free(q);

	zassert_false(no_fp,
		      "false positive: q[3] (granule 0 tail) must be accessible "
		      "after realloc(p, 1) — 1:32 granule covers bytes 0-3");
	zassert_true(fired,
		     "overflow not detected: q[4] must be poisoned "
		     "after realloc shrinks to 1 byte");
}


static void run_realloc_shrink_suffix(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(64);

	zassert_not_null(p, "alloc failed");
	uint8_t *q = ops->realloc(p, 16);

	zassert_not_null(q, "realloc failed");
	bool fired = EXPECT_PANIC(do_write(q, 16, 0xCC));

	ops->free(q);
	zassert_true(fired, "realloc shrink: suffix not poisoned");
}

static void run_realloc_shrink_no_false_positive(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(64);

	zassert_not_null(p, "alloc failed");
	uint8_t *q = ops->realloc(p, 16);

	zassert_not_null(q, "realloc failed");
	bool fired = EXPECT_PANIC({
		for (int i = 0; i < 16; i++) {
			do_write(q, i, (uint8_t)i);
		}
	});

	ops->free(q);
	zassert_false(fired, "false positive after realloc shrink");
}

static void run_realloc_expand(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	uint8_t *q = ops->realloc(p, 64);

	zassert_not_null(q, "realloc expand failed");
	bool fired = EXPECT_PANIC(do_write(q, 64, 0xEE));

	ops->free(q);
	zassert_true(fired, "realloc expand: overflow at new end not detected");
}

static void run_realloc_expand_no_false_positive(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	uint8_t *q = ops->realloc(p, 64);

	zassert_not_null(q, "realloc expand failed");
	bool fired = EXPECT_PANIC({
		for (int i = 0; i < 64; i++) {
			do_write(q, i, (uint8_t)i);
		}
	});

	ops->free(q);
	zassert_false(fired, "false positive in expanded region after realloc");
}

static void run_realloc_free_use_after(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	void *q = ops->realloc(p, 0);

	zassert_is_null(q, "realloc(p,0) should return NULL");
	bool fired = EXPECT_PANIC(do_write(p, 0, 0xBB));

	zassert_true(fired, "realloc(p,0) use-after-free not detected");
}

/*
 * A blocker forces realloc to move the block; the stale pointer to the old
 * address must then be detected.  Skip if realloc expands in-place (q == p).
 */
static void run_use_after_realloc_moved(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(8);
	uint8_t *blocker = ops->alloc(8);

	zassert_not_null(p,       "alloc p failed");
	zassert_not_null(blocker, "alloc blocker failed");

	uint8_t *q = ops->realloc(p, 256);

	zassert_not_null(q, "realloc(p, 256) failed");

	if (q == p) {
		ops->free(q);
		ops->free(blocker);
		ztest_test_skip();
		return;
	}

	bool fired = EXPECT_PANIC(do_write(p, 0, 0xDE));

	bool no_fp = EXPECT_PANIC({
		for (int i = 0; i < 256; i++) {
			do_write(q, i, (uint8_t)i);
		}
	});

	ops->free(q);
	ops->free(blocker);

	zassert_true(fired,
		     "use-after-realloc (moved block) not detected: "
		     "old chunk must be poisoned when realloc moves data");
	zassert_false(no_fp,
		      "false positive in new allocation after realloc move");
}


static void run_memset_overflow(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC(do_memset(p, 0xAA, 17));

	ops->free(p);
	zassert_true(fired, "memset overflow not detected");
}

static void run_memset_exact(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC(do_memset(p, 0, 16));

	ops->free(p);
	zassert_false(fired, "false positive on exact-size memset");
}

static void run_memcpy_overflow(struct heap_ops *ops)
{
	uint8_t *dst = ops->alloc(16);

	zassert_not_null(dst, "alloc failed");
	bool fired = EXPECT_PANIC(do_memcpy(dst, g_src_bytes, 17));

	ops->free(dst);
	zassert_true(fired, "memcpy overflow not detected");
}

static void run_memcpy_exact(struct heap_ops *ops)
{
	uint8_t *dst = ops->alloc(16);

	zassert_not_null(dst, "alloc failed");
	bool fired = EXPECT_PANIC(do_memcpy(dst, g_src_bytes, 16));

	ops->free(dst);
	zassert_false(fired, "false positive on exact-size memcpy");
}

static void run_memmove_overflow(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC(do_memmove(p, g_src_bytes, 17));

	ops->free(p);
	zassert_true(fired, "memmove overflow not detected");
}

static void run_memmove_exact(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC(do_memmove(p, g_src_bytes, 16));

	ops->free(p);
	zassert_false(fired, "false positive on exact-size memmove");
}

/*
 * Write starts at p+8, spans into the poisoned trailer — exercises the
 * non-trivial first-slot mask: sb &= 0xFF << (first_slot & 7).
 */
static void run_memset_mid_overflow(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC(do_memset(p + 8, 0xA5, 9));

	ops->free(p);
	zassert_true(fired, "memset mid-overflow not detected");
}

/*
 * Write exactly the second half of a 16-byte allocation: p[8..15].
 * Both first_slot and last_slot masks in check_write_range are non-trivial
 * here; a wrong shift in either produces a false positive.
 */
static void run_memset_mid_exact(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC(do_memset(p + 8, 0xC3, 8));

	ops->free(p);
	zassert_false(fired,
		      "false positive: memset(p+8, c, 8) in 16-byte alloc must not "
		      "trigger ASAN (non-trivial first/last slot masking)");
}


/* alloc(1): data_slots=1, so p[0]..p[3] accessible; p[4] poisoned. */
static void run_granule_single_slot(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(1);

	zassert_not_null(p, "alloc(1) failed");
	bool no_fp = EXPECT_PANIC(do_write(p, 3, 0xA1));
	bool fired  = EXPECT_PANIC(do_write(p, 4, 0xA2));

	ops->free(p);
	zassert_false(no_fp, "false positive: p[3] must be accessible in 1-byte alloc");
	zassert_true(fired,  "missed overflow: p[4] must be poisoned in 1-byte alloc");
}

/* alloc(5): data_slots=ceil(5/4)=2, so p[0]..p[7] accessible; p[8] poisoned. */
static void run_granule_mid_slot(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(5);

	zassert_not_null(p, "alloc(5) failed");
	bool no_fp = EXPECT_PANIC(do_write(p, 7, 0xB1));
	bool fired  = EXPECT_PANIC(do_write(p, 8, 0xB2));

	ops->free(p);
	zassert_false(no_fp, "false positive: p[7] must be accessible in 5-byte alloc");
	zassert_true(fired,  "missed overflow: p[8] must be poisoned in 5-byte alloc");
}

/* check_write_range early-return for size==0 must not be removed. */
static void run_bulk_zero_size(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	bool f1 = EXPECT_PANIC(do_memset(p, 0, 0));
	bool f2 = EXPECT_PANIC(do_memcpy(p, g_src_bytes, 0));
	bool f3 = EXPECT_PANIC(do_memmove(p, g_src_bytes, 0));

	ops->free(p);
	zassert_false(f1 || f2 || f3,
		      "false positive: zero-size bulk write must not trigger ASAN");
}

/* alloc(64) exercises the full-byte path in shadow_clear. */
static void run_large_alloc_boundary(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(64);

	zassert_not_null(p, "alloc(64) failed");
	bool no_fp = EXPECT_PANIC({
		for (int i = 0; i < 64; i++) {
			do_write(p, i, (uint8_t)i);
		}
	});
	bool fired = EXPECT_PANIC(do_write(p, 64, 0xEF));

	ops->free(p);
	zassert_false(no_fp, "false positive in 64-byte allocation");
	zassert_true(fired,  "overflow at byte 64 not detected");
}

/* Write past block A while block B is live — inter-chunk region must be poisoned. */
static void run_adjacent_allocs_overflow(struct heap_ops *ops)
{
	uint8_t *a = ops->alloc(16);
	uint8_t *b = ops->alloc(16);

	zassert_not_null(a, "alloc a failed");
	zassert_not_null(b, "alloc b failed");
	bool fired = EXPECT_PANIC(do_write(a, 16, 0xFF));

	ops->free(a);
	ops->free(b);
	zassert_true(fired, "overflow past first of two adjacent allocs not detected");
}

/*
 * Two live allocations A and B: valid writes to both must not trigger ASAN.
 * Guards against shadow interference between adjacent blocks.
 */
static void run_adjacent_allocs_no_false_positive(struct heap_ops *ops)
{
	uint8_t *a = ops->alloc(16);
	uint8_t *b = ops->alloc(16);

	zassert_not_null(a, "alloc a failed");
	zassert_not_null(b, "alloc b failed");
	bool fired = EXPECT_PANIC({
		for (int i = 0; i < 16; i++) {
			do_write(a, i, (uint8_t)i);
			do_write(b, i, (uint8_t)(i + 16));
		}
	});

	ops->free(a);
	ops->free(b);
	zassert_false(fired,
		      "false positive: valid writes to two adjacent allocations");
}

/*
 * memcpy FROM heap TO stack: dst is a stack buffer, src is heap.
 * ASAN checks only the write destination; a stack dst must never fire.
 */
static void run_memcpy_heap_src_stack_dst_no_false_positive(struct heap_ops *ops)
{
	uint8_t *heap_src = ops->alloc(16);

	zassert_not_null(heap_src, "alloc failed");
	/* write known data into the heap buffer first */
	do_memset(heap_src, 0xAB, 16);

	uint8_t stack_dst[16];
	bool fired = EXPECT_PANIC(do_memcpy(stack_dst, heap_src, 16));

	ops->free(heap_src);
	zassert_false(fired,
		      "false positive: memcpy with stack dst must not trigger ASAN");
}

/* Freed chunk must be poisoned; re-alloc of the same chunk must unpoison it. */
static void run_use_after_free_then_realloc(struct heap_ops *ops)
{
	uint8_t *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	ops->free(p);
	bool uaf = EXPECT_PANIC(do_write(p, 0, 0xAA));

	uint8_t *q = ops->alloc(16);

	zassert_not_null(q, "re-alloc failed");
	bool no_fp = EXPECT_PANIC({
		for (int i = 0; i < 16; i++) {
			do_write(q, i, (uint8_t)i);
		}
	});

	ops->free(q);
	zassert_true(uaf,   "use-after-free not detected before re-alloc");
	zassert_false(no_fp, "false positive: re-allocated memory must be accessible");
}

/* Writes to stack and global memory must not trigger ASAN. */
static uint8_t g_non_heap_buf[32];

static void run_non_heap_no_false_positive(struct heap_ops *ops)
{
	ARG_UNUSED(ops);
	uint8_t stack_buf[32];
	volatile uint8_t *vs = stack_buf;
	volatile uint8_t *vg = g_non_heap_buf;

	bool fired = EXPECT_PANIC({
		for (int i = 0; i < 32; i++) {
			vs[i] = (uint8_t)i;
			vg[i] = (uint8_t)i;
		}
	});

	zassert_false(fired,
		      "false positive: stack/global writes must not trigger ASAN");
}


static void run_strcpy_overflow(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC(do_strcpy(p, g_str_overflow));

	ops->free(p);
	zassert_true(fired, "strcpy overflow not detected (18 bytes into 16-byte buffer)");
}

static void run_strcpy_exact(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC(do_strcpy(p, g_str_exact));

	ops->free(p);
	zassert_false(fired, "false positive on exact strcpy (15 chars + null = 16 bytes)");
}

static void run_strcpy_use_after_free(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	ops->free(p);
	bool fired = EXPECT_PANIC(do_strcpy(p, g_str_exact));

	zassert_true(fired, "strcpy use-after-free not detected");
}

static void run_strncpy_overflow(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC(do_strncpy(p, g_str_overflow, 17));

	ops->free(p);
	zassert_true(fired, "strncpy overflow not detected (n=17 into 16-byte buffer)");
}

static void run_strncpy_exact(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC(do_strncpy(p, g_str_overflow, 16));

	ops->free(p);
	zassert_false(fired, "false positive on exact strncpy (n=16)");
}

static void run_strcat_overflow(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	do_memcpy(p, g_str_half, 9);   /* p = "abcdefgh\0" */
	bool fired = EXPECT_PANIC(do_strcat(p, g_str_append_overflow));

	ops->free(p);
	zassert_true(fired, "strcat overflow not detected (8+9+1=18 into 16-byte buffer)");
}

static void run_strcat_exact(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	do_memcpy(p, g_str_half, 9);
	bool fired = EXPECT_PANIC(do_strcat(p, g_str_append_exact));

	ops->free(p);
	zassert_false(fired, "false positive on exact strcat (8+7+1=16)");
}

static void run_strncat_overflow(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	do_memcpy(p, g_str_half, 9);
	/* n=8: check_write_range(p+8, 9) → p[8..16] overflows */
	bool fired = EXPECT_PANIC(do_strncat(p, g_str_overflow, 8));

	ops->free(p);
	zassert_true(fired, "strncat overflow not detected (n=8, writes 9 bytes from p[8])");
}

static void run_strncat_exact(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	do_memcpy(p, g_str_half, 9);
	/* n=7: check_write_range(p+8, 8) → p[8..15] exact */
	bool fired = EXPECT_PANIC(do_strncat(p, g_str_overflow, 7));

	ops->free(p);
	zassert_false(fired, "false positive on exact strncat (n=7, 8+7+1=16)");
}

static void run_strlcpy_overflow(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC(do_strlcpy(p, g_str_overflow, 17));

	ops->free(p);
	zassert_true(fired, "strlcpy overflow not detected (siz=17 into 16-byte buffer)");
}

static void run_strlcpy_exact(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC(do_strlcpy(p, g_str_overflow, 16));

	ops->free(p);
	zassert_false(fired, "false positive on exact strlcpy (siz=16)");
}

static void run_strlcat_overflow(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	do_memcpy(p, g_str_half, 9);
	bool fired = EXPECT_PANIC(do_strlcat(p, g_str_overflow, 17));

	ops->free(p);
	zassert_true(fired, "strlcat overflow not detected (siz=17 into 16-byte buffer)");
}

static void run_strlcat_exact(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	do_memcpy(p, g_str_half, 9);
	bool fired = EXPECT_PANIC(do_strlcat(p, g_str_overflow, 16));

	ops->free(p);
	zassert_false(fired, "false positive on exact strlcat (siz=16)");
}

static void run_snprintf_overflow(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC(do_snprintf(p, 17, g_str_overflow));

	ops->free(p);
	zassert_true(fired, "snprintf overflow not detected (n=17 into 16-byte buffer)");
}

static void run_snprintf_exact(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC(do_snprintf(p, 16, g_str_overflow));

	ops->free(p);
	zassert_false(fired, "false positive on exact snprintf (n=16)");
}

/*
 * sprintf: __asan_vsprintf dry-runs vsnprintf(NULL,0,...) to get the byte
 * count, then checks before writing.
 */
static void run_sprintf_overflow(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC(do_sprintf(p, g_str_overflow));

	ops->free(p);
	zassert_true(fired, "sprintf overflow not detected (17 chars + null into 16 bytes)");
}

static void run_sprintf_exact(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC(do_sprintf(p, g_str_exact));

	ops->free(p);
	zassert_false(fired, "false positive on exact sprintf (15 chars + null = 16 bytes)");
}

static void run_sprintf_use_after_free(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	ops->free(p);
	bool fired = EXPECT_PANIC(do_sprintf(p, g_str_exact));

	zassert_true(fired, "sprintf use-after-free not detected");
}

/* str operations on stack must not trigger ASAN. */
static void run_str_non_heap_no_false_positive(struct heap_ops *ops)
{
	ARG_UNUSED(ops);
	char stack_buf[64];
	static char fgets_src[] = "hi\n";
	bool fired = EXPECT_PANIC({
		do_strcpy(stack_buf, g_str_exact);
		do_stpcpy(stack_buf, g_str_exact);
		do_stpncpy(stack_buf, g_str_exact, sizeof(stack_buf));
		do_strcat(stack_buf, " ok");
		do_memccpy(stack_buf, g_src_bytes, 0, sizeof(stack_buf));
		do_mempcpy(stack_buf, g_src_bytes, 16);
		do_sprintf(stack_buf, g_str_exact);
		do_snprintf(stack_buf, sizeof(stack_buf), g_str_exact);
		FILE *f = fmemopen(fgets_src, sizeof(fgets_src), "r");
		if (f != NULL) {
			do_fgets(stack_buf, sizeof(stack_buf), f);
			fclose(f);
		}
	});

	zassert_false(fired,
		      "false positive: str/printf to stack buffer must not trigger ASAN");
}

/*
 * sprintf with empty format string: vsnprintf(NULL,0,...) returns 0 (needed==0).
 * __asan_vsprintf must still check 1 byte (the null terminator) via the
 * "needed >= 0" branch.  For any allocation >= 1 byte this must not panic.
 */
static void run_sprintf_empty_output_no_false_positive(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC(do_sprintf(p, ""));

	ops->free(p);
	zassert_false(fired,
		      "false positive: sprintf with empty output writes 1 null byte");
}

/*
 * Zero-size string operations: all must hit the size==0 early return in
 * check_write_range and not panic.
 *   strncpy(p, src, 0) — writes nothing
 *   strlcpy(p, src, 0) — writes nothing (siz=0: returns strlen(src), does nothing)
 *   snprintf(p, 0, ...) — writes nothing
 */
static void run_zero_size_str_ops_no_false_positive(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC({
		do_strncpy(p, g_str_overflow, 0); /* n=0: check(p,0) → skip */
		do_strlcpy(p, g_str_overflow, 0); /* siz=0: check(p,0) → skip */
		do_snprintf(p, 0, g_str_overflow); /* n=0: check(p,0) → skip */
	});

	ops->free(p);
	zassert_false(fired,
		      "false positive: zero-size str ops must not trigger ASAN");
}

/*
 * strncat conservative check: our interceptor uses check_write_range(p+len, n+1)
 * rather than min(strlen(src),n)+1 — intentionally conservative.
 * strncat(p, "X", 8) with 8 chars already in buf: n+1=9 > 8 remaining bytes,
 * so ASAN fires even though the actual write is only 2 bytes ("X\0").
 * This flags risky usage: declared capacity n exceeds remaining buffer space.
 */
static void run_strncat_conservative_check(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	do_memcpy(p, g_str_half, 9);  /* p = "abcdefgh\0" */
	/* n=8: check(p+8, 9) → p[8..16] → p[16] poisoned, even though
	 * strncat("X", 8) would only write 2 bytes.                      */
	bool fired = EXPECT_PANIC(do_strncat(p, "X", 8));

	ops->free(p);
	zassert_true(fired,
		     "strncat n > remaining capacity must be caught "
		     "(conservative check: n+1 bytes checked, not min(strlen,n)+1)");
}


static void run_stpcpy_overflow(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC(do_stpcpy(p, g_str_overflow));

	ops->free(p);
	zassert_true(fired, "stpcpy overflow not detected (18 bytes into 16-byte buffer)");
}

static void run_stpcpy_exact(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC(do_stpcpy(p, g_str_exact));

	ops->free(p);
	zassert_false(fired, "false positive on exact stpcpy (15 chars + null = 16 bytes)");
}


static void run_stpncpy_overflow(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC(do_stpncpy(p, g_str_overflow, 17));

	ops->free(p);
	zassert_true(fired, "stpncpy overflow not detected (n=17 into 16-byte buffer)");
}

static void run_stpncpy_exact(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC(do_stpncpy(p, g_str_overflow, 16));

	ops->free(p);
	zassert_false(fired, "false positive on exact stpncpy (n=16)");
}


static void run_memccpy_overflow(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	/* n=17: __asan_memccpy checks 17 bytes at dst before copying. */
	bool fired = EXPECT_PANIC(do_memccpy(p, g_src_bytes, 0, 17));

	ops->free(p);
	zassert_true(fired, "memccpy overflow not detected (n=17 into 16-byte buffer)");
}

static void run_memccpy_exact(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC(do_memccpy(p, g_src_bytes, 0, 16));

	ops->free(p);
	zassert_false(fired, "false positive on exact memccpy (n=16)");
}


static void run_mempcpy_overflow(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC(do_mempcpy(p, g_src_bytes, 17));

	ops->free(p);
	zassert_true(fired, "mempcpy overflow not detected (n=17 into 16-byte buffer)");
}

static void run_mempcpy_exact(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	bool fired = EXPECT_PANIC(do_mempcpy(p, g_src_bytes, 16));

	ops->free(p);
	zassert_false(fired, "false positive on exact mempcpy (n=16)");
}


/*
 * fgets overflow: __asan_fgets checks size bytes at buf before calling fgets.
 * The check fires before any I/O occurs, so no actual stdin input is needed.
 */
static void run_fgets_overflow(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");
	/* size=17 → check(p, 17) fires on 16-byte buffer before fgets is called. */
	bool fired = EXPECT_PANIC(do_fgets(p, 17, stdin));

	ops->free(p);
	zassert_true(fired, "fgets overflow not detected (size=17 into 16-byte buffer)");
}

static void run_fgets_exact(struct heap_ops *ops)
{
	char *p = ops->alloc(16);

	zassert_not_null(p, "alloc failed");

	/* Use fmemopen so fgets reads from memory instead of blocking stdin. */
	static char src[] = "hello\n";
	FILE *f = fmemopen(src, sizeof(src), "r");

	if (f == NULL) {
		ops->free(p);
		ztest_test_skip();
		return;
	}

	bool fired = EXPECT_PANIC(do_fgets(p, 16, f));

	fclose(f);
	ops->free(p);
	zassert_false(fired, "false positive on exact fgets (size=16 into 16-byte buffer)");
}


/* clang-format off */
#define ASAN_COMMON_TESTS(suite)                                                       \
	ZTEST_F(suite, test_normal)                                                    \
		{ run_normal(&fixture->ops); }                                         \
	ZTEST_F(suite, test_overflow)                                                  \
		{ run_overflow(&fixture->ops); }                                       \
	ZTEST_F(suite, test_overflow_no_false_positive)                                \
		{ run_overflow_no_false_positive(&fixture->ops); }                     \
	ZTEST_F(suite, test_overflow_8byte)                                            \
		{ run_overflow_8byte(&fixture->ops); }                                 \
	ZTEST_F(suite, test_overflow_8byte_no_false_positive)                          \
		{ run_overflow_8byte_no_false_positive(&fixture->ops); }               \
	ZTEST_F(suite, test_overflow_8byte_at_slot7)                                   \
		{ run_overflow_8byte_at_slot7(&fixture->ops); }                        \
	ZTEST_F(suite, test_overflow_8byte_at_slot7_no_false_positive)                 \
		{ run_overflow_8byte_at_slot7_no_false_positive(&fixture->ops); }      \
	ZTEST_F(suite, test_overflow_misaligned_granule_span)                          \
		{ run_overflow_misaligned_granule_span(&fixture->ops); }               \
	ZTEST_F(suite, test_overflow_misaligned_granule_span_no_false_positive)        \
		{ run_overflow_misaligned_granule_span_no_false_positive(&fixture->ops); } \
	ZTEST_F(suite, test_underflow)                                                 \
		{ run_underflow(&fixture->ops); }                                      \
	ZTEST_F(suite, test_use_after_free)                                            \
		{ run_use_after_free(&fixture->ops); }                                 \
	ZTEST_F(suite, test_use_after_free_memset)                                     \
		{ run_use_after_free_memset(&fixture->ops); }                          \
	ZTEST_F(suite, test_use_after_free_memcpy)                                     \
		{ run_use_after_free_memcpy(&fixture->ops); }                          \
	ZTEST_F(suite, test_use_after_free_interior_ptr)                               \
		{ run_use_after_free_interior_ptr(&fixture->ops); }                    \
	ZTEST_F(suite, test_realloc_shrink_suffix)                                     \
		{ run_realloc_shrink_suffix(&fixture->ops); }                          \
	ZTEST_F(suite, test_realloc_shrink_no_false_positive)                          \
		{ run_realloc_shrink_no_false_positive(&fixture->ops); }               \
	ZTEST_F(suite, test_realloc_expand)                                            \
		{ run_realloc_expand(&fixture->ops); }                                 \
	ZTEST_F(suite, test_realloc_expand_no_false_positive)                          \
		{ run_realloc_expand_no_false_positive(&fixture->ops); }               \
	ZTEST_F(suite, test_realloc_free_use_after)                                    \
		{ run_realloc_free_use_after(&fixture->ops); }                         \
	ZTEST_F(suite, test_use_after_realloc_moved)                                   \
		{ run_use_after_realloc_moved(&fixture->ops); }                        \
	ZTEST_F(suite, test_realloc_shrink_to_one_granule_boundary)                    \
		{ run_realloc_shrink_to_one_granule_boundary(&fixture->ops); }         \
	ZTEST_F(suite, test_memset_overflow)                                           \
		{ run_memset_overflow(&fixture->ops); }                                \
	ZTEST_F(suite, test_memset_exact)                                              \
		{ run_memset_exact(&fixture->ops); }                                   \
	ZTEST_F(suite, test_memcpy_overflow)                                           \
		{ run_memcpy_overflow(&fixture->ops); }                                \
	ZTEST_F(suite, test_memcpy_exact)                                              \
		{ run_memcpy_exact(&fixture->ops); }                                   \
	ZTEST_F(suite, test_memmove_overflow)                                          \
		{ run_memmove_overflow(&fixture->ops); }                               \
	ZTEST_F(suite, test_memmove_exact)                                             \
		{ run_memmove_exact(&fixture->ops); }                                  \
	ZTEST_F(suite, test_memccpy_overflow)                                          \
		{ run_memccpy_overflow(&fixture->ops); }                               \
	ZTEST_F(suite, test_memccpy_exact)                                             \
		{ run_memccpy_exact(&fixture->ops); }                                  \
	ZTEST_F(suite, test_mempcpy_overflow)                                          \
		{ run_mempcpy_overflow(&fixture->ops); }                               \
	ZTEST_F(suite, test_mempcpy_exact)                                             \
		{ run_mempcpy_exact(&fixture->ops); }                                  \
	ZTEST_F(suite, test_memset_mid_overflow)                                       \
		{ run_memset_mid_overflow(&fixture->ops); }                            \
	ZTEST_F(suite, test_memset_mid_exact)                                          \
		{ run_memset_mid_exact(&fixture->ops); }                               \
	ZTEST_F(suite, test_granule_single_slot)                                       \
		{ run_granule_single_slot(&fixture->ops); }                            \
	ZTEST_F(suite, test_granule_mid_slot)                                          \
		{ run_granule_mid_slot(&fixture->ops); }                               \
	ZTEST_F(suite, test_bulk_zero_size)                                            \
		{ run_bulk_zero_size(&fixture->ops); }                                 \
	ZTEST_F(suite, test_large_alloc_boundary)                                      \
		{ run_large_alloc_boundary(&fixture->ops); }                           \
	ZTEST_F(suite, test_adjacent_allocs_overflow)                                  \
		{ run_adjacent_allocs_overflow(&fixture->ops); }                       \
	ZTEST_F(suite, test_adjacent_allocs_no_false_positive)                         \
		{ run_adjacent_allocs_no_false_positive(&fixture->ops); }              \
	ZTEST_F(suite, test_memcpy_heap_src_stack_dst_no_false_positive)               \
		{ run_memcpy_heap_src_stack_dst_no_false_positive(&fixture->ops); }    \
	ZTEST_F(suite, test_use_after_free_then_realloc)                               \
		{ run_use_after_free_then_realloc(&fixture->ops); }                    \
	ZTEST_F(suite, test_non_heap_no_false_positive)                                \
		{ run_non_heap_no_false_positive(&fixture->ops); }

#define ASAN_STR_TESTS(suite)                                                          \
	ZTEST_F(suite, test_strcpy_overflow)                                           \
		{ run_strcpy_overflow(&fixture->ops); }                                \
	ZTEST_F(suite, test_strcpy_exact)                                              \
		{ run_strcpy_exact(&fixture->ops); }                                   \
	ZTEST_F(suite, test_strcpy_use_after_free)                                     \
		{ run_strcpy_use_after_free(&fixture->ops); }                          \
	ZTEST_F(suite, test_strncpy_overflow)                                          \
		{ run_strncpy_overflow(&fixture->ops); }                               \
	ZTEST_F(suite, test_strncpy_exact)                                             \
		{ run_strncpy_exact(&fixture->ops); }                                  \
	ZTEST_F(suite, test_strcat_overflow)                                           \
		{ run_strcat_overflow(&fixture->ops); }                                \
	ZTEST_F(suite, test_strcat_exact)                                              \
		{ run_strcat_exact(&fixture->ops); }                                   \
	ZTEST_F(suite, test_strncat_overflow)                                          \
		{ run_strncat_overflow(&fixture->ops); }                               \
	ZTEST_F(suite, test_strncat_exact)                                             \
		{ run_strncat_exact(&fixture->ops); }                                  \
	ZTEST_F(suite, test_strlcpy_overflow)                                          \
		{ run_strlcpy_overflow(&fixture->ops); }                               \
	ZTEST_F(suite, test_strlcpy_exact)                                             \
		{ run_strlcpy_exact(&fixture->ops); }                                  \
	ZTEST_F(suite, test_strlcat_overflow)                                          \
		{ run_strlcat_overflow(&fixture->ops); }                               \
	ZTEST_F(suite, test_strlcat_exact)                                             \
		{ run_strlcat_exact(&fixture->ops); }                                  \
	ZTEST_F(suite, test_snprintf_overflow)                                         \
		{ run_snprintf_overflow(&fixture->ops); }                              \
	ZTEST_F(suite, test_snprintf_exact)                                            \
		{ run_snprintf_exact(&fixture->ops); }                                 \
	ZTEST_F(suite, test_sprintf_overflow)                                          \
		{ run_sprintf_overflow(&fixture->ops); }                               \
	ZTEST_F(suite, test_sprintf_exact)                                             \
		{ run_sprintf_exact(&fixture->ops); }                                  \
	ZTEST_F(suite, test_sprintf_use_after_free)                                    \
		{ run_sprintf_use_after_free(&fixture->ops); }                         \
	ZTEST_F(suite, test_sprintf_empty_output_no_false_positive)                    \
		{ run_sprintf_empty_output_no_false_positive(&fixture->ops); }         \
	ZTEST_F(suite, test_zero_size_str_ops_no_false_positive)                       \
		{ run_zero_size_str_ops_no_false_positive(&fixture->ops); }            \
	ZTEST_F(suite, test_strncat_conservative_check)                                \
		{ run_strncat_conservative_check(&fixture->ops); }                     \
	ZTEST_F(suite, test_stpcpy_overflow)                                           \
		{ run_stpcpy_overflow(&fixture->ops); }                                \
	ZTEST_F(suite, test_stpcpy_exact)                                              \
		{ run_stpcpy_exact(&fixture->ops); }                                   \
	ZTEST_F(suite, test_stpncpy_overflow)                                          \
		{ run_stpncpy_overflow(&fixture->ops); }                               \
	ZTEST_F(suite, test_stpncpy_exact)                                             \
		{ run_stpncpy_exact(&fixture->ops); }                                  \
	ZTEST_F(suite, test_fgets_overflow)                                            \
		{ run_fgets_overflow(&fixture->ops); }                                 \
	ZTEST_F(suite, test_fgets_exact)                                               \
		{ run_fgets_exact(&fixture->ops); }                                    \
	ZTEST_F(suite, test_str_non_heap_no_false_positive)                            \
		{ run_str_non_heap_no_false_positive(&fixture->ops); }
/* clang-format on */


ZTEST_SUITE(asan_malloc, NULL, malloc_setup, NULL, NULL, NULL);
ASAN_COMMON_TESTS(asan_malloc)
ASAN_STR_TESTS(asan_malloc)


ZTEST_SUITE(asan_sys_heap, NULL, sys_heap_setup, NULL, NULL, NULL);
ASAN_COMMON_TESTS(asan_sys_heap)
ASAN_STR_TESTS(asan_sys_heap)
