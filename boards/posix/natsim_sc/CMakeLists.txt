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

zephyr_compile_options(
  -fvisibility=hidden
  -ffreestanding
  -fno-builtin
)

if(NOT CONFIG_EXTERNAL_LIBC)
  zephyr_compile_options(
    -nostdinc
    -isystem /usr/lib/gcc/x86_64-linux-gnu/12/include
  )
  #TODO: The compiler headers hardcoded include is a quick hack that needs replacing with something calling gcc instead
  # gcc -xc -E -Wp,-v /dev/null 2>&1 | grep gcc | grep -v ignoring
endif()

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
	set(NSI_BUILD_OPTIONS "${NSI_BUILD_OPTIONS} -m64"  CACHE INTERNAL " ")
endif()


if(CONFIG_HAS_SDL)
	add_subdirectory(${ZEPHYR_BASE}/boards/${ARCH}/common/sdl/ ${CMAKE_CURRENT_BINARY_DIR}/sdl)
endif()

#EMBEDDED_BUILD_FOLDER to be deleted from below (we should only need the EMBEDDED_CPU_SW)
#ZEPHYR_BASE to be deleted from below
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
  ${ZEPHYR_BASE}/boards/posix/natsim_sc/finnish_build.sh
)

set_property(GLOBAL APPEND PROPERTY
    extra_post_build_byproducts
    ${KERNEL_EXE_NAME}
)

# Override the C standard used for compilation to C 2011
# This is due to some tests using _Static_assert which is a 2011 feature, but
# otherwise relying on compilers supporting it also when set to C99.
# This was in general ok, but with some host compilers and C library versions
# it led to problems. So we override it to 2011 for native_posix.
set_property(GLOBAL PROPERTY CSTD c11)
