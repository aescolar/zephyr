/*
 * Copyright (c) 2017 Oticon A/S
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Linux embedded runner, CPU Thread emulation (lct)
 * TODO: rename LTS to LCT, fix comments, change bottom -> lct,..
 * Clear function names, specially in interface
 */

/**
 * Here is where things actually happen for the POSIX arch
 *
 * We isolate all functions here, to ensure they can be compiled as
 * independently as possible to the remainder of Zephyr to avoid name clashes
 * as Zephyr does provide functions with the same names as the POSIX threads
 * functions
 */
/**
 * Principle of operation:
 *
 * The Zephyr OS and its app run as a set of native pthreads.
 * The Zephyr OS only sees one of this thread executing at a time.
 * Which is running is controlled using {cond|mtx}_threads and
 * currently_allowed_thread.
 *
 * The main part of the execution of each thread will occur in a fully
 * synchronous and deterministic manner, and only when commanded by the Zephyr
 * kernel.
 * But the creation of a thread will spawn a new pthread whose start
 * is asynchronous to the rest, until synchronized in posix_wait_until_allowed()
 * below.
 * Similarly aborting and canceling threads execute a tail in a quite
 * asynchronous manner.
 *
 * This implementation is meant to be portable in between POSIX systems.
 * A table (threads_table) is used to abstract the native pthreads.
 * And index in this table is used to identify threads in the IF to the kernel.
 *
 */

#define LTS_ARCH_DEBUG_PRINTS 0

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "lct_if.h"
#include "posix_arch_internal.h"

#if LTS_ARCH_DEBUG_PRINTS
#define LTS_DEBUG(fmt, ...) posix_print_trace(PREFIX fmt, __VA_ARGS__)
#else
#define LTS_DEBUG(...)
#endif

#define PREFIX     "Tread Simulator: "
#define ERPREFIX   PREFIX"error on "
#define NO_MEM_ERR PREFIX"Can't allocate memory\n"

#define LTS_ALLOC_CHUNK_SIZE 64
#define LTS_REUSE_ABORTED_ENTRIES 0
/* tests/kernel/threads/scheduling/schedule_api fails when setting
 * LTS_REUSE_ABORTED_ENTRIES => don't set it by now
 */

struct ts_status_t;

struct threads_table_el {
	struct ts_status_t *ts_status; /* Pointer to the overall status of the threading simulator instance */
	int thread_idx; /* Index of this element in the bottom instance threads_table*/

	enum {NOTUSED = 0, USED, ABORTING, ABORTED, FAILED} state;
	bool running;     /* Is this the currently running thread */
	pthread_t thread; /* Actual pthread_t as returned by native kernel */
	int thead_cnt; /* For debugging: Unique, consecutive, thread number */

	/* Pointer to data from the hosted OS architecture
	 * What that is, if anything, is up to that the hosted OS */
	void *payload;
};

struct ts_status_t {
	struct threads_table_el *threads_table;
	int thread_create_count; /* For debugging. Thread creation counter */
	int threads_table_size;
	/*
	 * Conditional variable to block/awake all threads during swaps()
	 * (we only need 1 mutex and 1 cond variable for all threads)
	 */
	pthread_cond_t cond_threads;
	/* Mutex for the conditional variable posix_core_cond_threads */
	pthread_mutex_t mtx_threads;
	/* Token which tells which process is allowed to run now */
	int currently_allowed_thread;
	bool terminate; /* Are we terminating the program == cleaning up */
};

static void lct_wait_until_allowed(struct ts_status_t* this, int this_th_nbr);
static void *lct_thread_starter(void *arg);
static void lct_preexit_cleanup(struct ts_status_t* this);

/**
 * Helper function, run by a thread is being aborted
 */
static void abort_tail(struct ts_status_t* this, int this_th_nbr)
{
	LTS_DEBUG("Thread [%i] %i: %s: Aborting (exiting) (rel mut)\n",
		threads_table[this_th_nbr].thead_cnt,
		this_th_nbr,
		__func__);

	this->threads_table[this_th_nbr].running = false;
	this->threads_table[this_th_nbr].state = ABORTED;
	lct_preexit_cleanup(this);
	pthread_exit(NULL);
}

/**
 *  Helper function to block this thread until it is allowed again
 *  (somebody calls posix_let_run() with this thread number
 *
 * Note that we go out of this function (the while loop below)
 * with the mutex locked by this particular thread.
 * In normal circumstances, the mutex is only unlocked internally in
 * pthread_cond_wait() while waiting for cond_threads to be signaled
 */
static void lct_wait_until_allowed(struct ts_status_t* this, int this_th_nbr)
{
	this->threads_table[this_th_nbr].running = false;

	LTS_DEBUG("Thread [%i] %i: %s: Waiting to be allowed to run (rel mut)\n",
		this->threads_table[this_th_nbr].thead_cnt,
		this_th_nbr,
		__func__);

	while (this_th_nbr != this->currently_allowed_thread) {
		pthread_cond_wait(&this->cond_threads, &this->mtx_threads);

		if (this->threads_table &&
		    (this->threads_table[this_th_nbr].state == ABORTING)) {
			abort_tail(this, this_th_nbr);
		}
	}

	this->threads_table[this_th_nbr].running = true;

	LTS_DEBUG("Thread [%i] %i: %s(): I'm allowed to run! (hav mut)\n",
		this->threads_table[this_th_nbr].thead_cnt,
		this_th_nbr,
		__func__);
}


/**
 * Helper function to let the thread <next_allowed_th> run
 * Note: posix_let_run() can only be called with the mutex locked
 */
static void lct_let_run(struct ts_status_t *this, int next_allowed_th)
{
	LTS_DEBUG("%s: We let thread [%i] %i run\n",
		__func__,
		this->threads_table[next_allowed_th].thead_cnt,
		next_allowed_th);


	this->currently_allowed_thread = next_allowed_th;

	/*
	 * We let all threads know one is able to run now (it may even be us
	 * again if fancied)
	 * Note that as we hold the mutex, they are going to be blocked until
	 * we reach our own lct_wait_until_allowed() while loop or abort_tail()
	 * mutex release
	 */
	PC_SAFE_CALL(pthread_cond_broadcast(&this->cond_threads));
}


static void lct_preexit_cleanup(struct ts_status_t* this)
{
	//TODO: consider moving the pthread_exit() here
	/*
	 * Release the mutex so the next allowed thread can run
	 */
	PC_SAFE_CALL(pthread_mutex_unlock(&this->mtx_threads));

	/* We detach ourselves so nobody needs to join to us */
	pthread_detach(pthread_self());
}


/**
 * Let the ready thread run and block this thread until it is allowed again
 *
 * called from arch_swap() which does the picking from the kernel structures
 */
void lct_posix_swap(void *this_arg, int next_allowed_thread_nbr, int this_th_nbr)
{
	struct ts_status_t *this = (struct ts_status_t *)this_arg;
	lct_let_run(this, next_allowed_thread_nbr);

	if (this->threads_table[this_th_nbr].state == ABORTING) {
		LTS_DEBUG("Thread [%i] %i: %s: Aborting curr.\n",
			this->threads_table[this_th_nbr].thead_cnt,
			this_th_nbr,
			__func__);
		abort_tail(this, this_th_nbr);
	} else {
		lct_wait_until_allowed(this, this_th_nbr);
	}
}

/**
 * Let the ready thread (main) run, and exit this thread (init)
 *
 * Called from arch_switch_to_main_thread() which does the picking from the
 * kernel structures
 *
 * Note that we could have just done a swap(), but that would have left the
 * init thread lingering. Instead here we exit the init thread after enabling
 * the new one
 */
void lct_main_thread_start(void *this_arg, int next_allowed_thread_nbr)
{
	struct ts_status_t *this = (struct ts_status_t *)this_arg;

	lct_let_run(this, next_allowed_thread_nbr);
	LTS_DEBUG("%s: Init thread dying now (rel mut)\n",
		__func__);
	lct_preexit_cleanup(this);
	pthread_exit(NULL);
}

/**
 * Handler called when any thread is cancelled or exits
 */
static void lct_cleanup_handler(void *arg)
{
	struct threads_table_el* element = (struct threads_table_el*)arg;
	struct ts_status_t *this = element->ts_status;

	/*
	 * If we are not terminating, this is just an aborted thread,
	 * and the mutex was already released
	 * Otherwise, release the mutex so other threads which may be
	 * caught waiting for it could terminate
	 */

	if (!this->terminate) {
		return;
	}

#if LTS_ARCH_DEBUG_PRINTS
	LTS_DEBUG("Thread %i: %s: Canceling (rel mut)\n",
		element->thread_idx,
		__func__);
#endif


	PC_SAFE_CALL(pthread_mutex_unlock(&this->mtx_threads));

	/* We detach ourselves so nobody needs to join to us */
	pthread_detach(pthread_self());
}

/**
 * Helper function to start a Zephyr thread as a POSIX thread:
 *  It will block the thread until a arch_swap() is called for it
 *
 * Spawned from posix_new_thread() below
 */
static void *lct_thread_starter(void *arg)
{
	struct threads_table_el* element = (struct threads_table_el*)arg;
	struct ts_status_t *this = element->ts_status;

	int thread_idx = element->thread_idx;

	LTS_DEBUG("Thread [%i] %i: %s: Starting\n",
		this->threads_table[thread_idx].thead_cnt,
		thread_idx,
		__func__);

	/*
	 * We block until all other running threads reach the while loop
	 * in posix_wait_until_allowed() and they release the mutex
	 */
	PC_SAFE_CALL(pthread_mutex_lock(&this->mtx_threads));

	/*
	 * The program may have been finished before this thread ever got to run
	 */
	/* LCOV_EXCL_START */ /* See Note1 */
	if (!this->threads_table || this->terminate) {
		lct_cleanup_handler(arg);
		pthread_exit(NULL);
	}
	/* LCOV_EXCL_STOP */

	pthread_cleanup_push(lct_cleanup_handler, arg);

	LTS_DEBUG("Thread [%i] %i: %s: After start mutex (hav mut)\n",
		element->thead_cnt,
		thread_idx,
		__func__);

	/*
	 * The thread would try to execute immediately, so we block it
	 * until allowed
	 */
	lct_wait_until_allowed(this, thread_idx);

	posix_arch_thread_entry(element->payload);

	/*
	 * We only reach this point if the thread actually returns which should
	 * not happen. But we handle it gracefully just in case
	 */
	/* LCOV_EXCL_START */
	posix_print_trace(PREFIX"Thread [%i] %i [%lu] ended!?!\n",
			element->thead_cnt,
			thread_idx,
			pthread_self());


	element->running = false;
	element->state = FAILED;

	pthread_cleanup_pop(1);

	return NULL;
	/* LCOV_EXCL_STOP */
}

/**
 * Return the first free entry index in the threads table
 */
static int ttable_get_empty_slot(struct ts_status_t *this)
{

	for (int i = 0; i < this->threads_table_size; i++) {
		if ((this->threads_table[i].state == NOTUSED)
			|| (LTS_REUSE_ABORTED_ENTRIES
			&& (this->threads_table[i].state == ABORTED))) {
			return i;
		}
	}

	/*
	 * else, we run out table without finding an index
	 * => we expand the table
	 */

	this->threads_table = realloc(this->threads_table,
				(this->threads_table_size + LTS_ALLOC_CHUNK_SIZE)
				* sizeof(struct threads_table_el));
	if (this->threads_table == NULL) { /* LCOV_EXCL_BR_LINE */
		posix_print_error_and_exit(NO_MEM_ERR); /* LCOV_EXCL_LINE */
	}

	/* Clear new piece of table */
	(void)memset(&this->threads_table[this->threads_table_size], 0,
		     LTS_ALLOC_CHUNK_SIZE * sizeof(struct threads_table_el));

	this->threads_table_size += LTS_ALLOC_CHUNK_SIZE;

	/* The first newly created entry is good: */
	return this->threads_table_size - LTS_ALLOC_CHUNK_SIZE;
}

/**
 * Create a new POSIX thread for the new hosted OS thread.
 *
 * Returns a unique integer thread identifier/index, which should be used
 * to refer to this thread for future calls to the posix core.
 *
 * It takes as parameter a pointer which will be passed to
 * posix_arch_thread_entry() when the thread is swapped in
 */
int lct_new_thread(void *this_arg, void *payload)
{
	struct ts_status_t *this = (struct ts_status_t *)this_arg;
	int t_slot;

	t_slot = ttable_get_empty_slot(this);
	this->threads_table[t_slot].state = USED;
	this->threads_table[t_slot].running = false;
	this->threads_table[t_slot].thead_cnt = this->thread_create_count++;
	this->threads_table[t_slot].payload = payload;
	this->threads_table[t_slot].ts_status = this;
	this->threads_table[t_slot].thread_idx = t_slot;

	PC_SAFE_CALL(pthread_create(&this->threads_table[t_slot].thread,
				  NULL,
				  lct_thread_starter,
				  (void *)&this->threads_table[t_slot]));

	LTS_DEBUG("%s created thread [%i] %i [%lu]\n",
		__func__,
		this->threads_table[t_slot].thead_cnt,
		t_slot,
		this->threads_table[t_slot].thread);

	return t_slot;
}

/**
 * Called from zephyr_wrapper()
 * prepare whatever needs to be prepared to be able to start threads
 */
void *lct_init(void)
{
	struct ts_status_t *this;

	/*
	 * Note: This (and the calloc below) won't be free'd by this code
	 * but left for the OS to clear at process end.
	 * This is a conscious choice, see lct_clean_up() for mroe info.
	 * If you got here due to valgrind's leak report, please use the
	 * provided valgrind suppression file valgrind.supp
	 */
	this = calloc(1, sizeof(struct ts_status_t));
	if (this == NULL) { /* LCOV_EXCL_BR_LINE */
		posix_print_error_and_exit(NO_MEM_ERR); /* LCOV_EXCL_LINE */
	}

	this->thread_create_count = 0;
	this->currently_allowed_thread = -1;

	PC_SAFE_CALL(pthread_cond_init(&this->cond_threads, NULL));
	PC_SAFE_CALL(pthread_mutex_init(&this->mtx_threads, NULL));

	this->threads_table = calloc(LTS_ALLOC_CHUNK_SIZE,
				sizeof(struct threads_table_el));
	if (this->threads_table == NULL) { /* LCOV_EXCL_BR_LINE */
		posix_print_error_and_exit(NO_MEM_ERR); /* LCOV_EXCL_LINE */
	}

	this->threads_table_size = LTS_ALLOC_CHUNK_SIZE;

	PC_SAFE_CALL(pthread_mutex_lock(&this->mtx_threads));

	return (void*)this;
}

/**
 * Free any allocated memory by the posix core and clean up.
 * Note that this function cannot be called from a SW thread
 * (the CPU is assumed halted. Otherwise we will cancel ourselves)
 *
 * This function cannot guarantee the threads will be cancelled before the HW
 * thread exists. The only way to do that, would be to wait for each of them in
 * a join (without detaching them, but that could lead to locks in some
 * convoluted cases. As a call to this function can come from an ASSERT or other
 * error termination, we better do not assume things are working fine.
 * => we prefer the supposed memory leak report from valgrind, and ensure we
 * will not hang
 *
 */
void lct_clean_up(void *this_arg)
{
	struct ts_status_t *this = (struct ts_status_t *)this_arg;

	if (!this->threads_table) { /* LCOV_EXCL_BR_LINE */
		return; /* LCOV_EXCL_LINE */
	}

	this->terminate = true;

	for (int i = 0; i < this->threads_table_size; i++) {
		if (this->threads_table[i].state != USED) {
			continue;
		}

		/* LCOV_EXCL_START */
		if (pthread_cancel(this->threads_table[i].thread)) {
			posix_print_warning(
				PREFIX"cleanup: could not stop thread %i\n",
				i);
		}
		/* LCOV_EXCL_STOP */
	}

	/*
	 * This is the cleanup we do not do:
	 *
	 * free(this->threads_table);
	 * this->threads_table = NULL;
         *
	 * (void)pthread_cond_destroy(&this->cond_threads);
	 * (void)pthread_mutex_destroy(&this->mtx_threads);
         *
	 * free(this);
	 */
}


/*
 * Mark a thread as being aborted. This will result in the underlying pthread
 * being terminated some time later:
 *   If the thread is marking itself as aborting, as soon as it is swapped out
 *   by the hosted (embedded) OS
 *   If another thread, at some non-specific time in the future (But note that no
 *   embedded part of the thread will execute anymore)
 *
 * *  thread_idx : The thread identifier as provided during creation
 * *  self       : Should be true (!=0) if this call is happening from that thread
 *                 itself
 */
void lct_abort_thread(void *this_arg, int thread_idx, int self)
{
	struct ts_status_t *this = (struct ts_status_t *)this_arg;

	if (self) {
		LTS_DEBUG("Thread [%i] %i: %s Marked myself "
			"as aborting\n",
			this->threads_table[thread_idx].thead_cnt,
			thread_idx,
			__func__);
	} else {
		if (this->threads_table[thread_idx].state != USED) { /* LCOV_EXCL_BR_LINE */
			/* The thread may have been already aborted before */
			return; /* LCOV_EXCL_LINE */
		}

		LTS_DEBUG("Aborting not scheduled thread [%i] %i\n",
			this->threads_table[thread_idx].thead_cnt,
			thread_idx);
	}
	this->threads_table[thread_idx].state = ABORTING;
	/*
	 * Note: the native thread will linger in RAM until it catches the
	 * mutex or awakes on the condition.
	 * Note that even if we would pthread_cancel() the thread here, that
	 * would be the case, but with a pthread_cancel() the mutex state would
	 * be uncontrolled
	 */
}

/*
 * Return a thread identifier which is unique for this
 * run. This identifier is only meant for debug purposes
 */
int lct_get_unique_thread_id(void *this_arg, int thread_idx){
	struct ts_status_t *this = (struct ts_status_t *)this_arg;

	return this->threads_table[thread_idx].thead_cnt;
}

/*
 * Notes about coverage:
 *
 * Note1:
 *
 * This condition will only be triggered in very unlikely cases
 * (once every few full regression runs).
 * It is therefore excluded from the coverage report to avoid confusing
 * developers.
 *
 * Background: This arch creates a pthread as soon as the Zephyr kernel creates
 * a Zephyr thread. A pthread creation is an asynchronous process handled by the
 * host kernel.
 *
 * This architecture normally keeps only 1 thread executing at a time.
 * But part of the pre-initialization during creation of a new thread
 * and some cleanup at the tail of the thread termination are executed
 * in parallel to other threads.
 * That is, the execution of those code paths is a bit indeterministic.
 *
 * Only when the Zephyr kernel attempts to swap to a new thread does this
 * architecture need to wait until its pthread is ready and initialized
 * (has reached posix_wait_until_allowed())
 *
 * In some cases (tests) threads are created which are never actually needed
 * (typically the idle thread). That means the test may finish before this
 * thread's underlying pthread has reached posix_wait_until_allowed().
 *
 * In this unlikely cases the initialization or cleanup of the thread follows
 * non-typical code paths.
 * This code paths are there to ensure things work always, no matter
 * the load of the host. Without them, very rare & mysterious segfault crashes
 * would occur.
 * But as they are very atypical and only triggered with some host loads,
 * they will be covered in the coverage reports only rarely.
 *
 * Note2:
 *
 * Some other code will never or only very rarely trigger and is therefore
 * excluded with LCOV_EXCL_LINE
 *
 */
