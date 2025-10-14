/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stddef.h>
#include <nsi_cpu_if.h>

NATIVE_SIMULATOR_IF void banana(void) {
	int *a = NULL;
	*a = 5;
}

int main(void)
{
	void nsi_segfault_register(void);
	nsi_segfault_register();

	banana();

	printf("Hello World! %s\n", CONFIG_BOARD_TARGET);

	return 0;
}
