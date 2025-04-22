/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

char a[10];

int main(void)
{
	printf("Hello World! %s\n", CONFIG_BOARD_TARGET);
	printf("%i\n", *(int*)&a[1]);

	return 0;
}
