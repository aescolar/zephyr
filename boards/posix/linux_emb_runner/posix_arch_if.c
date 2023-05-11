/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h> /* for exit */
#include <stdio.h>  /* for printfs */
#include <stdarg.h> /* for va args */
#include "cpu_ler_if.h"

/*
 * This file provides the interfaces the posix architecture and soc_inf
 * expect from all boards that use them
 */

void posix_exit(int exit_code)
{
	ler_exit(exit_code);
}

void posix_print_error_and_exit(const char *format, ...)
{
	va_list variable_args;

	va_start(variable_args, format);
	ler_vprint_error_and_exit(format, variable_args);
	va_end(variable_args);
}

void posix_print_warning(const char *format, ...)
{
	va_list variable_args;

	va_start(variable_args, format);
	ler_vprint_warning(format, variable_args);
	va_end(variable_args);
}

void posix_print_trace(const char *format, ...)
{
	va_list variable_args;

	va_start(variable_args, format);
	ler_vprint_trace(format, variable_args);
	va_end(variable_args);
}

int posix_trace_over_tty(int file_number)
{
	return ler_trace_over_tty(file_number);
}
