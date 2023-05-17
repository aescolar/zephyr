# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_compile_definitions(NO_POSIX_CHEATS)

zephyr_library_sources(
	soc.c
	native_tasks.c
	)

zephyr_library_include_directories(
  ${ZEPHYR_BASE}/kernel/include
  ${ZEPHYR_BASE}/arch/posix/include
  )

if(CONFIG_NATIVE_APPLICATION)
	set(LER_PATH ${ZEPHYR_BASE}/linux_embedded_runner)

	zephyr_library_sources(
		${LER_PATH}/src/lce.c
	)
	zephyr_library_include_directories(
		${LER_PATH}/src/include
	)
endif()
