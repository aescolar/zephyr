# SPDX-License-Identifier: Apache-2.0

set(LER_PATH ${ZEPHYR_BASE}/linux_embedded_runner)

zephyr_library()

zephyr_library_compile_definitions(NO_POSIX_CHEATS)

zephyr_library_sources(
	ler_if.c
	posix_arch_if.c
	cmdline.c
	cpu_wait.c
	irq_handler.c
	)

zephyr_include_directories(
  ${LER_PATH}/src/include
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
    -I/usr/lib/gcc/x86_64-linux-gnu/12/include
  )
  #TODO: The compiler headers hardcoded include is a quick hack that needs replacing with something calling gcc instead
  # gcc -xc -E -Wp,-v /dev/null 2>&1 | grep gcc | grep -v ignoring
endif()

zephyr_ld_options(
#  -lm
#  -Wl,--unresolved-symbols=ignore-all
  -r
)

set(LER_LINK_OPTIONS
  "${LER_LINK_OPTIONS} -lm"
)

if(CONFIG_EXTERNAL_LIBCPP)
	set(LER_LINK_OPTIONS
	  "${LER_LINK_OPTIONS} -lstdc++"
	)
endif()

if(CONFIG_GPROF)
	set(LER_LINK_OPTIONS
	  "${LER_LINK_OPTIONS} -pg"
	)
endif()

if(CONFIG_64BIT)
  # By default the linux embedded runner builds and links a 32bits (x86) executable
  # we override it to build a 64 bit one
	set(LER_LINK_OPTIONS
	  "${LER_LINK_OPTIONS} -m64"
	)
	set(LER_BUILD_OPTIONS
	  "${LER_BUILD_OPTIONS} -m64"
	)
endif()


if(CONFIG_HAS_SDL)
	find_package(PkgConfig REQUIRED)
	pkg_search_module(SDL2 REQUIRED sdl2)
	zephyr_include_directories(${SDL2_INCLUDE_DIRS})
	set(LER_LINK_OPTIONS
	  "${LER_LINK_OPTIONS} ${SDL2_LINK_LIBRARIES}"
	)
	zephyr_compile_options(${SDL2_CFLAGS_OTHER})
	zephyr_library_sources(sdl_events.c)
endif()

#EMBEDDED_BUILD_FOLDER to be deleted from below (we should only need the EMBEDDED_CPU_SW)
#ZEPHYR_BASE to be deleted from below
set_property(GLOBAL APPEND PROPERTY extra_post_build_commands
  COMMAND
  export LER_LINK_OPTIONS=${LER_LINK_OPTIONS}
  COMMAND
  export LER_BUILD_OPTIONS=${LER_BUILD_OPTIONS}
  COMMAND
  export EMBEDDED_BUILD_FOLDER=${CMAKE_BINARY_DIR}/zephyr
  COMMAND
  export EMBEDDED_CPU_SW=${CMAKE_BINARY_DIR}/zephyr/zephyr.elf 
  COMMAND
  export LER_EXE=${CMAKE_BINARY_DIR}/zephyr/${KERNEL_EXE_NAME}
  COMMAND
  export LER_BUILD_PATH=${CMAKE_BINARY_DIR}/zephyr/LER
  COMMAND
  export LER_PATH=${LER_PATH}
  COMMAND
  export ZEPHYR_BASE=${ZEPHYR_BASE}
  COMMAND
  ${ZEPHYR_BASE}/boards/posix/linux_emb_runner/finnish_build.sh
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
