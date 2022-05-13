#!/bin/bash
set +x

RED='\033[0;31m'
GREEN='\033[0;32m'
NOCOLOR='\033[0m'

# In this array, every element contains:
# Test file
regression=(
        # overlay-dummy.cpp
        
        overlay-1-1.cpp
        overlay-1-2.cpp
        overlay-1-3.cpp

        overlay-2-1.cpp
        overlay-2-2.cpp

        overlay-3-1.cpp

        overlay-4-1.cpp
        overlay-4-2.cpp

        overlay-many-tasks.cpp

        overlay-bcropped-1.cpp
        overlay-bcropped-2.cpp
        overlay-bcropped-3.cpp

        overlay-fib.cpp
        
        # overlay-circular-dep-negative.cpp
        # overlay-enqueue-negative.cpp
        overlay-command-negative.cpp
        
       )
       
succ=0
fail=0

function emulate_func {
    eval file="$1"
    eval bitstream="$2"
    printf "$file "
    clean="rm -rf a a.out $HOME/tmp/a.aocx $HOME/tmp/a.aocr $HOME/tmp/a.aoco $HOME/tmp/a.cl $HOME/tmp/a *.cl *.aocx *.aoco *.aocr test exec_time.txt *.temp"
    compile="   g++ $file -O0 -g -I ../util -I ../../../../Halide/include -L ../../../../Halide/bin $EMULATOR_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -DVERBOSE_DEBUG -DPLACE0=Place::Host -DPLACE1=Place::Device "
    $clean            
    $compile >& a
    if [ -f "a.out" ]; then
        # There is an error "Unterminated quoted string" using $run due to AOC_OPTION. To avoid it, explicitly run for every case.
        rm -f a
        run="env BITSTREAM=test.aocx PRAGMAUNROLL=1 CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA=1 INTEL_FPGA_OCL_PLATFORM_NAME="$EMULATOR_PLATFORM" AOC_OPTION="\""$EMULATOR_AOC_OPTION -board=$FPGA_BOARD -emulator-channel-depth-model=strict "\"" ./a.out"
        timeout 5m env CL_CONFIG_CHANNEL_DEPTH_EMULATION_MODE=strict CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA=1 INTEL_FPGA_OCL_PLATFORM_NAME="$EMULATOR_PLATFORM" AOC_OPTION="$EMULATOR_AOC_OPTION -board=$FPGA_BOARD -I $INTELFPGAOCLSDKROOT/include/kernel_headers" BITSTREAM="$bitstream" ./a.out >& a
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
            echo $clean >> success.txt
            echo $compile >> failure.txt
            echo $run >> failure.txt
            cat a >> failure.txt
            let fail=fail+1
            echo "Failure!"
        fi
    else
        echo >> failure.txt
        echo $clean >> success.txt
        echo $compile >> failure.txt
        cat a >> failure.txt
        let fail=fail+1
        echo " Failure!"
    fi 
    $clean
}

rm -rf success.txt failure.txt

array_to_read=("${regression[@]}")
echo "Testing Overlay for regression."

index=0
while [ "$index" -lt "${#array_to_read[*]}" ]; do
    file=${array_to_read[$index]}
    let index=index+1
    emulate_func "\${file}" "test.aocx"
done

let total=succ+fail
echo -e Total $total, pass ${GREEN}$succ${NOCOLOR}, fail ${RED}$fail${NOCOLOR}. See $PWD/success.txt and failure.txt for details.

# Return values for the parent script
echo $total > ../total.temp
echo $succ > ../succ.temp
echo $fail > ../fail.temp
