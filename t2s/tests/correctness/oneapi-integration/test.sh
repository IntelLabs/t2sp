#!/bin/bash
# ./test.sh

RED='\033[0;31m'
GREEN='\033[0;32m'
NOCOLOR='\033[0m'

# In this array, every element contains:
# Test file
EMU_DEFINES="-DTINY -DFPGA_EMULATOR -DFPGA"
FPGA_DEFINES="-DFPGA"

EMU=(
    "oneapi-target-4-gemm.cpp" "oneapi-target-4-run.cpp" "test.fpga_emu" "$EMU_DEFINES"
    "oneapi-target-5-conv.cpp" "oneapi-target-5-run.cpp" "test.fpga_emu" "$EMU_DEFINES"
    "oneapi-target-6-capsule.cpp" "oneapi-target-6-run.cpp" "test.fpga_emu" "$EMU_DEFINES"
)

FPGA=(
    "oneapi-target-4-gemm.cpp" "oneapi-target-4-run.cpp" "test.fpga" "$FPGA_DEFINES"
    "oneapi-target-5-conv.cpp" "oneapi-target-5-run.cpp" "test.fpga" "$FPGA_DEFINES"
    "oneapi-target-6-capsule.cpp" "oneapi-target-6-run.cpp" "test.fpga" "$FPGA_DEFINES"
)

succ=0
fail=0


function test_func {
    eval file="$1"
    eval run_file="$2"
    eval exec_name="$3"
    eval defs="$4"
    eval emu_var="$5"

    ## Needed Files
    # support_files="../../../src/AOT-OneAPI-Runtime.cpp ../../../src/Roofline.cpp  ../../../src/SharedUtilsInC.cpp "
    support_files="../../../src/Roofline.cpp  ../../../src/SharedUtilsInC.cpp "

    ## T2S Compile Flags
    # flags="-I ../../../src/ -I ../util  -I $T2S_PATH/Halide/include -L $T2S_PATH/Halide/bin $EMULATOR_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -lHalide "
    # flags="-I ../../../src/ -I ../util  -I $T2S_PATH/Halide/include -L $T2S_PATH/Halide/bin $HW_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -lHalide "
    flags="-I ../../../src/ -I ../util  -I $T2S_PATH/Halide/include -L $T2S_PATH/Halide/bin -lz -lpthread -ldl -std=c++11 -lHalide "
    flags+="$defs "

    ## BOARD
    eval board="$FPGA_BOARD"
    printf "BOARD: $board\n"
    if [ $board = "pac_a10" ] 
    then 
        board="intel_a10gx_pac:pac_a10"
    elif [ $board = "pac_s10" ] 
    then 
        board="intel_s10sx_pac:pac_s10"
    elif [ $board = "pac_s10_dc" ]
    then 
        board="intel_s10sx_pac:pac_s10"
    else
        printf "FOUND BOARD HERE\n"
        board="intel_a10gx_pac:pac_a10"
    fi


    ## DPC++ Compile Flags
    ### Emulation
    # dpcpp_flags="-I ../../../src/ -I ../util  -I $T2S_PATH/Halide/include -L $T2S_PATH/Halide/bin -lz -lpthread -ldl -lHalide "
    dpcpp_flags="-lz -lpthread -ldl "
    dpcpp_flags+="-fintelfpga -fsycl -fsycl-device-code-split=off "
    dpcpp_flags+="$defs "
    ### Report
    dpcc_report_flags="$dpcpp_flags -fsycl-link=early -Xshardware -Xsboard=$board"
    ### FPGA
    dpcc_fpga_flags="$dpcpp_flags -Xshardware -Xsboard=$board"



    ## T2S Compile command 
    compile="g++ $file $flags"

    ## DPC++ Compile command
    # compile_oneapi="dpcpp $support_files $run_file $dpcpp_flags -o ./test.fpga_emu"     # compile_emulation i.e. 
    compile_oneapi="dpcpp $run_file $dpcpp_flags -o ./test.fpga_emu"     # compile_emulation i.e. 
    # compile_oneapi="dpcpp $run_file $dpcc_report_flags"                # compile_report
    if [ "$emu_var" = "FPGA" ] 
    then
        compile_oneapi="dpcpp $run_file $dpcc_fpga_flags -o ./test.fpga"   # compile_fpga
    fi
    


    ## Clean Command
    clean="rm -rf exec_time.txt *.png "
    # clean="rm -rf a a.out $file.aoc* $file.cl exec_time.txt *.png *.generated_oneapi_header.h *.fpga_emu *.fpga "
    # clean="echo clean"

    ## Print Commands for debugging purposes
    printf "COMPILE T2S COMMAND:\n"
    printf "\t$compile\n\n"
    printf "COMPILE ONEAPI COMMAND: \n"
    printf "\t$compile_oneapi\n\n"
    # return;

    # Clean up directory
    $clean


    #########################################################################

    printf "\tCompile T2S \n"
    $compile >& a
    if [ -f "a.out" ]; then
        printf "\tRun T2S Executible \n"
        ./a.out >& a
        if  tail -n 1 a | grep -q -E "^Success"; then
            printf "\tCompile OneAPI \n"
            $compile_oneapi >& a
            if [ -f "$exec_name" ]; then
                printf "\tRun OneAPI Executible \n"
                ./$exec_name >& a
                if  tail -n 1 a | grep -q -E "^Success"; then
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
                echo $run >> failure.txt
                cat a >> failure.txt
                let fail=fail+1
                echo "Failure!"
            fi

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

    # Clean up directory
    $clean
}
        

array_to_read=("${EMU[@]}")
echo "Testing OneAPI-Integration"
echo "  usage ./test.sh (default:EMU|FPGA)"
rm -f success.txt failure.txt
index=0
emu_var=$1

# USE EITHER FPGA OR EMULATOR
if [ $# -ge 1 ]
then
    if [ $1 = "FPGA" ] 
    then 
        printf "  USING FPGA\n"
        array_to_read=("${FPGA[@]}")
    elif [ $1 = "EMU" ]
    then
        printf "  USING EMULATOR\n"
    else 
        printf "  UNKNOWN ARGUMENT $1. USING EMULATOR\n"
    fi
else
    printf "  USING EMULATOR\n"
fi

# RUN TEST FUNCTION
while [ "$index" -lt "${#array_to_read[*]}" ]; do
    file=${array_to_read[$index]}
    run_file=${array_to_read[$((index+1))]}
    exec_name=${array_to_read[$((index+2))]}
    define_flags=${array_to_read[$((index+3))]}
    let index=index+4
    printf "Case: $file $run_file $exec_name $define_flags\n"
    test_func "\${file}" "\${run_file}" "\${exec_name}" "\${define_flags}" "\${emu_var}"
done

let total=succ+fail
echo -e Total $total, pass ${GREEN}$succ${NOCOLOR}, fail ${RED}$fail${NOCOLOR}. See $PWD/success.txt and failure.txt for details.

# Return values for the parent script
echo $total > ../total.temp
echo $succ > ../succ.temp
echo $fail > ../fail.temp
