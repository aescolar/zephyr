/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef LINUX_EMBEDDED_RUNNER_LCE_IF_H
#define LINUX_EMBEDDED_RUNNER_LCE_IF_H

#ifdef __cplusplus
extern "C" {
#endif

/* Linux embedded runner CPU start/stop emulation */

void *lce_init(void);
void lce_terminate(void *this);
void lce_boot_cpu(void *this, void (*start_routine)(void));
void lce_halt_cpu(void *this);
void lce_wake_cpu(void *this);
int lce_is_cpu_running(void *this);

#ifdef __cplusplus
}
#endif

#endif /* LINUX_EMBEDDED_RUNNER_LCE_IF_H */
