/*
 * Copyright (c) 2018 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LINUX_EMBEDDED_RUNNER_LER_CMDLINE_H
#define LINUX_EMBEDDED_RUNNER_LER_CMDLINE_H

#include "ler_cmdline_common.h"

#ifdef __cplusplus
extern "C" {
#endif

void ler_handle_cmd_line(int argc, char *argv[]);
void ler_get_cmd_line_args(int *argc, char ***argv);
void ler_get_test_cmd_line_args(int *argc, char ***argv);
void ler_add_command_line_opts(struct args_struct_t *args);
void ler_cleanup_cmd_line(void);

#ifdef __cplusplus
}
#endif

#endif /* LINUX_EMBEDDED_RUNNER_LER_CMDLINE_H */
