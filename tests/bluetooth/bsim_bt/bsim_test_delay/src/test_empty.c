/*
 * Copyright (c) 2018 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/kernel.h>
#include "bs_types.h"
#include "bs_tracing.h"
#include "time_machine.h"
#include "bstests.h"
#include "NRF_HWLowL.h"

/*
 * This is just a demo of the test framework facilities
 */

extern enum bst_result_t bst_result;

static void test_empty_init(void)
{
	bs_time_t time = hwll_phy_time_from_dev(tm_get_abs_time());
	bs_trace_info_time(0,"Init: It is %" PRItime "\n", time);
	bst_result = Passed;
	bst_ticker_set_next_tick_absolute(200e3);
}

static void test_empty_tick(bs_time_t HW_device_time)
{
	bs_time_t time = hwll_phy_time_from_dev(tm_get_abs_time());
	bs_trace_info_time(0,"tick: It is %" PRItime "\n", time);
}


static void test_main(void)
{
	bs_time_t time = hwll_phy_time_from_dev(tm_get_abs_time());
	bs_trace_info_time(0,"main: It is %" PRItime "\n", time);
}

static const struct bst_test_instance test_def[] = {
	{
		.test_id = "empty",
		.test_descr = "delay init test",
		.test_pre_init_f = test_empty_init,
		.test_tick_f = test_empty_tick,
		.test_main_f = test_main
	},
	BSTEST_END_MARKER
};

struct bst_test_list *test_empty_install(struct bst_test_list *tests)
{
	return bst_add_tests(tests, test_def);
}
