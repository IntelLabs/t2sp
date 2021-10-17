#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
NOCOLOR='\033[0m'

# In this array, every element contains:
#  Test name
#  number of place combinations in addition to the default combination of all being Host
#  number of places in each combination
#  place combinations
array=( "isolate-producer-a-A.cpp" 2 2
           Host Device
           Device Device
        "isolate-producer-a-A-2.cpp" 2 2   
           Host Device
           Device Device
        "isolate-producer-a-A-3-negative.cpp" 0 2
        "isolate-producer-a-A-4.cpp" 3 3
           Host Device Device
           Host Device Host
           Device Host Device
        "isolate-producer-a-A-5.cpp" 3 5
           Host Host Host Device Host
           Device Device Device Device Device
           Host Host Device Device Device
        "isolate-producer-a-A-6.cpp" 2 2
           Host Device
           Device Device
        "isolate-producer-a-A-7.cpp" 2 5
           Host Device Device Device Host
           Device Host Host Host Device
        "isolate-producer-a-A-8.cpp" 2 7
           Host Device Device Device Device Device Host
           Device Host Host Host Host Host Device
        "isolate-producer-a-A-condition-9.cpp" 3 2
           Host Device
           Device Device
           Device Host
        "isolate-producer-a-A-condition-10.cpp" 3 2
           Host Device
           Device Device
           Device Host        
        "isolate-producer-a-A-condition-merge-ures-11.cpp" 3 2
           Host Device
           Device Device
           Device Host        
        "isolate-producer-a-A-condition-merge-ures-12.cpp" 3 2
           Host Device
           Device Device
           Device Host        
        "isolate-producer-a-A-condition-merge-ures-negative-13.cpp" 0 2
        "isolate-producer-a-A-condition-merge-ures-negative-14.cpp" 0 2
        "isolate-producer-a-A-17-negative.cpp" 0 2
        "isolate-producer-a-A-condition-merge-ures-negative-18.cpp" 0 2
        "isolate-producer-a-A-19-negative.cpp" 0 2
        "isolate-consumer-1.cpp" 3 2
           Host Device
           Device Host
           Device Device
        "isolate-consumer-2.cpp" 2 3
           Host Device Host
           Device Device Host
        "isolate-consumer-3-merge-ures.cpp" 3 2
           Host Device
           Device Host
           Device Device
         "isolate-consumer-4-merge-ures.cpp" 3 2
           Host Device
           Device Host
           Device Device
         "isolate-producer-a-A-condition-merge-ures-19.cpp" 2 2
           Host Device
           Device Host
         "isolate-producer-signals-0.cpp" 2 3
           Device Host Device
           Device Device Device
         "isolate-producer-signals-1.cpp" 1 4
           Device Device Device Device
      )

succ=0
fail=0

function test_func {
    eval file="$1"
    eval place_combination="$2"
    compile="    g++ $file -g -I ../util  -I ../../../../Halide/include -L ../../../../Halide/bin $EMULATOR_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -DSIZE=10  -DVERBOSE_DEBUG $place_combination"
    clean="rm -rf a a.out $HOME/tmp/a.aocx $HOME/tmp/a.aocr $HOME/tmp/a.aoco $HOME/tmp/a.cl $HOME/tmp/a exec_time.txt"
    $clean
    $compile >& a
    if [ -f "a.out" ]; then
        # There is an error "Unterminated quoted string" using $run due to AOC_OPTION. To avoid it, explicitly run for every case.
        rm -f a
        timeout 5m env CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA=1 INTEL_FPGA_OCL_PLATFORM_NAME="$EMULATOR_PLATFORM" AOC_OPTION="$EMULATOR_AOC_OPTION -board=$FPGA_BOARD" ./a.out >& a
        run="env  CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA=1 INTEL_FPGA_OCL_PLATFORM_NAME="$EMULATOR_PLATFORM" AOC_OPTION="\""$EMULATOR_AOC_OPTION -board=$FPGA_BOARD"\"" ./a.out"
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
        
echo "Testing isolation."
rm -f success.txt failure.txt
index=0
while [ "$index" -lt "${#array[*]}" ]; do
   file=${array[$index]}
   num_combinations=${array[$((index+1))]}
   num_places=${array[$((index+2))]}
   let index=index+3

   # default case: all places are Host
   default_option=""
   for (( place=0; place<$num_places; place++ )); do
       default_option="$default_option -DPLACE$place=Place::Host"
   done
   printf "Case: $file $default_option"
   test_func "\${file}" "\${default_option}"
   
   for (( combination=0; combination<$num_combinations; combination++ )); do
       option=""
       for (( place=0; place<$num_places; place++ )); do
           option="$option -DPLACE$place=Place::${array[$index]}"
           let index=index+1
       done
       printf "Case: $file $option"
       test_func "\${file}" "\${option}"
   done
done

let total=succ+fail
echo -e Total $total, pass ${GREEN}$succ${NOCOLOR}, fail ${RED}$fail${NOCOLOR}. See $PWD/success.txt and failure.txt for details.

# Return values for the parent script
echo $total > ../total.temp
echo $succ > ../succ.temp
echo $fail > ../fail.temp