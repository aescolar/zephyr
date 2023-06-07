#! /usr/bin/env bash
#
# Copyright (c) 2023 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

echo "** Entering native simulator runner build and link **"
cd ${NSI_PATH}/
make all --warn-undefined-variables #NSI_BUILD_VERBOSE=1
