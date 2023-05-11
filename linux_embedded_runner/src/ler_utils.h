/*
 * Copyright (c) 2010-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LINUX_EMBEDDED_RUNNER_LER_UTILS_H
#define LINUX_EMBEDDED_RUNNER_LER_UTILS_H


#define _STRINGIFY(x) #x
#define STRINGIFY(s) _STRINGIFY(s)

/* concatenate the values of the arguments into one */
#define _DO_CONCAT(x, y) x ## y
#define _CONCAT(x, y) _DO_CONCAT(x, y)

#ifndef ARG_UNUSED
#define ARG_UNUSED(x) (void)(x)
#endif

#endif /* LINUX_EMBEDDED_RUNNER_LER_UTILS_H */
