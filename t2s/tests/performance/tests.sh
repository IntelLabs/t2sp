#!/bin/bash

function show_usage {
    echo "Usage:"
    echo "  ./tests.sh (devcloud|local) (a10|s10|gen9|gen12)"
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

if [ "$2" != "a10" -a "$2" != "s10"  -a  "$2" != "gen9" -a  "$2" != "gen12" ]; then
    show_usage
    exit
else
    target="$2"
fi

# The path to this script.
PATH_TO_SCRIPT="$( cd "$(dirname "$BASH_SOURCE" )" >/dev/null 2>&1 ; pwd -P )"

cur_dir=$PWD
cd $PATH_TO_SCRIPT

if [ "$target" == "gen9" ]; then
    # GPU: Verify correctness with ITER=1
    ./test.sh $location gemm $target tiny hw
    ./test.sh $location conv $target tiny hw
    ./test.sh $location capsule $target tiny hw
    
    # GPU: Test perf with ITER=100
    ./test.sh $location gemm $target large hw
    ./test.sh $location conv $target large hw
    ./test.sh $location capsule $target large hw
elif [ "$target" == "a10" ]; then
    # FPGA: Verify correctness with tiny problem sizes and emulator
    ./test.sh $location gemm $target tiny emulator
    ./test.sh $location conv $target tiny emulator
    ./test.sh $location capsule $target tiny emulator
    
    # FPGA: Test perf with large matrices
    ./test.sh $location gemm $target large hw
    ./test.sh $location conv $target large hw
    ./test.sh $location capsule $target large hw
else
    echo "Performance testing on $target not supported yet in this release"
    exit
fi

cd $cur_dir