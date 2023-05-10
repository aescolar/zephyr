/*
 * Copyright (c) 2018 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * To support native_posix drivers or tests which register their own arguments
 * we provide the same API as in native_posix
 */

#include "ler_cmdline.h"
#include "ler_cmdline_common.h"

void native_add_command_line_opts(struct args_struct_t *args)
{
	ler_add_command_line_opts(args);
}

void native_get_cmd_line_args(int *argc, char ***argv)
{
	ler_get_cmd_line_args(argc, argv);
}

void native_get_test_cmd_line_args(int *argc, char ***argv)
{
	ler_get_test_cmd_line_args(argc, argv);
}
