/*
 * Copyright (c) 2023 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Bottom for the fake native entropy generator.
 * Here we keep all accesses to the host OS/libC
 *
 * Note that this file is compiled outside of Zephyr scope when
 * building for the native simulator
 */

#include <stdlib.h>

long fenb_host_random(void)
{
	return random();
}

void fenb_host_srandom(unsigned int seed)
{
	srandom(seed);
}
