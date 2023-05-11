# SPDX-License-Identifier: Apache-2.0

set(LER_PATH ${ZEPHYR_BASE}/linux_embedded_runner)

zephyr_library()

zephyr_library_compile_definitions(NO_POSIX_CHEATS)

zephyr_library_sources(
	ler_if.c
	posix_arch_if.c
	
	hw_models_top.c
	timer_model.c
	native_rtc.c
	irq_handler.c
	irq_ctrl.c

	cmdline.c
	cpu_wait.c
	hw_counter.c
	)

zephyr_library_include_directories(
  ${LER_PATH}/src/include
  ${ZEPHYR_BASE}/kernel/include
  ${ZEPHYR_BASE}/arch/posix/include
  )

zephyr_compile_options(
  -fvisibility=hidden
  -ffreestanding
  -fno-builtin
)

#zephyr_compile_options(
#  -nostdinc
#)

zephyr_ld_options(
#  -lm
  -Wl,--unresolved-symbols=ignore-all
#  -r
)

set(LER_LINK_FLAGS
  "${LER_LINK_FLAGS} -lm"
)

if(CONFIG_EXTERNAL_LIBCPP)
	set(LER_LINK_FLAGS
	  "${LER_LINK_FLAGS} -lstdc++"
	)
endif()

if(CONFIG_HAS_SDL)
	find_package(PkgConfig REQUIRED)
	pkg_search_module(SDL2 REQUIRED sdl2)
	zephyr_include_directories(${SDL2_INCLUDE_DIRS})
	set(LER_LINK_FLAGS
	  "${LER_LINK_FLAGS} ${SDL2_LINK_LIBRARIES}"
	)
	zephyr_compile_options(${SDL2_CFLAGS_OTHER})
	zephyr_library_sources(sdl_events.c)
endif()

#EMBEDDED_BUILD_FOLDER to be deleted from below (we should only need the EMBEDDED_CPU_SW)
#ZEPHYR_BASE to be deleted from below
set_property(GLOBAL APPEND PROPERTY extra_post_build_commands
  COMMAND
  export LER_LINK_FLAGS=${LER_LINK_FLAGS}
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
