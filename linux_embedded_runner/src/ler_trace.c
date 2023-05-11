/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h> /* for exit */
#include <stdio.h>  /* for printfs */
#include <stdarg.h> /* for va args */
#include <unistd.h>
#include "cpu_ler_if.h"
#include "ler_tasks.h"
#include "ler_cmdline.h"

void ler_vprint_error_and_exit(const char *format, va_list vargs)
{
	vfprintf(stderr, format, vargs);
	ler_exit(1);
}

void ler_vprint_warning(const char *format, va_list vargs)
{
	vfprintf(stderr, format, vargs);
}

void ler_vprint_trace(const char *format, va_list vargs)
{
	vfprintf(stdout, format, vargs);
}

void ler_print_error_and_exit(const char *format, ...)
{
	va_list variable_args;

	va_start(variable_args, format);
	ler_vprint_error_and_exit(format, variable_args);
	va_end(variable_args);
}

void ler_print_warning(const char *format, ...)
{
	va_list variable_args;

	va_start(variable_args, format);
	vfprintf(stderr, format, variable_args);
	va_end(variable_args);
}

void ler_print_trace(const char *format, ...)
{
	va_list variable_args;

	va_start(variable_args, format);
	vfprintf(stdout, format, variable_args);
	va_end(variable_args);
}


/**
 * Are stdout and stderr connected to a tty
 * 0  = no
 * 1  = yes
 * -1 = we do not know yet
 * Indexed 0:stdout, 1:stderr
 */
static int is_a_tty[2] = {-1, -1};

void trace_disable_color(char *argv, int offset)
{
	is_a_tty[0] = 0;
	is_a_tty[1] = 0;
}

void trace_enable_color(char *argv, int offset)
{
	is_a_tty[0] = -1;
	is_a_tty[1] = -1;

}

void trace_force_color(char *argv, int offset)
{
	is_a_tty[0] = 1;
	is_a_tty[1] = 1;
}

int ler_trace_over_tty(int file_number)
{
	return is_a_tty[file_number];
}

static void decide_about_color(void)
{
	if (is_a_tty[0] == -1) {
		is_a_tty[0] = isatty(STDOUT_FILENO);
	}
	if (is_a_tty[1] == -1) {
		is_a_tty[1] = isatty(STDERR_FILENO);
	}
}

LER_TASK(decide_about_color, PRE_BOOT_2, 0);

static void ler_add_tracing_options(void)
{
	static struct args_struct_t trace_options[] = {
		/*
		 * Fields:
		 * manual, mandatory, switch,
		 * option_name, var_name ,type,
		 * destination, callback,
		 * description
		 */
		{ false, false, true,
		"color", "color", 'b',
		NULL, trace_enable_color,
		"(default) Enable color in traces if printing to console"},
		{ false, false, true,
		"no-color", "no-color", 'b',
		NULL, trace_disable_color,
		"Disable color in traces even if printing to console"},
		{ false, false, true,
		"force-color", "force-color", 'b',
		NULL, trace_force_color,
		"Enable color in traces even if printing to files/pipes"},
		ARG_TABLE_ENDMARKER};

	ler_add_command_line_opts(trace_options);
}

LER_TASK(ler_add_tracing_options, PRE_BOOT_1, 0);
