# Note: This is not a Zephyr application project
# This builds the Linux runner which will be linked with a previously
# compiled zephyr image to produce an executable

BOGUS TRIAL IGNORE

cmake_minimum_required(VERSION 3.20.0)
project("Single Core linux runner")

add_executable(linux_runner.exe main.c)

#add_library(runner STATIC)
#target_sources(runner PRIVATE )
include_directories(PRIVATE
  -I$ENV{ZEPHYR_BASE}/include
  -I$ENV{ZEPHYR_BASE}/soc/posix/inf_clock/
  -I$ENV{ZEPHYR_BASE}/boards/posix/linux_runner_sc/
)

