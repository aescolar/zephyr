/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Interface the Linux Embedded Runner expects from
 * each the embedded CPU
 */

#ifndef LINUX_EMBEDDED_RUNNER_LER_CPU_IF_H
#define LINUX_EMBEDDED_RUNNER_LER_CPU_IF_H

#ifdef __cplusplus
extern "C" {
#endif

#define LINUX_RUNNER_IF __attribute__ ((visibility ("default"))) __attribute__((used))

LINUX_RUNNER_IF void lrif_cpu0_pre_cmdline_hooks(void);
LINUX_RUNNER_IF void lrif_cpu0_pre_hw_init_hooks(void);
LINUX_RUNNER_IF void lrif_cpu0_boot(void);
LINUX_RUNNER_IF void lrif_cpu0_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* LINUX_EMBEDDED_RUNNER_LER_CPU_IF_H */
