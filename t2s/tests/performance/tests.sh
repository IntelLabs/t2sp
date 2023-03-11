#!/bin/bash

function show_usage {
    echo "Usage:"
    echo "  ./tests.sh (devcloud|local) (oneapi|opencl|cm) (a10|s10|gen9|gen12) [bitstream]"
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

if [ "$2" != "oneapi" -a "$2" != "opencl" -a "$2" != "cm" ]; then
    show_usage
    exit
else
    language="$2"
fi

if [ "$3" != "a10" -a "$3" != "s10"  -a  "$3" != "gen9" -a  "$3" != "gen12" ]; then
    show_usage
    exit
else
    target="$3"
fi

# The path to this script.
PATH_TO_SCRIPT="$( cd "$(dirname "$BASH_SOURCE" )" >/dev/null 2>&1 ; pwd -P )"

cur_dir=$PWD
cd $PATH_TO_SCRIPT

if [ "$target" == "gen9" -o "$target" == "gen12" ]; then
    # GPU: Verify correctness with ITER=1
    ./test.sh $location $language gemm $target tiny hw
    ./test.sh $location $language conv $target tiny hw
    ./test.sh $location $language capsule $target tiny hw
    ./test.sh $location $language pairhmm $target tiny hw
    
    # GPU: Test perf with ITER=100
    ./test.sh $location $language gemm $target large hw
    ./test.sh $location $language conv $target large hw
    ./test.sh $location $language capsule $target large hw
    ./test.sh $location $language pairhmm $target large hw
elif [ "$target" == "a10" -o "$target" == "s10" ]; then
    # FPGA: Verify correctness with tiny problem sizes and emulator
    ./test.sh $location $language gemm $target tiny emulator
    ./test.sh $location $language qrd $target tiny emulator
    ./test.sh $location $language conv $target tiny emulator
    ./test.sh $location $language capsule $target tiny emulator
    ./test.sh $location $language pairhmm $target tiny emulator
    
    # FPGA: Test perf with large matrices on real hardware
    ./test.sh $location $language gemm $target large hw $4
    ./test.sh $location $language qrd $target large hw $4
    ./test.sh $location $language conv $target large hw $4
    ./test.sh $location $language capsule $target large hw $4
    ./test.sh $location $language pairhmm $target large hw $4
else
    echo "Performance testing on $target not supported yet in this release"
    exit
fi

cd $cur_dir
