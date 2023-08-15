# Copyright 2023 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0
#!/usr/bin/env bash

set -eu

source ${ZEPHYR_BASE}/tests/bsim/compile.common

app=tests/bsim/bluetooth/audio _compile
