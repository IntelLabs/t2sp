#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
NOCOLOR='\033[0m'

# In this array, every element contains:
# Test file
# design
regression=(
              2D-loop-1-1.cpp 1
              2D-loop-1-2.cpp 1
              2D-loop-1-3-negative.cpp 1
              2D-loop-1-4.cpp 1
              2D-loop-2-1.cpp 1
              2D-loop-2-4.cpp 1
              2D-loop-3-1.cpp 1
              2D-loop-3-4.cpp 1
              2D-loop-4.cpp 1
              2D-loop-5.cpp 1
              2D-loop-6.cpp 1
              3D-loop-1-1.cpp 1
              3D-loop-1-4.cpp 1
              3D-loop-1-5.cpp 1
              3D-loop-2-1.cpp 1
              3D-loop-2-4.cpp 1
              3D-loop-3-1.cpp 1
              3D-loop-3-2.cpp 1
              3D-loop-3-3-negative.cpp 1
              4D-loop-1-1.cpp 1
              4D-loop-1-2.cpp 1
              4D-loop-1-4.cpp 1
              4D-loop-1-5.cpp 1
              matrix-multiply-1-p.cpp 1
              matrix-multiply-1-p.cpp 2
              matrix-multiply-1-p.cpp 3
            # matrix-multiply-1-p.cpp 4
            # matrix-multiply-1-p.cpp 5
              cnn-2-p.cpp 4
              cnn-1-p.cpp 6
            # cnn-2-p.cpp 1
            # cnn-2-p.cpp 2
            # cnn-2-p.cpp 7
            # cnn-2-p.cpp 8
            # cnn-2-p.cpp 11
            # cnn-2-p.cpp 12
           )

succ=0
fail=0

function test_func {
    eval file="$1"
    eval design="$2"
    printf "$file design=$design "
    compile="    g++ $file -g -I ../util  -I ../../../../Halide/include -L ../../../../Halide/bin $EMULATOR_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -DSIZE=10  -DVERBOSE_DEBUG -DPLACE0=Place::Host -DPLACE1=Place::Host -DDESIGN=$design"
    run="./a.out"
    clean="rm -rf a a.out $HOME/tmp/a.aocx $HOME/tmp/a.aocr $HOME/tmp/a.aoco $HOME/tmp/a.cl $HOME/tmp/a exec_time.txt"
    $clean
    $compile >& a
    if [ -f "a.out" ]; then
        rm -f a; $run >& a
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

echo "Testing space-time transform."
rm -f success.txt failure.txt

array_to_read=("${regression[@]}")

index=0
while [ "$index" -lt "${#array_to_read[*]}" ]; do
   file=${array_to_read[$index]}
   design=${array_to_read[$((index+1))]}
   let index=index+2
   test_func "\${file}" "\${design}"
done

let total=succ+fail
echo -e Total $total, pass ${GREEN}$succ${NOCOLOR}, fail ${RED}$fail${NOCOLOR}. See $PWD/success.txt and failure.txt for details.

# Return values for the parent script
echo $total > ../total.temp
echo $succ > ../succ.temp
echo $fail > ../fail.temp
