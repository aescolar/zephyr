/*
 * Copyright (c) 2018 Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bstests.h"

extern struct bst_test_list *test_empty_install(struct bst_test_list *tests);

bst_test_install_t test_installers[] = {
	test_empty_install,
	NULL
};

void main(void)
{
	bst_main();
}
