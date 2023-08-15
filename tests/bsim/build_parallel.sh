#!/usr/bin/env bash
# Copyright 2023 Nordic Semiconductor ASA
# SPDX-License-Identifier: Apache-2.0

function display_help(){
  echo "build_parallel.sh [-help] [options]"
}

# Parse command line
if [ $# -ge 1 ]; then
  if grep -Eiq "(\?|-\?|-h|help|-help|--help)" <<< $1 ; then
    display_help
    exit 0
  fi
fi

err=0
i=0

if [ -n "${TESTS_FILE}" ]; then
	#remove comments and empty lines from file
	all_cases=$(sed 's/#.*$//;/^$/d' "${TESTS_FILE}")
elif [ -n "${TESTS_LIST}" ]; then
	all_cases=${TESTS_LIST}
else
	SEARCH_PATH="${SEARCH_PATH:-.}"
	all_cases=`find ${SEARCH_PATH} -name "*.sh" | \
	         grep -Ev "(/_|run_parallel|build_parallel.sh|compile.*sh|generate_coverage_report.sh)"`
	#we dont run ourselves
fi

set -u

all_cases_a=( $all_cases )
n_cases=$((${#all_cases_a[@]}))

needed_apps=()

#find all lines in the testcase scripts starting with '#.*Requires:'
#remove posible trailing comments, and trim spaces 
needed_apps+=$(grep -E "#.*Requires:" $all_cases | sed 's/.*#.*Requires://g' | sed 's/#.*$//;/^$/d' | xargs)
 
# Remove duplicates
needed_apps_sorted=$(echo $needed_apps | sort | uniq)
#echo $needed_apps_sorted

build_scripts=$(echo $needed_apps_sorted | awk 'NR > 1 {printf("-o ")}; {print "-name compile."$0".sh"}' | xargs find ${ZEPHYR_BASE}/tests/bsim/)

if [ `command -v parallel` ]; then
  parallel '
  {} $@ &> {#}.log
  if [ $? -ne 0 ]; then
    (>&2 echo -e "\e[91m{} FAILED\e[39m")
    (>&2 cat {#}.log)
    exit 1
  else
    (>&2 echo -e "{} SUCCEEDED")
    rm {#}.log
  fi
  ' ::: $build_scripts ; err=$?
else #fallback in case parallel is not installed
  for build_script in $build_scripts; do
    $build_script $@ &> $i.log
    if [ $? -ne 0 ]; then
      echo -e "\e[91m$build_script FAILED\e[39m"
      cat $i.log
      let "err++"
    else
      echo -e "$build_script SUCCEEDED"
    fi
    rm $i.log
    let i=i+1
  done
fi

exit $err
