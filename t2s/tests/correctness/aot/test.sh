#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
NOCOLOR='\033[0m'

# In this array, every element contains:
# Test file
regression=(
        gemm
        lu
        )

succ=0
fail=0

function emulate_func {
    eval file="$1"
    printf "$file emulate"
    compile1="   g++ $file-generate.cpp -g -I ../util -I ../../../../Halide/include -L ../../../../Halide/bin $EMULATOR_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 "
    rm -rf b b.aoc* b.cl a host.cpp host.h exec_time.txt a.out
    $compile1 >& a
    if [ -f "a.out" ]; then
        # There is an error "Unterminated quoted string" using $run due to AOC_OPTION. To avoid it, explicitly run for every case.
        rm -f a
        run1="env BITSTREAM=b.aocx AOC_OPTION="\""$EMULATOR_AOC_OPTION -board=$FPGA_BOARD -emulator-channel-depth-model=strict "\"" ./a.out"
        timeout 5m env BITSTREAM=b.aocx AOC_OPTION="$EMULATOR_AOC_OPTION -board=$FPGA_BOARD -emulator-channel-depth-model=strict " ./a.out >& a        
        compile2="   g++ $file-run.cpp host.cpp ../../../src/AOT-OpenCL-Runtime.cpp ../../../src/SharedUtilsInC.cpp -g -DLINUX -DALTERA_CL -fPIC -I../../../src/ -I ../../../../Halide/include -I$INTELFPGAOCLSDKROOT/examples_aoc/common/inc $INTELFPGAOCLSDKROOT/examples_aoc/common/src/AOCLUtils/opencl.cpp $INTELFPGAOCLSDKROOT/examples_aoc/common/src/AOCLUtils/options.cpp -I$INTELFPGAOCLSDKROOT/host/include -L$INTELFPGAOCLSDKROOT/linux64/lib -L$AOCL_BOARD_PACKAGE_ROOT/linux64/lib -L$INTELFPGAOCLSDKROOT/host/linux64/lib -lOpenCL -L ../../../../Halide/bin -lelf $EMULATOR_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 "
        $compile2 >& a
        if [ -f "a.out" ]; then
            run2="env CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA=1 INTEL_FPGA_OCL_PLATFORM_NAME="\""$EMULATOR_PLATFORM"\"" BITSTREAM=b.aocx ./a.out"
            timeout 5m env CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA=1 INTEL_FPGA_OCL_PLATFORM_NAME="$EMULATOR_PLATFORM" BITSTREAM=b.aocx ./a.out >& a
            if  tail -n 1 a | grep -q -E "^Success!"; then
                echo >> success.txt
                echo "rm -rf b b.aoc* b.cl a host.cpp host.h exec_time.txt a.out" >> success.txt
                echo $compile1 >> success.txt
                echo $run1 >> success.txt
                echo $compile2 >> success.txt
                echo $run2 >> success.txt
                cat a >> success.txt
                let succ=succ+1
                echo " Success!"
            else
                echo >> failure.txt
                echo "rm -rf b b.aoc* b.cl a host.cpp host.h exec_time.txt a.out" >> failure.txt
                echo $compile1 >> failure.txt
                echo $run1 >> failure.txt
                echo $compile2 >> failure.txt
                echo $run2 >> failure.txt
                cat a >> failure.txt
                let fail=fail+1
                echo "Failure!"
            fi
        else
            echo >> failure.txt
            echo "rm -rf b b.aoc* b.cl a host.cpp host.h exec_time.txt a.out" >> failure.txt
            echo $compile1 >> failure.txt
            echo $run1 >> failure.txt
            echo $compile2 >> failure.txt
            cat a >> failure.txt
            let fail=fail+1
            echo " Failure!"
        fi
    else
        echo >> failure.txt
        echo "rm -rf b b.aoc* b.cl a host.cpp host.h exec_time.txt a.out" >> failure.txt
        echo $compile1 >> failure.txt
        cat a >> failure.txt
        let fail=fail+1
        echo " Failure!"
    fi 
    rm -rf b b.aoc* b.cl a host.cpp host.h exec_time.txt a.out
}
        
rm -f success.txt failure.txt

array_to_read=("${regression[@]}")
echo "Testing AOT for regression."

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
