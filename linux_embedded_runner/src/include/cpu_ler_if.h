/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Interfaces the Linux Embedded Runner provides from
 * each embedded CPU
 */

#ifndef LINUX_EMBEDDED_RUNNER_LER_CPU_IF_H
#define LINUX_EMBEDDED_RUNNER_LER_CPU_IF_H

#include "ler_tracing.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Terminate the execution
 *
 * exit_code: Requested exit code to the shell
 *            Note that other components may have requested a different
 *            exit code which may have precedence if it was !=0
 */
void ler_exit(int exit_code);

#ifdef __cplusplus
}
#endif

#endif /* LINUX_EMBEDDED_RUNNER_LER_CPU_IF_H */
