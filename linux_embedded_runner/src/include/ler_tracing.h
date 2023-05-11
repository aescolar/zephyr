/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LINUX_EMBEDDED_RUNNER_LER_TRACING_H
#define LINUX_EMBEDDED_RUNNER_LER_TRACING_H

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

void ler_print_error_and_exit(const char *format, ...);
void ler_print_warning(const char *format, ...);
void ler_print_trace(const char *format, ...);
void ler_vprint_error_and_exit(const char *format, va_list vargs);
void ler_vprint_warning(const char *format, va_list vargs);
void ler_vprint_trace(const char *format, va_list vargs);

int ler_trace_over_tty(int file_number);

#ifdef __cplusplus
}
#endif

#endif /* LINUX_EMBEDDED_RUNNER_LER_TRACING_H */
