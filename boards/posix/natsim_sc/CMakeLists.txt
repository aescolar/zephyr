# SPDX-License-Identifier: Apache-2.0

set(NSI_PATH ${ZEPHYR_BASE}/native_simulator/)

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
  ${NSI_PATH}common/src/include
  ${NSI_PATH}native/src/include
)

zephyr_library_include_directories(
  ${ZEPHYR_BASE}/kernel/include
  ${ZEPHYR_BASE}/arch/posix/include
)

if(NOT CONFIG_EXTERNAL_LIBC)
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
)

if(CONFIG_EXTERNAL_LIBCPP)
  target_link_options(native_simulator INTERFACE "-lstdc++")
endif()

if(CONFIG_GPROF)
  target_link_options(native_simulator INTERFACE "-pg")
endif()

if(CONFIG_64BIT)
  # By default the linux embedded runner builds and links a 32bits (x86) executable
  # we override it to build a 64 bit one
  target_link_options(native_simulator INTERFACE "-m64")
  target_compile_options(native_simulator INTERFACE "-m64")
endif()

if(CONFIG_HAS_SDL)
  add_subdirectory(${ZEPHYR_BASE}/boards/${ARCH}/common/sdl/ ${CMAKE_CURRENT_BINARY_DIR}/sdl)
endif()

set(zephyr_build_path ${CMAKE_BINARY_DIR}/zephyr)

set(nsi_config_content 
  "NSI_LINK_OPTIONS:=$<TARGET_PROPERTY:native_simulator,INTERFACE_LINK_OPTIONS>"
  "NSI_BUILD_OPTIONS:=$<TARGET_PROPERTY:native_simulator,INTERFACE_COMPILE_OPTIONS>"
  "NSI_BUILD_PATH:=${zephyr_build_path}"
  "NSI_EMBEDDED_CPU_SW:=${zephyr_build_path}/${KERNEL_ELF_NAME}"
  "NSI_EXE:=${zephyr_build_path}/${KERNEL_EXE_NAME}"
  "NSI_BUILD_PATH:=${zephyr_build_path}/NSI"
  "NSI_CC:=${CMAKE_C_COMPILER}"
  "NSI_PATH:=${NSI_PATH}"
  #"NSI_BUILD_VERBOSE:=1"
)

string(REPLACE ";" "\n" nsi_config_content "${nsi_config_content}")

file(GENERATE OUTPUT "${zephyr_build_path}/NSI/nsi_config"
  CONTENT "${nsi_config_content}"
)
