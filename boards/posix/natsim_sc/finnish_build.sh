#! /usr/bin/env bash

echo "** Entered Finnish_build.sh **"
cd ${NSI_PATH}/
make all --warn-undefined-variables #NSI_VERBOSE=1
