/*
 * Copyright 2026 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <nsi_tracing.h>

int ns_i2c_open_file_bottom(const char *path, bool read)
{
	int fd;

	if (read) {
		fd = open(path, O_RDONLY);
	} else {
		fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0600);
	}

	if (fd < 0) {
		nsi_print_warning("%s could not be opened (%s)\n", path, strerror(errno));
	}

	return fd;
}
