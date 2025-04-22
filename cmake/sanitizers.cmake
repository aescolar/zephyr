
if(CONFIG_ASAN)
  list(APPEND LLVM_SANITIZERS "address")
endif()

if(CONFIG_MSAN)
  list(APPEND LLVM_SANITIZERS "memory")
endif()

if(CONFIG_UBSAN)
  zephyr_compile_options($<$<COMPILE_LANGUAGE:C>:$<TARGET_PROPERTY:compiler,sanitizer_undefined>>)
  zephyr_link_libraries($<TARGET_PROPERTY:linker,sanitizer_undefined>)

  if (CONFIG_NATIVE_LIBRARY)
    target_compile_options(native_simulator INTERFACE $<TARGET_PROPERTY:compiler,sanitizer_undefined>)
    target_link_options(native_simulator INTERFACE $<TARGET_PROPERTY:linker,sanitizer_undefined>)
  endif()

  if(CONFIG_UBSAN_LIBRARY)
    zephyr_compile_options($<$<COMPILE_LANGUAGE:C>:$<TARGET_PROPERTY:compiler,sanitizer_undefined_library>>)
    zephyr_link_libraries($<TARGET_PROPERTY:linker,sanitizer_undefined_library>)
  elseif(CONFIG_UBSAN_TRAP)
    zephyr_compile_options($<$<COMPILE_LANGUAGE:C>:$<TARGET_PROPERTY:compiler,sanitizer_undefined_trap>>)
    zephyr_link_libraries($<TARGET_PROPERTY:linker,sanitizer_undefined_trap>)
  endif()
endif()

if(CONFIG_ASAN_RECOVER)
  zephyr_compile_options(-fsanitize-recover=all)
  target_compile_options(native_simulator INTERFACE "-fsanitize-recover=all")
endif()

if(CONFIG_ARCH_POSIX_LIBFUZZER)
  list(APPEND LLVM_SANITIZERS "fuzzer")
  target_compile_options(native_simulator INTERFACE "-DNSI_NO_MAIN=1")
  if(NOT CONFIG_64BIT)
    # On i386, libfuzzer seems to dynamically relocate the binary, so
    # we need to emit PIC code.  This limitation is undocumented and
    # poorly understood...
    zephyr_compile_options(-fPIC)
  endif()
endif()

list(JOIN LLVM_SANITIZERS "," LLVM_SANITIZERS_ARG)
if(NOT ${LLVM_SANITIZERS_ARG} STREQUAL "")
  set(LLVM_SANITIZERS_ARG "-fsanitize=${LLVM_SANITIZERS_ARG}")
  zephyr_compile_options("${LLVM_SANITIZERS_ARG}")
  if (CONFIG_NATIVE_APPLICATION)
    zephyr_link_libraries("${LLVM_SANITIZERS_ARG}")
  endif()

  if (CONFIG_NATIVE_LIBRARY)
    target_link_options(native_simulator INTERFACE ${LLVM_SANITIZERS_ARG})
    target_compile_options(native_simulator INTERFACE ${LLVM_SANITIZERS_ARG})
  endif()
endif()
