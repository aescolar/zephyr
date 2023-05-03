#! /usr/bin/env bash

ARCH="-m32"
COVERAGE_COMP=--coverage
COVERAGE_LINK=--coverage
C_FLAGS="-g $COVERAGE_COMP $ARCH -fno-pie -fno-pic -no-pie"
C_FLAGS_RELINK="$C_FLAGS -Wl,--gc-sections"

#objcopy --localize-hidden  zephyr/zephyr.elf zephyr/zephyr.post.elf -w --localize-symbols=${ZEPHYR_BASE}/boards/posix/linux_runner_sc/linker_symbols_to_localize
objcopy --localize-hidden zephyr/zephyr.elf cpu_0.sw.o -w --localize-symbol=_*

gcc $C_FLAGS -c ${ZEPHYR_BASE}/boards/posix/linux_runner_sc/main.c -o runner.o -I../include -I${ZEPHYR_BASE}/soc/posix/inf_clock/ -I${ZEPHYR_BASE}/boards/posix/linux_runner_sc/ -include zephyr/include/generated/autoconf.h

#echo "#Final:"
gcc runner.o cpu_0.sw.o -o zephyr.exe $C_FLAGS_RELINK
