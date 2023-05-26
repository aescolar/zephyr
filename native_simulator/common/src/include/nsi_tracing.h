/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef NSI_COMMON_SRC_INCL_NSI_TRACING_H
#define NSI_COMMON_SRC_INCL_NSI_TRACING_H

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

void nsi_print_error_and_exit(const char *format, ...);
void nsi_print_warning(const char *format, ...);
void nsi_print_trace(const char *format, ...);
void nsi_vprint_error_and_exit(const char *format, va_list vargs);
void nsi_vprint_warning(const char *format, va_list vargs);
void nsi_vprint_trace(const char *format, va_list vargs);
int nsi_trace_over_tty(int file_number);

#ifdef __cplusplus
}
#endif

#endif /* NSI_COMMON_SRC_INCL_NSI_TRACING_H */
