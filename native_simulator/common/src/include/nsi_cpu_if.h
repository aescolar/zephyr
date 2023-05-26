/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Interfaces the Native Simulator expects from
 * each embedded CPU
 */

#ifndef NSI_COMMON_SRC_INCL_NSI_CPU_IF_H
#define NSI_COMMON_SRC_INCL_NSI_CPU_IF_H

#ifdef __cplusplus
extern "C" {
#endif

#define NATIVE_SIMULATOR_IF __attribute__((visibility ("default"))) __attribute__((used))

NATIVE_SIMULATOR_IF void nsif_cpu0_pre_cmdline_hooks(void);
NATIVE_SIMULATOR_IF void nsif_cpu0_pre_hw_init_hooks(void);
NATIVE_SIMULATOR_IF void nsif_cpu0_boot(void);
NATIVE_SIMULATOR_IF void nsif_cpu0_cleanup(void);
NATIVE_SIMULATOR_IF void nsif_cpu0_irq_raised(void);
NATIVE_SIMULATOR_IF void nsif_cpu0_irq_raised_from_sw(void);

#ifdef __cplusplus
}
#endif

#endif /* NSI_COMMON_SRC_INCL_NSI_CPU_IF_H */
