#!/bin/bash

function show_usage {
    echo "Options: (devcloud|local) (oneapi|opencl|cm) (gemm|conv|capsule|pairhmm|qrd) (a10|s10|gen9|gen12) (tiny|large) (hw|emulator) [bitstream]"
}

if [ $0 == $BASH_SOURCE ]; then
   echo "This script should be sourced, not run."
   exit
fi 

wrong_options=1

if [ "$1" != "devcloud" -a "$1" != "local" ]; then
    show_usage
    return
else
    location="$1"
fi

if [ "$2" != "oneapi" -a "$2" != "opencl" -a "$2" != "cm" ]; then
    show_usage
    exit
else
    language="$2"
fi

if [ "$3" != "gemm" -a "$3" != "conv"  -a  "$3" != "capsule" -a "$3" != "pairhmm" -a "$3" != "qrd" ]; then
    show_usage
    return
else
    workload="$3"
fi

if [ "$4" != "a10" -a "$4" != "s10"  -a  "$4" != "gen9" -a  "$4" != "gen12" ]; then
    show_usage
    return
else
    target="$4"
fi

if [ "$5" != "tiny" -a "$5" != "large" ]; then
    show_usage
    return
else
    size="$5"
fi

if [ "$6" != "hw" -a "$6" != "emulator" ]; then
    show_usage
    return
else
    platform="$6"
fi

if [ "$7" != "" ]; then
    # Add prefix to the bitstream
    bitstream="$7/$4/a.aocx"
    echo "Use the bitstream ${bitstream}"
fi

if [ "$platform" == "emulator" ]; then
    if [ "$target" != "a10" -a "$target" != "s10" ]; then
        show_usage
        echo "Note: The emulator option is applicable only to FPGAs and tiny size"
        return
    elif [ "$size" != "tiny" ]; then
        show_usage
        echo "Note: The emulator option is applicable only to FPGAs and tiny size"
        return
    fi
fi

wrong_options=0
