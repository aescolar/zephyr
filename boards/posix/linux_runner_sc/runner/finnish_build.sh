#! /usr/bin/env bash

echo "** Entered Finnish_build.sh **"

ARCH="-m32"
COVERAGE_COMP=--coverage
COVERAGE_LINK=--coverage
C_FLAGS="-g $COVERAGE_COMP $ARCH -fno-pie -fno-pic -no-pie -ffunction-sections -fdata-sections"
C_FLAGS_FINALLINK="$C_FLAGS -Wl,--gc-sections -lm"

set -x 
#objcopy --localize-hidden  zephyr/zephyr.elf zephyr/zephyr.post.elf -w --localize-symbols=${ZEPHYR_BASE}/boards/posix/linux_runner_sc/linker_symbols_to_localize
objcopy --localize-hidden zephyr.elf cpu_0.sw.o -w --localize-symbol=_*

gcc $C_FLAGS -c ${ZEPHYR_BASE}/boards/posix/linux_runner_sc/runner/main.c -o runner.o -I${ZEPHYR_BASE}/include -I${ZEPHYR_BASE}/soc/posix/inf_clock/ -I${ZEPHYR_BASE}/boards/posix/linux_runner_sc/ -include include/generated/autoconf.h

gcc runner.o cpu_0.sw.o -o linux_runner.exe $C_FLAGS_FINALLINK
