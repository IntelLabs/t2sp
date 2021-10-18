#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
NOCOLOR='\033[0m'

# In this array, every element contains:
#  Test name
#  yes/no: should this test pass g++ compilation and generate a.out?
#  number of place combinations
#  number of places in each combination
#  place combinations
#  if test Buffer::Single

array=( "buffer-with-scatter.cpp" yes 1 2 no
            Host Device
        "buffer-without-remove.cpp" yes 1 2 no
            Host Device
        "buffer-without-scatter.cpp" yes 1 2 no
            Host Device
        "CNN-Kung-Song.cpp" yes 1 2 no
            Host Device
        "tmm_small.cpp" yes 1 2 no
            Host Device
      )

succ=0
fail=0

function test_func {
    eval file="$1"
    eval place_combination="$2"
    eval strategy="$3"
    eval expect_executable="$4"
    compile="    g++ $file -g -I ../util  -I ../../../../Halide/include -L ../../../../Halide/bin $EMULATOR_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -DSIZE=10  -DTINY -DVERBOSE_DEBUG $strategy $place_combination"
    clean="rm -rf a a.out $HOME/tmp/a.aocx $HOME/tmp/a.aocr $HOME/tmp/a.aoco $HOME/tmp/a.cl $HOME/tmp/a exec_time.txt"
    $clean        
    $compile >& a
    if [ -f "a.out" ]; then
        rm -f a
        run="env PRAGMAUNROLL=1 CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA=1 INTEL_FPGA_OCL_PLATFORM_NAME="$EMULATOR_PLATFORM" AOC_OPTION="\""$EMULATOR_AOC_OPTION -board=$FPGA_BOARD -emulator-channel-depth-model=strict "\"" ./a.out"
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
        if [ "$expect_executable" = "no" ]; then
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
            cat a >> failure.txt
            let fail=fail+1
            echo " Failure!"
        fi
    fi 
    $clean
}
        
echo "Testing databuffering."
rm -f success.txt failure.txt
index=0
while [ "$index" -lt "${#array[*]}" ]; do
   file=${array[$index]}
   expect_executable=${array[$((index+1))]}
   num_combinations=${array[$((index+2))]}
   num_places=${array[$((index+3))]}
   if_test_single=${array[$((index+4))]}
   let index=index+5
   
   for (( combination=0; combination<$num_combinations; combination++ )); do
       option=""
       for (( place=0; place<$num_places; place++ )); do
           option="$option -DPLACE$place=Place::${array[$index]}"
           let index=index+1
       done
       printf "Case: $file $option"
       if [ "$if_test_single" = "no" ]; then
            test_func "\${file}" "\${option}" "-DSTRATEGY=BufferStrategy::Double" "\${expect_executable}"
        else
            test_func "\${file}" "\${option}" "-DSTRATEGY=BufferStrategy::Double" "\${expect_executable}"
            printf "For Buffer::Single\n"
            test_func "\${file}" "\${option}" "-DSTRATEGY=BufferStrategy::Single" "\${expect_executable}"
       fi
   done

done

let total=succ+fail
echo -e Total $total, pass ${GREEN}$succ${NOCOLOR}, fail ${RED}$fail${NOCOLOR}. See $PWD/success.txt and failure.txt for details.

# Return values for the parent script
echo $total > ../total.temp
echo $succ > ../succ.temp
echo $fail > ../fail.temp
