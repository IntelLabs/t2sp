#!/bin/bash

function show_usage {
    echo "Usage:"
    echo "  ./tests.sh (devcloud|local) "
}       

# BASH_SOURCE is this script
if [ $0 != $BASH_SOURCE ]; then
    echo "Error: The script should be directly run, not sourced"
    return
fi

if [ "$HOSTNAME" == "login-2" ]; then
    echo "Error: The script should be run on a compute node of $location or a local machine, not on the head node of $location (login-2)."
    exit
fi

if [ "$1" != "devcloud" -a "$1" != "local" ]; then
    show_usage
    exit
else
    location="$1"
fi

# The path to this script.
PATH_TO_SCRIPT="$( cd "$(dirname "$BASH_SOURCE" )" >/dev/null 2>&1 ; pwd -P )"

cur_dir=$PWD
cd $PATH_TO_SCRIPT

# GPU: Verify correctness with tiny problem sizes
./test.sh $location gemm gen9 tiny hw
./test.sh $location conv gen9 tiny hw
./test.sh $location capsule gen9 tiny hw

# GPU: Test perf
./test.sh $location gemm gen9 large hw
./test.sh $location conv gen9 large hw
./test.sh $location capsule gen9 large hw

# FPGA: Verify correctness with tiny problem sizes and emulator
./test.sh $location gemm a10 tiny emulator
./test.sh $location conv a10 tiny emulator
./test.sh $location capsule a10 tiny emulator

# FPGA: Test perf
./test.sh $location gemm a10 large hw
./test.sh $location conv a10 large hw
./test.sh $location capsule a10 large hw

cd $cur_dir