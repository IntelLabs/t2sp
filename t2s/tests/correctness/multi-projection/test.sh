#!/bin/bash
# ./test.sh

RED='\033[0;31m'
GREEN='\033[0;32m'
NOCOLOR='\033[0m'

# In this array, every element contains:
# Test file, gcc option 
regression=(
        1dconv-bfs.cpp     ""
        1dconv-bsm.cpp     ""
        1dconv-fbs-1.cpp   ""
        1dconv-fbs-2.cpp   ""
        1dconv-ffs.cpp     ""
        1dconv-fsm.cpp     ""
        1dconv-sbm.cpp     ""
        2dconv-fbs.cpp     ""
        2dconv-sbm.cpp     ""
        2D-loop-1-4.cpp    ""
        2D-loop-2-3.cpp    ""
        2D-loop-3-3.cpp    ""
        2D-loop-4-4.cpp    ""
        3D-loop-1-2-8.cpp  ""
        3D-loop-2-1-2.cpp  "-DCASE1"
        3D-loop-2-2-4.cpp  "-DCASE1"
        3D-loop-2-2-4.cpp  "-DCASE2"
        3D-loop-2-2-4.cpp  "-DCASE3"
        3D-loop-3-2-6.cpp  "-DCASE1"
        3D-loop-3-2-6.cpp  "-DCASE2"
        3D-loop-3-2-6.cpp  "-DCASE4"
        # 4D-loop-BMA.cpp    ""      # TOFIX: mins of u, v are negative, which seems to cause some issues.
        capsule.cpp        ""
)

succ=0
fail=0

function emulate_func {
    eval file="$1"
    eval gcc_option="$2"
    printf "$file $gcc_option "
    compile="   g++ $file $gcc_option -g -I ../util  -I ../../../../Halide/include -L ../../../../Halide/bin $EMULATOR_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 "
    rm -rf a a.out
    $compile >& a
    if [ -f "a.out" ]; then
        # There is an error "Unterminated quoted string" using $run due to AOC_OPTION. To avoid it, explicitly run for every case.
        rm -f a
        run="./a.out"
        timeout 5m $run >& a
        if  tail -n 1 a | grep -q -E "^Success!"; then
            echo >> success.txt
            echo "rm -rf a a.out" >> success.txt
            echo $compile >> success.txt
            echo $run >> success.txt
            cat a >> success.txt
            let succ=succ+1
            echo " Success!"
        else
            echo >> failure.txt
            echo "rm -rf a a.out" >> failure.txt
            echo $compile >> failure.txt
            echo $run >> failure.txt
            cat a >> failure.txt
            let fail=fail+1
            echo "Failure!"
        fi
    else
        echo >> failure.txt
        echo "rm -rf a a.out" >> failure.txt
        echo $compile >> failure.txt
        cat a >> failure.txt
        let fail=fail+1
        echo " Failure!"
    fi 
    rm -f a a.out exec_time.txt 1dconv-fbs-1 capsule 
}
        
rm -f success.txt failure.txt

array_to_read=("${regression[@]}")
echo "Testing multi-prejection for regression."

index=0
while [ "$index" -lt "${#array_to_read[*]}" ]; do
    file=${array_to_read[$index]}
    gcc_option=${array_to_read[$((index+1))]}
    let index=index+2
    emulate_func "\${file}" "\${gcc_option}"
done

let total=succ+fail
echo -e Total $total, pass ${GREEN}$succ${NOCOLOR}, fail ${RED}$fail${NOCOLOR}. See $PWD/success.txt and failure.txt for details.

# Return values for the parent script
echo $total > ../total.temp
echo $succ > ../succ.temp
echo $fail > ../fail.temp
