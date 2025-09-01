#include "cpplib.hpp"

#include <utility>

// This won't work because of using STL/templated functions:
int get_value() {
	int value = 42;
	return std::move(value);
}

// This would work fine if uncommented:
//int get_value() {
//	return 42;
//}
