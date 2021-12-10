#!/bin/bash

# BASH_SOURCE is this script
if [ $0 != $BASH_SOURCE ]; then
    echo "Error: The script should be directly run, not sourced"
    return
fi

# Path to this script
PATH_TO_SCRIPT="$( cd "$(dirname "$BASH_SOURCE" )" >/dev/null 2>&1 ; pwd -P )" # The path to this script

cur_dir=$PWD
cd $PATH_TO_SCRIPT

source ./test-common-funcs.sh
source ./parse-options.sh $@
if [ "$wrong_options" != "0" ]; then
    echo "Error: wrong options encountered"
    exit
fi

# Convert size to uppercase: TINY and LARGE
size=${size^^}

echo ------------------- Testing $@
set -x
if [ "$target" == "a10" -o "$target" == "s10" ]; then
    generate_test_fpga_kernel
else
    generate_test_gpu_kernel
fi
set +x

cd $cur_dir

