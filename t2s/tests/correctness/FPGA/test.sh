#!/bin/bash
set +x

RED='\033[0;31m'
GREEN='\033[0;32m'
NOCOLOR='\033[0m'

# In this array, every element contains:
#  Test name
#  g++ compile option to generate a.out.
#  bitstream name. "" means the default "~/tmp/a.aocx".
#  remove/keep: after a.out runs, remove the generated OpenCL and bitstream file? If not, the bitstream may be invoked to run in the next realize. 
#  PLACE1: host or device
#  PLACE2: host or device

array=(
        # places are meaningless for 1.cpp
        "1.cpp" " -DCOMPILE " ""  "keep"    "Host" "Host" 

        # Loader on host.
        "2.cpp" " -DCOMPILE " ""  "keep"    "Host" "Host"   

        # Loader on device.
        "2.cpp" " -DCOMPILE " ""  "keep"    "Device" "Device"   
        "2.cpp" " -DREALIZE " ""  "remove"  "Device" "Device"   

        # Drainer on host.
        "3.cpp" " -DCOMPILE " ""  "keep"    "Host" "Host"   
        
        # Drainer on device.
        "3.cpp" " -DCOMPILE " ""  "keep"    "Device" "Device"   
        "3.cpp" " -DREALIZE " ""  "remove"  "Device" "Device"   

        # Loader and Drainer on host
        "4.cpp" " -DCOMPILE " "$HOME/tmp/4.aocx"  "keep"   "Host" "Host"
        "4.cpp" " -DREALIZE " "$HOME/tmp/4.aocx"  "remove" "Host" "Host"
      )

succ=0
fail=0

function test_func {
    eval file="$1"
    eval gcc_option="$2"
    eval bitstream="$3"
    eval remove="$4"
    eval place1="$5"
    eval place2="$6"
    if [ "$bitstream" = "" ]; then
        bitstream_option=" "  
    else
        bitstream_option=" BITSTREAM="\""$bitstream"\"" "
    fi
    target_option=" CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA=1 INTEL_FPGA_OCL_PLATFORM_NAME="$EMULATOR_PLATFORM" AOC_OPTION="\""$EMULATOR_AOC_OPTION -board=$FPGA_BOARD"\"" "
    places=" -DPLACE1=Place::$place1 -DPLACE2=Place::$place2 "
    compile="g++ $file -O0 -g -I ../util  -I ../../../../Halide/include -L ../../../../Halide/bin $EMULATOR_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -DSIZE=10 -DVERBOSE_DEBUG $gcc_option $places "
    run="env $bitstream_option $target_option ./a.out"
    clean="rm -rf a a.out $HOME/tmp/a.aocx $HOME/tmp/a.aocr $HOME/tmp/a.aoco $HOME/tmp/a.cl $HOME/tmp/a"
    $clean
    $compile >& a
    if [ -f "a.out" ]; then        
        # There is an error "Unterminated quoted string" using $run due to BITSTREAM and AOC_OPTION. To avoid it, explicitly run for every case.
        rm -f a
        if [ "$bitstream" = "" ]; then
            timeout 5m env CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA=1 INTEL_FPGA_OCL_PLATFORM_NAME="$EMULATOR_PLATFORM" AOC_OPTION="$EMULATOR_AOC_OPTION -board=$FPGA_BOARD" ./a.out >& a
        else
            timeout 5m env CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA=1 INTEL_FPGA_OCL_PLATFORM_NAME="$EMULATOR_PLATFORM" AOC_OPTION="$EMULATOR_AOC_OPTION -board=$FPGA_BOARD" BITSTREAM="$bitstream" ./a.out >& a
        fi
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

    if [ "$remove" = "remove" ]; then
        rm -rf $bitstream profile_info.txt a $HOME/tmp/a.*  $HOME/tmp/a $HOME/tmp/4.*  $HOME/tmp/4 $HOME/tmp/5.*  $HOME/tmp/5
    fi
}
        
echo "Testing FPGA target."
rm -rf success.txt failure.txt profile_info.txt a $HOME/tmp/a.*  $HOME/tmp/a $HOME/tmp/4.*  $HOME/tmp/4 $HOME/tmp/5.*  $HOME/tmp/5
index=0
while [ "$index" -lt "${#array[*]}" ]; do
   file=${array[$index]}
   gcc_option=${array[$((index+1))]}
   bitstream=${array[$((index+2))]}
   remove=${array[$((index+3))]}
   place1=${array[$((index+4))]}
   place2=${array[$((index+5))]}
   let index=index+6
   printf "Case: $file $gcc_option $bitstream $emulator $remove $place1 $place2"
   test_func "\${file}" "\${gcc_option}" "\${bitstream}" "\${remove}" "\${place1}" "\${place2}" 
done

let total=succ+fail
echo -e Total $total, pass ${GREEN}$succ${NOCOLOR}, fail ${RED}$fail${NOCOLOR}. See $PWD/success.txt and failure.txt for details.

# Return values for the parent script
echo $total > ../total.temp
echo $succ > ../succ.temp
echo $fail > ../fail.temp