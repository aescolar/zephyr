/*
 * Copyright (c) 2017 Oticon A/S
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The basic principle of operation is:
 *   No asynchronous behavior, no indeterminism.
 *   If you run the same thing 20 times, you get exactly the same result 20
 *   times.
 *   It does not matter if you are running from console, or in a debugger
 *   and you go for lunch in the middle of the debug session.
 *
 * This is achieved as follows:
 *  The execution of native_posix is decoupled from the underlying host and its
 *  peripherals (unless set otherwise).
 *  In general, time in native_posix is simulated.
 *
 * But, native_posix can also be linked if desired to the underlying host,
 * e.g.:You can use the provided Ethernet TAP driver, or a host BLE controller.
 *
 * In this case, the no-indeterminism principle is lost. Runs of native_posix
 * will depend on the host load and the interactions with those real host
 * peripherals.
 *
 */


#include <stdio.h>
#include <stdlib.h>

#include "ler_cpu_if.h"
#include "ler_tasks.h"
#include "ler_cmdline.h"
#include "ler_utils.h"

#include "hw_models_top.h"

void ler_exit(int exit_code)
{
	static int max_exit_code;

	max_exit_code = L_MAX(exit_code, max_exit_code);
	/*
	 * posix_soc_clean_up may not return if this is called from a SW thread,
	 * but instead it would get posix_exit() recalled again
	 * ASAP from the HW thread
	 */
	lrif_cpu0_cleanup();
	run_ler_tasks(LERTASK_ON_EXIT_LEVEL);
	hwm_cleanup();
	ler_cleanup_cmd_line();
	exit(max_exit_code);
}

/**
 * Run all early native_posix initialization steps, including command
 * line parsing and CPU start, until we are ready to let the HW models
 * run via hwm_one_event()
 */
static void ler_init(int argc, char *argv[])
{
	/*
	 * Let's ensure that even if we are redirecting to a file, we get stdout
	 * and stderr line buffered (default for console)
	 * Note that glibc ignores size. But just in case we set a reasonable
	 * number in case somebody tries to compile against a different library
	 */
	setvbuf(stdout, NULL, _IOLBF, 512);
	setvbuf(stderr, NULL, _IOLBF, 512);

	run_ler_tasks(LERTASK_PRE_BOOT_1_LEVEL);
	lrif_cpu0_pre_cmdline_hooks();

	ler_handle_cmd_line(argc, argv);

	run_ler_tasks(LERTASK_PRE_BOOT_2_LEVEL);
	lrif_cpu0_pre_hw_init_hooks();

	hwm_init();

	run_ler_tasks(LERTASK_PRE_BOOT_3_LEVEL);

	lrif_cpu0_boot();

	run_ler_tasks(LERTASK_FIRST_SLEEP_LEVEL);
}

/**
 * Execute the simulator for at least the specified timeout, then
 * return.  Note that this does not affect event timing, so the "next
 * event" may be significantly after the request if the hardware has
 * not been configured to e.g. send an interrupt when expected.
 */
void ler_exec_for(uint64_t us)
{
	uint64_t start = hwm_get_time();

	do {
		hwm_one_event();
	} while (hwm_get_time() < (start + us));
}

#ifndef CONFIG_ARCH_POSIX_LIBFUZZER

/**
 * This is the actual host process main routine.  The Zephyr
 * application's main() is renamed via preprocessor trickery to avoid
 * collisions.
 *
 * Not used when building fuzz cases, as libfuzzer has its own main()
 * and calls the "OS" through a per-case fuzz test entry point.
 */
int main(int argc, char *argv[])
{
	ler_init(argc, argv);
	while (true) {
		hwm_one_event();
	}

	/* This line should be unreachable */
	return 1; /* LCOV_EXCL_LINE */
}

#else /* CONFIG_ARCH_POSIX_LIBFUZZER */

/**
 * Entry point for fuzzing (when enabled). Works by placing the data
 * into two known symbols, triggering an app-visible interrupt, and
 * then letting the OS run for a fixed amount of time (intended to be
 * "long enough" to handle the event and reach a quiescent state
 * again)
 */
uint8_t *ler_fuzz_buf, ler_fuzz_sz;

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t sz)
{
	static bool ler_initialized;

	if (!ler_initialized) {
		ler_init(0, NULL);
		ler_initialized = true;
	}

	/* Provide the fuzz data to Zephyr as an interrupt, with
	 * "DMA-like" data placed into ler_fuzz_buf/sz
	 */
	ler_fuzz_buf = (void *)data;
	ler_fuzz_sz = sz;
	hw_irq_ctrl_set_irq(CONFIG_ARCH_POSIX_FUZZ_IRQ);

	/* Give the OS time to process whatever happened in that
	 * interrupt and reach an idle state.
	 */
	ler_exec_for(k_ticks_to_us_ceil64(CONFIG_ARCH_POSIX_FUZZ_TICKS));

	return 0;
}

#endif
