#!/bin/bash

function show_usage {
    echo "Options: (devcloud|local) (gemm|conv|capsule|pairhmm|qrd) (a10|s10|gen9|gen12) (tiny|large) (hw|emulator) [bitstream]"
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

if [ "$2" != "gemm" -a "$2" != "conv"  -a  "$2" != "capsule" -a "$2" != "pairhmm" -a "$2" != "qrd"]; then
    show_usage
    return
else
    workload="$2"
fi

if [ "$3" != "a10" -a "$3" != "s10"  -a  "$3" != "gen9" -a  "$3" != "gen12" ]; then
    show_usage
    return
else
    target="$3"
fi

if [ "$4" != "tiny" -a "$4" != "large" ]; then
    show_usage
    return
else
    size="$4"
fi

if [ "$5" != "hw" -a "$5" != "emulator" ]; then
    show_usage
    return
else
    platform="$5"
fi

if [ "$6" != "" ]; then
    # Add prefix to the bitstream
    bitstream="$6/$3/a.aocx"
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
