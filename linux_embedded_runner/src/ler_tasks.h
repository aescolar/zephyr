/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _LINUX_EMBEDDED_RUNNER_LER_TASKS_H
#define _LINUX_EMBEDDED_RUNNER_LER_TASKS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * LER_TASK
 *
 * Register a function to be called at particular moments
 * during the Linux Embedded Runner execution.
 *
 * There is 5 choices for when the function will be called (level):
 * * PRE_BOOT_1: Will be called before the command line parameters are parsed,
 *   or the HW models are initialized
 *
 * * PRE_BOOT_2: Will be called after the command line parameters are parsed,
 *   but before the HW models are initialized
 *
 * * PRE_BOOT_3: Will be called after the HW models initialization, right before
 *   the "CPU is booted" and embedded SW is started.
 *
 * * FIRST_SLEEP: Will be called the 1st time the CPU is sent to sleep
 *
 * * ON_EXIT: Will be called during termination of the runner
 * execution.
 *
 * The function must take no parameters and return nothing.
 */
#define LER_TASK(fn, level, prio)	\
	static void (* const _CONCAT(__ler_task_, fn))() __used __noasan \
	__attribute__((__section__(".ler_" #level STRINGIFY(prio) "_task")))\
	= fn

#define LERTASK_PRE_BOOT_1_LEVEL	0
#define LERTASK_PRE_BOOT_2_LEVEL	1
#define LERTASK_PRE_BOOT_3_LEVEL	2
#define LERTASK_FIRST_SLEEP_LEVEL	3
#define LERTASK_ON_EXIT_LEVEL		4

/**
 * @brief Run the set of special native tasks corresponding to the given level
 *
 * @param level One of LERTASK_*_LEVEL as defined in soc.h
 */
void run_ler_tasks(int level);

#ifdef __cplusplus
}
#endif

#endif /* _LINUX_EMBEDDED_RUNNER_LER_TASKS_H */
