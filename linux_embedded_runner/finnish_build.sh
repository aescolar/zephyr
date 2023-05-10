#! /usr/bin/env bash

echo "** Entered Finnish_build.sh **"

ARCH="-m32"
COVERAGE_COMP=--coverage
COVERAGE_LINK=--coverage
C_FLAGS="-g $COVERAGE_COMP $ARCH -fno-pie -fno-pic -no-pie -ffunction-sections -fdata-sections"
C_FLAGS_FINALLINK="$C_FLAGS -Wl,--gc-sections ${LINUX_RUNNER_LINK_OPTIONS}"

set -x 
objcopy --localize-hidden zephyr.elf cpu_0.sw.o -w --localize-symbol=_*

INCLUDES="
 -I${ZEPHYR_BASE}/linux_embedded_runner/src/include \
 \
 -I${ZEPHYR_BASE}/include \
 -I${ZEPHYR_BASE}/soc/posix/inf_clock/ \
 -I${ZEPHYR_BASE}/boards/posix/linux_emb_runner/"

gcc $C_FLAGS -c ${ZEPHYR_BASE}/linux_embedded_runner/src/main.c -o runner.o ${INCLUDES} -include include/generated/autoconf.h

gcc runner.o cpu_0.sw.o -o linux_runner.exe $C_FLAGS_FINALLINK
