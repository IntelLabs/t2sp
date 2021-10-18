#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
NOCOLOR='\033[0m'

# In this array, every element contains:
#  Test name
#  gcc options
array=(
          "vectorize-1-2-p.cpp" "-DPLACE0=Place::Host"
          "vectorize-1-2-p.cpp" "-DPLACE0=Place::Device"
          "vectorize-1-3-n.cpp" "-DPLACE0=Place::Host   -DTEST_HOST"
          "vectorize-1-3-n.cpp" "-DPLACE0=Place::Device"
          "vectorize-1-5-p.cpp" "-DPLACE0=Place::Host"   
          "vectorize-1-5-p.cpp" "-DPLACE0=Place::Device"   
          "vectorize-1-6-p.cpp" "-DPLACE0=Place::Host"   
          "vectorize-1-6-p.cpp" "-DPLACE0=Place::Device"   
          "vectorize-1-7-p.cpp" "-DPLACE0=Place::Host"   
          "vectorize-1-7-p.cpp" "-DPLACE0=Place::Device"   
          "vectorize-1-8-n.cpp" ""   
          "vectorize-2-2-p.cpp" "-DPLACE0=Place::Host   -DPLACE1=Place::Device"
          "vectorize-2-2-p.cpp" "-DPLACE0=Place::Device -DPLACE1=Place::Host"
        # "vectorize-2-2-p.cpp" "-DPLACE0=Place::Device -DPLACE1=Place::Device"
          "vectorize-2-3-p.cpp" "-DPLACE0=Place::Host   -DPLACE1=Place::Device"
          "vectorize-2-3-p.cpp" "-DPLACE0=Place::Device -DPLACE1=Place::Host"
          "vectorize-2-3-p.cpp" "-DPLACE0=Place::Device -DPLACE1=Place::Device"
          "vectorize-3-3-p.cpp" "-DPLACE0=Place::Host"   
          "vectorize-3-3-p.cpp" "-DPLACE0=Place::Device"   
          "vectorize-3-5-p.cpp" "-DPLACE0=Place::Host   -DPLACE1=Place::Device"
          "vectorize-3-5-p.cpp" "-DPLACE0=Place::Device -DPLACE1=Place::Host"
        # "vectorize-3-5-p.cpp" "-DPLACE0=Place::Device -DPLACE1=Place::Device"
        # "vectorize-3-6-p.cpp" "-DPLACE0=Place::Host   -DPLACE1=Place::Device -DPLACE2=Place::Device"
        # "vectorize-3-6-p.cpp" "-DPLACE0=Place::Host   -DPLACE1=Place::Device -DPLACE2=Place::Host"
        # "vectorize-3-6-p.cpp" "-DPLACE0=Place::Device -DPLACE1=Place::Host   -DPLACE2=Place::Device"
        # "vectorize-4-2-p.cpp" "-DPLACE0=Place::Device -DPLACE1=Place::Device -DPLACE2=Place::Device" 
          "vectorize-4-4-n.cpp" "-DPLACE0=Place::Device"
        # "vectorize-5-1-p.cpp" "-DPLACE0=Place::Host   -DPLACE1=Place::Device"
          "vectorize-5-1-p.cpp" "-DPLACE0=Place::Device -DPLACE1=Place::Host"
        # "vectorize-5-2-p.cpp" "-DPLACE0=Place::Host   -DPLACE1=Place::Device -DPLACE2=Place::Device" 
          "vectorize-5-4-p.cpp" "-DPLACE0=Place::Device -DPLACE1=Place::Device -DVEC_PRODUCER -DVEC_CONSUMER"
        # "vectorize-5-4-p.cpp" "-DPLACE0=Place::Device -DPLACE1=Place::Device -DVEC_PRODUCER"
          "vectorize-5-4-p.cpp" "-DPLACE0=Place::Device -DPLACE1=Place::Device -DVEC_CONSUMER"
        # "vectorize-7-1-p.cpp" "-DPLACE0=Place::Host"
        # "vectorize-7-1-p.cpp" "-DPLACE0=Place::Device"
          "vectorize-9-1-p.cpp" "-DPLACE0=Place::Device -DPARALLEL_LOOP"   
          "vectorize-9-1-p.cpp" "-DPLACE0=Place::Device -DVECTORIZE_LOOP"   
          "vectorize-9-1-p.cpp" "-DPLACE0=Place::Device -DUNROLL_LOOP"   
        # "vectorize-9-4-n.cpp" "-DPLACE0=Place::Device -DBLOCKS"   
        # "vectorize-9-4-n.cpp" "-DPLACE0=Place::Device -DTHREADS"  
        # "vectorize-9-4-n.cpp" "-DPLACE0=Place::Device"            
          "gemm-3.cpp"          "-DPLACE0=Place::Device -DDESIGN=1"
          "gemm-3.cpp"          "-DPLACE0=Place::Device -DDESIGN=3"
        
      )

succ=0
fail=0

function emulate_func {
    eval file="$1"
    eval gcc_options="$2"
    compile="g++ $file $gcc_options -g -I ../util  -I ../../../../Halide/include -L ../../../../Halide/bin $EMULATOR_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -DVERBOSE_DEBUG "
    clean="rm -rf a a.out $HOME/tmp/a.aocx $HOME/tmp/a.aocr $HOME/tmp/a.aoco $HOME/tmp/a.cl $HOME/tmp/a exec_time.txt"
    $clean
    $compile >& a
    if [ -f "a.out" ]; then
        # There is an error "Unterminated quoted string" using $run due to AOC_OPTION. To avoid it, explicitly run for every case.
        rm -f a
        run="env PRAGMAUNROLL=1 BITSTREAM="\""${HOME}/tmp/a.aocx"\"" CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA=1 INTEL_FPGA_OCL_PLATFORM_NAME="\""$EMULATOR_PLATFORM"\"" AOC_OPTION="\""$EMULATOR_AOC_OPTION -board=$FPGA_BOARD -emulator-channel-depth-model=strict "\"" ./a.out"
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
        
echo "Testing vectorization."
rm -f success.txt failure.txt
index=0
while [ "$index" -lt "${#array[*]}" ]; do
   file=${array[$index]}
   gcc_options=${array[$((index+1))]}
   let index=index+2
   printf "Case: $file $gcc_options"
   emulate_func "\${file}" "\${gcc_options}"
done

let total=succ+fail
echo -e Total $total, pass ${GREEN}$succ${NOCOLOR}, fail ${RED}$fail${NOCOLOR}. See $PWD/success.txt and failure.txt for details.

# Return values for the parent script
echo $total > ../total.temp
echo $succ > ../succ.temp
echo $fail > ../fail.temp
