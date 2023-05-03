/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Linker command/script file for the linux runner
 */

#define LER_INIT_LEVEL(level)					\
		__ler_##level##_tasks_start = .;			\
		KEEP(*(SORT(.ler_##level[0-9]_task)));		\
		KEEP(*(SORT(.ler_##level[1-9][0-9]_task)));		\
		KEEP(*(SORT(.ler_##level[1-9][0-9][0-9]_task)));	\

SECTIONS
 {
	ler_tasks :
	{
		__ler_tasks_start = .;
		LER_INIT_LEVEL(PRE_BOOT_1)
		LER_INIT_LEVEL(PRE_BOOT_2)
		LER_INIT_LEVEL(PRE_BOOT_3)
		LER_INIT_LEVEL(FIRST_SLEEP)
		LER_INIT_LEVEL(ON_EXIT)
		__ler_tasks_end = .;
	}
 } INSERT AFTER .data;

/*
 * Note this script augments the default host linker script with extra SECTIONS
 */
