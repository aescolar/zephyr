/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Run the set of special LER tasks corresponding to the given level
 *
 * @param level One of _LERTASK_*_LEVEL as defined in ler_tasks.h
 */
void run_ler_tasks(int level)
{
	extern void (*__ler_PRE_BOOT_1_tasks_start[])(void);
	extern void (*__ler_PRE_BOOT_2_tasks_start[])(void);
	extern void (*__ler_PRE_BOOT_3_tasks_start[])(void);
	extern void (*__ler_FIRST_SLEEP_tasks_start[])(void);
	extern void (*__ler_ON_EXIT_tasks_start[])(void);
	extern void (*__ler_tasks_end[])(void);

	static void (**ler_pre_tasks[])(void) = {
		__ler_PRE_BOOT_1_tasks_start,
		__ler_PRE_BOOT_2_tasks_start,
		__ler_PRE_BOOT_3_tasks_start,
		__ler_FIRST_SLEEP_tasks_start,
		__ler_ON_EXIT_tasks_start,
		__ler_tasks_end
	};

	void (**fptr)(void);

	for (fptr = ler_pre_tasks[level]; fptr < ler_pre_tasks[level+1];
		fptr++) {
		if (*fptr) { /* LCOV_EXCL_BR_LINE */
			(*fptr)();
		}
	}
}
