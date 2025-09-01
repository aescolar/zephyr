/*
 * Copyright (c) 2023, Meta
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <iostream>
#include <utility>
#include <cpplib.hpp>

int main(void)
{
	std::cout << "Hello, C++ world! " << CONFIG_BOARD << get_value() << std::endl;
	return 0;
}
