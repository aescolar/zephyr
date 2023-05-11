/*
 * Copyright (c) 2010-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LINUX_EMBEDDED_RUNNER_LER_UTILS_H
#define LINUX_EMBEDDED_RUNNER_LER_UTILS_H


#define _LER_STRINGIFY(x) #x
#define LER_STRINGIFY(s) _LER_STRINGIFY(s)

/* concatenate the values of the arguments into one */
#define LER_DO_CONCAT(x, y) x ## y
#define LER_CONCAT(x, y) LER_DO_CONCAT(x, y)

#define L_MAX(a,b)  (((a) > (b)) ? (a) : (b))
#define L_MIN(a, b) (((a) < (b)) ? (a) : (b))

#ifndef ARG_UNUSED
#define ARG_UNUSED(x) (void)(x)
#endif

#endif /* LINUX_EMBEDDED_RUNNER_LER_UTILS_H */
