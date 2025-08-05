# Copyright (c) 2025 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

# Selection of optional components provided by the native simulator

if(CONFIG_NATIVE_USE_NSI_ERRNO OR CONFIG_NATIVE_USE_NSI_FCNTL)
  zephyr_library()
endif()

if(CONFIG_NATIVE_USE_NSI_ERRNO)
  zephyr_library_sources(${NSI_DIR}/common/src/nsi_errno.c)
endif()

if(CONFIG_NATIVE_USE_NSI_FCNTL)
  zephyr_library_sources(${NSI_DIR}/common/src/nsi_fcntl.c)
endif()
