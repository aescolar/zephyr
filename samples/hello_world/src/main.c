/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <hal/nrf_cracen.h>
#include <hal/nrf_cracen_rng.h>
#include <nrfx_cracen.h>

static uint32_t buf[64/4];

static uint64_t t1 = 0;
extern uint64_t sys_clock_cycle_get_64(void);
void set_t_ref(void) {
	t1 = sys_clock_cycle_get_64();
}

void print_t_delta(void) {
	uint64_t t2 = sys_clock_cycle_get_64();
	printf("%llu\n", t2 - t1);
	t1 = t2;
}

int main(void)
{
	printf("Hello World! %s\n", CONFIG_BOARD_TARGET);

	nrf_cracen_module_enable(NRF_CRACEN, NRF_CRACEN_MODULE_RNG_MASK);

	nrfx_cracen_trng_init();
	set_t_ref();
	nrfx_cracen_trng_entropy_get((uint8_t*)buf, 64);
	print_t_delta();
	nrfx_cracen_trng_uninit();

	nrf_cracen_module_disable(NRF_CRACEN, NRF_CRACEN_MODULE_RNG_MASK);

	for (int i = 0; i < 64/4; i++) {
		printf("0x%08X\n", buf[i]);
	}

	return 0;
}
