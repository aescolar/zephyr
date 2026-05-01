/*
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <nsi_tracing.h>

int ns_dmic_open_file_bottom(const char *path)
{
	int fd = open(path, O_RDONLY);

	if (fd < 0) {
		nsi_print_warning("%s could not be opened (%s)\n", path, strerror(errno));
	}
	return fd;
}
