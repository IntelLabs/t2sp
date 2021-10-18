#!/bin/bash
# ./test.sh

RED='\033[0;31m'
GREEN='\033[0;32m'
NOCOLOR='\033[0m'

# In this array, every element contains:
# Test file
regression=(
        ure-iso-input-a-b-device.cpp
        ure-iso-input-a-b-stt-vec-scatter-device.cpp
        ure-iso-input-a-b-stt-device.cpp
        ure-iso-input-a-b-vec-device.cpp
        ure-iso-input-all-device.cpp
        ure-iso-input-all-stt-device.cpp
        ure-iso-input-chain-all-device.cpp
        ure-iso-input-chain-all-stt-vec-device.cpp
        ure-iso-input-chain-a-b-device.cpp
        ure-iso-input-chain-a-b-vec-device.cpp
        ure-iso-output-chain-input-chain-a-b-stt-vec-device.cpp
        ure-iso-output-chain-stt-device.cpp
        ure-iso-output-chain-stt-gather-device.cpp
        ure-iso-output-chain-stt-vec-device.cpp
        ure-iso-output-device.cpp
        ure-iso-output-input-a-b-device.cpp
        ure-iso-output-input-a-b-stt-device.cpp
        ure-iso-output-input-a-b-stt-vec-device.cpp
        ure-iso-output-stt-device.cpp
        ure-iso-output-stt-vec-device.cpp
        ure-only-device.cpp
        ure-stt-device.cpp
        ure-stt-vec-device.cpp
        ure-vec-device.cpp
        gemm.cpp
        gemm2.cpp
        variableOuterLoopsGemm.cpp
        #variableOuterLoopsGemm2.cpp

)

succ=0
fail=0

function emulate_func {
    eval file="$1"
    printf "$file "
    compile="   g++ $file -g -I ../util  -I ../../../../Halide/include -L ../../../../Halide/bin $EMULATOR_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -DSIZE=10  -DVERBOSE_DEBUG -DPLACE0=Place::Host -DPLACE1=Place::Device "
    clean="rm -rf a a.out $HOME/tmp/a.aocx $HOME/tmp/a.aocr $HOME/tmp/a.aoco $HOME/tmp/a.cl $HOME/tmp/a exec_time.txt"
    $clean
    $compile >& a
    if [ -f "a.out" ]; then
        # There is an error "Unterminated quoted string" using $run due to AOC_OPTION. To avoid it, explicitly run for every case.
        rm -f a
        run="env PRAGMAUNROLL=1 CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA=1 INTEL_FPGA_OCL_PLATFORM_NAME="\""$EMULATOR_PLATFORM"\"" AOC_OPTION="\""$EMULATOR_AOC_OPTION -board=$FPGA_BOARD -emulator-channel-depth-model=strict "\"" ./a.out"
        timeout 5m env PRAGMAUNROLL=1 BITSTREAM="${HOME}/tmp/a.aocx" CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA=1 INTEL_FPGA_OCL_PLATFORM_NAME="$EMULATOR_PLATFORM" AOC_OPTION="$EMULATOR_AOC_OPTION -board=${FPGA_BOARD} -emulator-channel-depth-model=strict " ./a.out >& a
        if  tail -n 1 a | grep -q -E "^Success!"; then
            echo >> success.txt
            echo $clean >> success.txt
            echo $compile >> success.txt
            echo $run >> success.txt
            cat a >> success.txt
            let succ=succ+1
            echo " Success!"
        else
            echo >> failure.txt
            echo $clean >> failure.txt
            echo $compile >> failure.txt
            echo $run >> failure.txt
            cat a >> failure.txt
            let fail=fail+1
            echo "Failure!"
        fi
    else
        echo >> failure.txt
        echo $clean >> failure.txt
        echo $compile >> failure.txt
        cat a >> failure.txt
        let fail=fail+1
        echo " Failure!"
    fi 
    $clean 
}
        
rm -f success.txt failure.txt

array_to_read=("${regression[@]}")
echo "Testing GEMM for regression."

index=0
while [ "$index" -lt "${#array_to_read[*]}" ]; do
    file=${array_to_read[$index]}
    let index=index+1
    emulate_func "\${file}"
done

let total=succ+fail
echo -e Total $total, pass ${GREEN}$succ${NOCOLOR}, fail ${RED}$fail${NOCOLOR}. See $PWD/success.txt and failure.txt for details.

# Return values for the parent script
echo $total > ../total.temp
echo $succ > ../succ.temp
echo $fail > ../fail.temp
