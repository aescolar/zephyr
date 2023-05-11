# Copyright 2023 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

# Linux Embedded Runner (LER) Makefile.
# It builds the runner itself, and produces the final
# Linux executable by linking it to the the embeded cpu library

# By default all the build output is placed under the _build folder, but the user can override it
# setting this variable
LER_BUILD_PATH?=$(abspath _build/)
EXE_NAME?=linux_runner.exe
LER_EXE?=${LER_BUILD_PATH}/${EXE_NAME}
LER_LINK_FLAGS?=
EMBEDDED_CPU_SW?=

#TODO: remove this, we should eventually only get the EMBEDDED_CPU_SW variable
ifeq ("$(wildcard $(EMBEDDED_BUILD_FOLDER))","")
$(error EMBEDDED_BUILD_FOLDER (${EMBEDDED_BUILD_FOLDER}) does not point to a folder )
endif

SHELL:=bash
CC:=gcc
AR:=ar

no_default:
	@echo "There is no default rule, please specify what you want to build,\
 or run make help for more info"

ARCH:=-m32
DEBUG:=-g
OPT:=-O0
WARNINGS:=-Wall -Wpedantic
COVERAGE:=--coverage
CPPFLAGS:=
NO_PIE:=-fno-pie -fno-pic -no-pie
DEPENDFLAGS=-MMD -MP
CFLAGS:=${ARCH} ${DEBUG} ${COVERAGE} ${OPT} ${NO_PIE} -ffunction-sections -fdata-sections \
  ${DEPENDFLAGS} -std=c11
FINALLINK_FLAGS:=${ARCH} ${COVERAGE} ${NO_PIE} \
  -Wl,--gc-sections \
  ${LER_LINK_OPTIONS}

RUNNER_LIB:=runner.a

SRCS:=src/main.c \
 src/ler_trace.c \
 src/ler_tasks.c \
 src/ler_cmdline.c \
 src/ler_cmdline_common.c

INCLUDES:=\
 -Isrc/include/ \
 -I${ZEPHYR_BASE}/linux_embedded_runner/src/include \
 \
 -I${ZEPHYR_BASE}/include \
 -I${ZEPHYR_BASE}/soc/posix/inf_clock/ \
 -I${ZEPHYR_BASE}/boards/posix/linux_emb_runner/ \
 -include ${EMBEDDED_BUILD_FOLDER}/include/generated/autoconf.h

OBJS=$(abspath $(addprefix $(LER_BUILD_PATH)/,${SRCS:.c=.o}))

DEPENDFILES:=$(addsuffix .d,$(basename ${OBJS})) 

-include ${DEPENDFILES}

${LER_BUILD_PATH}:
	@if [ ! -d ${LER_BUILD_PATH} ]; then mkdir -p ${LER_BUILD_PATH}; fi

${LER_BUILD_PATH}/%.o: %.c Makefile | ${LER_BUILD_PATH}
	@if [ ! -d $(dir $@) ]; then mkdir -p $(dir $@); fi
	@${CC} ${CPPFLAGS} ${INCLUDES} ${CFLAGS} -c $< -o $@

${LER_BUILD_PATH}/linker_script.ld : other/linker_script.pre.ld
	@${CC} -x c -E -P $< -o $@  ${DEPENDFLAGS}

${LER_BUILD_PATH}/${RUNNER_LIB}: ${OBJS}
	@rm ${LER_BUILD_PATH}/${RUNNER_LIB} &> /dev/null ; true
	@${AR} -cr ${LER_BUILD_PATH}/${RUNNER_LIB} ${OBJS}

${LER_EXE}: ${LER_BUILD_PATH}/${RUNNER_LIB} ${EMBEDDED_CPU_SW} ${LER_BUILD_PATH}/linker_script.ld
	@if [ -z ${EMBEDDED_CPU_SW} ] || [ ! -f ${EMBEDDED_CPU_SW} ]; then \
	  echo "Erorr: Input embedded CPU SW not found (EMBEDDED_CPU_SW=${EMBEDDED_CPU_SW} )"; \
	  false; \
	fi
	@objcopy --localize-hidden ${EMBEDDED_CPU_SW} ${LER_BUILD_PATH}/cpu_0.sw.o -w --localize-symbol=_*
	@gcc ${LER_BUILD_PATH}/${RUNNER_LIB} ${LER_BUILD_PATH}/cpu_0.sw.o -o $@ ${FINALLINK_FLAGS} \
	-T ${LER_BUILD_PATH}/linker_script.ld

Makefile: ;

link_with_esw: ${LER_EXE};

runner_lib: ${LER_BUILD_PATH}/${RUNNER_LIB}

all: link_with_esw

clean:
	@echo "Deleting intermediate compilation results + libraries + executables (*.d .o .a .exe)"
	@find $(LER_BUILD_PATH) -name "*.o" -or -name "*.exe" -or -name "*.a" -or -name "*.d" | xargs rm -f

clean_coverage:
	@find $(LER_BUILD_PATH) -name "*.gcda" -or -name "*.gcno" | xargs rm -f ; true

clean_all: clean clean_coverage ;

.PHONY: clean clean_coverage clean_all link_with_esw runner_lib no_default all ${DEPENDFILES}

help:
	@echo "**********************************"
	@echo "* Linux Embedded Runner makefile *"
	@echo "**********************************"
	@echo "Provided rules:"
	@echo " clean           : clean all build output"
	@echo " clean_coverage  : clean all coverage files"
	@echo " clean_all       : clean + clean_coverage"
	@echo " link_with_esw   : Link the runner with the CPU embedded sw"
	@echo " runner_lib      : Build the runner itself (pending the embedded SW)"
	@echo " all             : link_with_esw"
	@echo "Note that you can use TAB to autocomplete rules in the command line in modern OSs"
