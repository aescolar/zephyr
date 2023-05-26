/*
 * Copyright (c) 2018 Oticon A/S
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NATIVE_SIMULATOR_ES_CMDLINE_H
#define NATIVE_SIMULATOR_ES_CMDLINE_H

#include "nsi_cmdline_common.h"

#ifdef __cplusplus
extern "C" {
#endif

void nsi_handle_cmd_line(int argc, char *argv[]);
void nsi_get_cmd_line_args(int *argc, char ***argv);
void nsi_get_test_cmd_line_args(int *argc, char ***argv);
void nsi_add_command_line_opts(struct args_struct_t *args);
void nsi_cleanup_cmd_line(void);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_SIMULATOR_ES_CMDLINE_H */
