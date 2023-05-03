/*
 * Copyright (c) 2017 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _POSIX_SOC_INF_CLOCK_SOC_H
#define _POSIX_SOC_INF_CLOCK_SOC_H

#include <zephyr/toolchain.h>
#include "board_soc.h"
#include "posix_soc.h"
#include "posix_native_task.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LINUX_RUNNER_IF __attribute__ ((visibility ("default"))) /* __attribute__((used))*/

LINUX_RUNNER_IF void posix_soc_clean_up(void);


#ifdef __cplusplus
}
#endif

#endif /* _POSIX_SOC_INF_CLOCK_SOC_H */
