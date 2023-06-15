# SPDX-License-Identifier: Apache-2.0

# Configures CMake for using GCC

find_program(CMAKE_C_COMPILER gcc)

if(CONFIG_CPP)
  set(cplusplus_compiler g++)
else()
  if(EXISTS g++)
    set(cplusplus_compiler g++)
  else()
    # When the toolchain doesn't support C++, and we aren't building
    # with C++ support just set it to something so CMake doesn't
    # crash, it won't actually be called
    set(cplusplus_compiler ${CMAKE_C_COMPILER})
  endif()
endif()
find_program(CMAKE_CXX_COMPILER ${cplusplus_compiler}     CACHE INTERNAL " " FORCE)

set(NOSTDINC "")

# Note that NOSYSDEF_CFLAG may be an empty string, and
# set_ifndef() does not work with empty string.
if(NOT DEFINED NOSYSDEF_CFLAG)
  set(NOSYSDEF_CFLAG -undef)
endif()

foreach(file_name include/stddef.h)
  execute_process(
    COMMAND ${CMAKE_C_COMPILER} --print-file-name=${file_name}
    OUTPUT_VARIABLE _OUTPUT
    )
  get_filename_component(_OUTPUT "${_OUTPUT}" DIRECTORY)
  string(REGEX REPLACE "\n" "" _OUTPUT "${_OUTPUT}")

  list(APPEND NOSTDINC ${_OUTPUT})
endforeach()

if(CONFIG_NATIVE_LIBRARY AND NOT CONFIG_EXTERNAL_LIBC)
	# Get the *compiler* include path, that is where the *compiler* provided headers are (not the
	# default libC ones). This includes basic headers like stdint.h, stddef.h or float.h
	# We expect something like
	#  /usr/lib/gcc/x86_64-linux-gnu/12/include or /usr/lib/llvm-14/lib/clang/14.0.0/include
	cmake_path(GET CMAKE_C_COMPILER FILENAME CC)
	execute_process(
		COMMAND ${CMAKE_C_COMPILER} -xc -E -Wp,-v ${ZEPHYR_BASE}/misc/empty_file.c
		ERROR_VARIABLE COMPILER_OWN_INCLUDE_PATH
		OUTPUT_QUIET
		COMMAND_ERROR_IS_FATAL ANY
	)
	string(REPLACE "\n" ";" COMPILER_OWN_INCLUDE_PATH "${COMPILER_OWN_INCLUDE_PATH}")
	list(FILTER COMPILER_OWN_INCLUDE_PATH INCLUDE REGEX " /.*${CC}")
	string(STRIP ${COMPILER_OWN_INCLUDE_PATH} COMPILER_OWN_INCLUDE_PATH)
endif()
