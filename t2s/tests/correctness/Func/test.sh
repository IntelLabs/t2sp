#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
NOCOLOR='\033[0m'

# The script can be executed like this:
# source test.sh [1] [2] [3]
# The first arguement means the big feature that be tested, 1: declare-func, 2: define-func, 3: ure
# The second arguement means the sub-class of one feature.
# The third arguement will appoint a specific case.
# If there are no arguements, all the test cases will be evaluated.
# The test result will be stored in success.txt and failure.txt

prefix_array=("not_here" "declare-func" 'define-func' 'ure' 'typical')

regression=(
        declare-func-1-1.cpp
        declare-func-1-2.cpp
        declare-func-1-3-negative.cpp 
        declare-func-2-1.cpp
        declare-func-2-2.cpp
        declare-func-2-3.cpp
        declare-func-2-4.cpp
        declare-func-2-5-negative.cpp 
        declare-func-2-6-negative.cpp 
        define-func-1-1.cpp
        define-func-1-2.cpp
        define-func-1-6.cpp
        define-func-1-9-negative.cpp 
        define-func-2-1.cpp
        define-func-2-2.cpp
        define-func-2-3.cpp
        define-func-2-4.cpp
        define-func-3-1.cpp
        define-func-3-2-negative.cpp 
        define-func-3-3-negative.cpp 
        define-func-3-4.cpp
        define-func-4-1.cpp
        ure-1-1.cpp
        ure-1-2.cpp
        ure-1-4-negative.cpp 
        ure-1-5-negative.cpp 
        ure-1-6-negative.cpp 
        ure-1-8.cpp 
        ure-1-9.cpp
        ure-2-1-negative.cpp 
        ure-2-4-negative.cpp  
        ure-3-1.cpp
        ure-4-1.cpp
        ure-4-2.cpp
        ure-4-3.cpp
        ure-4-4.cpp
        ure-4-8.cpp
        ure-5-1.cpp
        ure-5-2.cpp
        ure-5-3.cpp
        ure-7-3-negative.cpp 
        ure-7-4-negative.cpp 
        ure-7-5-negative.cpp 
        ure-7-6-negative.cpp 
        ure-8-1-negative.cpp 
        ure-8-2-negative.cpp 
        ure-8-3-negative.cpp 
        ure-8-4.cpp
        ure-9-4-negative.cpp 
        ure-9-5-negative.cpp 
       )

file_array=("${regression[@]}")
echo "Testing Func for regression."

succ=0
fail=0

function test_func {
    eval file="$1"
    compile="g++ $file -g -I ../util  -I ../../../../Halide/include -L ../../../../Halide/bin $EMULATOR_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -DSIZE=10  -DVERBOSE_DEBUG"
    # Be sure to run this script after source setenv.sh so there is no environment issue.
    run="./a.out"
    clean="rm -rf a a.out $HOME/tmp/a.aocx $HOME/tmp/a.aocr $HOME/tmp/a.aoco $HOME/tmp/a.cl $HOME/tmp/a"
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
            echo " Failure!"
        fi
    else
            echo >> failure.txt
            echo $clean >> failure.txt
            echo $compile >> failure.txt
            cat a >> failure.txt
            let fail=fail+1
            echo " Failure!"
    fi 
}
        
rm -f success.txt failure.txt
index=0
while [ "$index" -lt "${#file_array[*]}" ]; do
   file=${file_array[$index]}
   let index=index+1

   printf "Case: $file"
   test_func "\${file}"
done

let total=succ+fail
echo -e Total $total, pass ${GREEN}$succ${NOCOLOR}, fail ${RED}$fail${NOCOLOR}. See $PWD/success.txt and failure.txt for details.

# Return values for the parent script
echo $total > ../total.temp
echo $succ > ../succ.temp
echo $fail > ../fail.temp