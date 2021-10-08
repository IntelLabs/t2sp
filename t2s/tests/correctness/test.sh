#!/bin/bash

# Usage:
#   ./test.sh clean  # remove temporary files
# or
#   ./test.sh        # regression tests

if [ "$1" == "clean" ]; then
    find . -name \*.swp -type f -delete
    find . -name success.txt -type f -delete
    find . -name failure.txt -type f -delete
    find . -name profile_info.txt -type f -delete
    find . -name \*.temp -type f -delete
    find . -name a -type f -delete
    find . -name a.out -type f -delete
    rm -rf overlay/test.*.temp
    rm -f multi-projection/1dconv_fbs_1 multi-projection/capsule roofline/roofline.png
    exit
fi

RED='\033[0;31m'
GREEN='\033[0;32m'
NOCOLOR='\033[0m'

features=(aot buffer FPGA Func gather gemm integrate isolation LU multi-projection overlay qrd roofline scatter space-time-transform vectorize)
echo "**** Testing for regression ****"

index=0
total_tests=0
total_succ=0
total_fail=0
while [ "$index" -lt "${#features[*]}" ]; do
   feature=${features[$index]}
   let index=index+1 
   rm -f total.temp succ.temp fail.temp
   echo 
   (cd $feature ; ./test.sh; cd - )
   tests=`cat total.temp`
   succ=`cat succ.temp`
   fail=`cat fail.temp`
   let total_tests=total_tests+tests
   let total_succ=total_succ+succ
   let total_fail=total_fail+fail
done
rm -f total.temp succ.temp fail.temp

echo ----------------------------
echo -e Total $total_tests, pass ${GREEN}$total_succ${NOCOLOR}, fail ${RED}$total_fail${NOCOLOR}.
