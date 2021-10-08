#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
NOCOLOR='\033[0m'

# skip test about compute_root by now
# In this array, every element contains:
#  Test name
#  gcc options
array=(   "scatter-1-1.cpp" "-DSTRATEGY=ScatterStrategy::Up"
        # "scatter-1-1.cpp" "-DSTRATEGY=ScatterStrategy::Down"
        # "scatter-1-2.cpp" "-DSTRATEGY=ScatterStrategy::Up"
        # "scatter-1-3.cpp" "-DSTRATEGY=ScatterStrategy::Up"
        # "scatter-1-3.cpp" "-DSTRATEGY=ScatterStrategy::Down"
        # "gemm-3.cpp"      "-DSTRATEGY=ScatterStrategy::Up   -DPLACE=Place::Device -DSCATTER -DDESIGN=1"
        # "gemm-3.cpp"      "-DSTRATEGY=ScatterStrategy::Up   -DPLACE=Place::Device -DSCATTER -DDESIGN=2"
        # "gemm-3.cpp"      "-DSTRATEGY=ScatterStrategy::Up   -DPLACE=Place::Device -DSCATTER -DDESIGN=3"
        # "gemm-3.cpp"      "-DSTRATEGY=ScatterStrategy::Up   -DPLACE=Place::Device -DSCATTER -DDESIGN=4"
        # "gemm-3.cpp"      "-DSTRATEGY=ScatterStrategy::Up   -DPLACE=Place::Device -DSCATTER -DDESIGN=5"
          "ure-iso-input-chain-a-b-vec-scatter.cpp" "-DSTRATEGY=ScatterStrategy::Up"
        
)

succ=0
fail=0

function test_func {
    eval file="$1"
    eval option="$2"
    compile="    g++ $file $option -g -I ../util  -I ../../../../Halide/include -L ../../../../Halide/bin $EMULATOR_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -DSIZE=10"
    clean="rm -rf a a.out $HOME/tmp/a.aocx $HOME/tmp/a.aocr $HOME/tmp/a.aoco $HOME/tmp/a.cl $HOME/tmp/a profile_info.txt"
    $clean
    $compile >& a
    if [ -f "a.out" ]; then
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
            echo "Success!"
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
        echo "Failure!"
    fi 
    $clean
}
        
echo "Testing scattering."
rm -f success.txt failure.txt
index=0
while [ "$index" -lt "${#array[*]}" ]; do
   file=${array[$index]}
   gcc_option=${array[$((index+1))]}
   let index=index+2
   printf "Case: $file $gcc_option "       
   test_func "\${file}" "\${gcc_option}"
done

let total=succ+fail
echo -e Total $total, pass ${GREEN}$succ${NOCOLOR}, fail ${RED}$fail${NOCOLOR}. See $PWD/success.txt and failure.txt for details.

# Return values for the parent script
echo $total > ../total.temp
echo $succ > ../succ.temp
echo $fail > ../fail.temp
