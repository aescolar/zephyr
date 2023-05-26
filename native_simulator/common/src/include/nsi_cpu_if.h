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

#define NATIVE_SIMULATOR_IF __attribute__((visibility("default"))) \
	__attribute__((__section__(".native_sim_if")))
/*
 * Implementation note:
 * The interface between the embedded SW and the native simulator is allocated in its
 * own section to allow the embedded software developers to, using a linker script,
 * direct the linker to keep those symbols even when doing its linking with garbage collection.
 * It is also be possible for the embedded SW to require the linker to keep those
 * symbols by requiring each of them to be kept explicitly by name (either by defining them
 * as entry points, or as required in the output).
 * It is also possible for the embedded SW developers to not use garbage collection
 * during their SW linking.
 */

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
