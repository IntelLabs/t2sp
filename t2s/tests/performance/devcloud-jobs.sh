#!/bin/bash

# BASH_SOURCE is this script
if [ $0 != $BASH_SOURCE ]; then
    echo "Error: The script should be directly run, not sourced"
    return
fi

if [ "$HOSTNAME" != "login-2" ]; then
    echo "Error: The script should be run on the head node of DevCloud, i.e. login-2."
    exit
fi

# The path to this script.
PATH_TO_SCRIPT="$( cd "$(dirname "$BASH_SOURCE" )" >/dev/null 2>&1 ; pwd -P )"

cur_dir=$PWD
cd $PATH_TO_SCRIPT

# GPU: Verify correctness with tiny problem sizes
./devcloud-job.sh gemm gen9 tiny hw
./devcloud-job.sh conv gen9 tiny hw
./devcloud-job.sh capsule gen9 tiny hw

# GPU: Test perf
./devcloud-job.sh gemm gen9 large hw
./devcloud-job.sh conv gen9 large hw
./devcloud-job.sh capsule gen9 large hw

# FPGA: Verify correctness with tiny problem sizes and emulator
./devcloud-job.sh gemm a10 tiny emulator
./devcloud-job.sh conv a10 tiny emulator
./devcloud-job.sh capsule a10 tiny emulator

# FPGA: Test perf
./devcloud-job.sh gemm a10 large hw
./devcloud-job.sh conv a10 large hw
./devcloud-job.sh capsule a10 large hw

cd $cur_dir