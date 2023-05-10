/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <soc.h>
#include <posix_native_task.h>
#include <ler_cpu_if.h>

void lrif_cpu0_pre_cmdline_hooks(void){
	run_native_tasks(_NATIVE_PRE_BOOT_1_LEVEL);
}

void lrif_cpu0_pre_hw_init_hooks(void){
	run_native_tasks(_NATIVE_PRE_BOOT_2_LEVEL);
}

void lrif_cpu0_boot(void){
	run_native_tasks(_NATIVE_PRE_BOOT_3_LEVEL);
	posix_boot_cpu();
	run_native_tasks(_NATIVE_FIRST_SLEEP_LEVEL);
}

void lrif_cpu0_cleanup(void){
	posix_soc_clean_up();
}
