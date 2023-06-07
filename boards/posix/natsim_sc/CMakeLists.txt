# SPDX-License-Identifier: Apache-2.0

set(NSI_PATH ${ZEPHYR_BASE}/native_simulator)

zephyr_library()

zephyr_library_compile_definitions(NO_POSIX_CHEATS)

zephyr_library_sources(
	cmdline.c
	cpu_wait.c
	nsi_if.c
	irq_handler.c
	misc.c
	posix_arch_if.c
	)

zephyr_include_directories(
  ${NSI_PATH}/common/src/include
  ${NSI_PATH}/native/src/include
)

zephyr_library_include_directories(
  ${ZEPHYR_BASE}/kernel/include
  ${ZEPHYR_BASE}/arch/posix/include
)

if(NOT CONFIG_EXTERNAL_LIBC)
	# Get the *compiler* include path, that is where the *compiler* provided headers are (not the
	# default libC ones). This includes basic headers like stdint.h, stddef.h or float.h
	# We expect something like
	#  /usr/lib/gcc/x86_64-linux-gnu/12/include or /usr/lib/llvm-14/lib/clang/14.0.0/include
	cmake_path(GET CMAKE_C_COMPILER FILENAME CC)
	execute_process(
		COMMAND bash -c "${CMAKE_C_COMPILER} -xc -E -Wp,-v /dev/null 2>&1 | grep ' /.*${CC}'"
		OUTPUT_VARIABLE COMPILER_OWN_INCLUDE_PATH
		COMMAND_ERROR_IS_FATAL ANY
	)
	string(STRIP ${COMPILER_OWN_INCLUDE_PATH} COMPILER_OWN_INCLUDE_PATH)
	#message(STATUS "COMPILER_OWN_INCLUDE_PATH = ${COMPILER_OWN_INCLUDE_PATH}")

	# Do not use the default standard library for this compiler,
	# but still use the basic compiler headers
	zephyr_compile_options(
		-nostdinc
		-isystem ${COMPILER_OWN_INCLUDE_PATH}
		-ffreestanding
		-fno-builtin
	)
endif()

zephyr_compile_options(
  -fvisibility=hidden
)

zephyr_ld_options(
  -Wl,--unresolved-symbols=ignore-all
#  -r
)

if(CONFIG_EXTERNAL_LIBCPP)
	set(NSI_LINK_OPTIONS "${NSI_LINK_OPTIONS} -lstdc++" CACHE INTERNAL " ")
endif()

if(CONFIG_GPROF)
	set(NSI_LINK_OPTIONS "${NSI_LINK_OPTIONS} -pg"  CACHE INTERNAL " ")
endif()

if(CONFIG_64BIT)
  # By default the linux embedded runner builds and links a 32bits (x86) executable
  # we override it to build a 64 bit one
	set(NSI_LINK_OPTIONS "${NSI_LINK_OPTIONS} -m64" CACHE INTERNAL " ")
	set(NSI_BUILD_OPTIONS "${NSI_BUILD_OPTIONS} -m64" CACHE INTERNAL " ")
endif()

if(CONFIG_HAS_SDL)
	add_subdirectory(${ZEPHYR_BASE}/boards/${ARCH}/common/sdl/ ${CMAKE_CURRENT_BINARY_DIR}/sdl)
endif()

set_property(GLOBAL APPEND PROPERTY extra_post_build_commands
  COMMAND
  export NSI_LINK_OPTIONS=${NSI_LINK_OPTIONS}
  COMMAND
  export NSI_BUILD_OPTIONS=${NSI_BUILD_OPTIONS}
  COMMAND
  export NSI_BUILD_PATH=${CMAKE_BINARY_DIR}/zephyr
  COMMAND
  export NSI_EMBEDDED_CPU_SW=${CMAKE_BINARY_DIR}/zephyr/zephyr.elf 
  COMMAND
  export NSI_EXE=${CMAKE_BINARY_DIR}/zephyr/${KERNEL_EXE_NAME}
  COMMAND
  export NSI_BUILD_PATH=${CMAKE_BINARY_DIR}/zephyr/ES
  COMMAND
  export NSI_PATH=${NSI_PATH}
  COMMAND
  export NSI_CC=${CMAKE_C_COMPILER}
  COMMAND
  ${ZEPHYR_BASE}/boards/posix/natsim_sc/finnish_build.sh
)

set_property(GLOBAL APPEND PROPERTY
    extra_post_build_byproducts
    ${KERNEL_EXE_NAME}
)
