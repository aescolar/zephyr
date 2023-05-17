# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_compile_definitions(NO_POSIX_CHEATS)
zephyr_library_sources(
	cpuhalt.c
	fatal.c
	irq.c
	swap.c
	thread.c
	posix_core.c
	)

if(CONFIG_NATIVE_APPLICATION)
	set(LER_PATH ${ZEPHYR_BASE}/linux_embedded_runner)

	zephyr_library_sources(
		ler_compat.c
		${LER_PATH}/src/lct.c
	)
	zephyr_library_include_directories(
		${LER_PATH}/src/include
	)
endif()