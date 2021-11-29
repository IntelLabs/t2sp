#!/bin/bash

function show_usage {
    echo "Options: (devcloud|local) (gemm|conv|capsule) (a10|s10|gen9|gen12) (tiny|large) (hw|emulator)"
    echo "The emulator option is applicable only to FPGAs and tiny size"
}

wrong_options=1

if [ $0 != $BASH_SOURCE ]; then
    echo "Error: The script should be sourced"
    show_usage
    return
fi

if [ "$1" != "devcloud" -a "$1" != "local" ]; then
    show_usage
    return
else
    location="$1"
fi

if [ "$2" != "gemm" -a "$2" != "conv"  -a  "$2" != "capsule" ]; then
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

if [ "$platform" == "emulator" ]; then
    if [ "$target" != "a10" -a "$target" != "s10" ]; then
        show_usage
        return
    elif [ "$size" != "tiny" ]; then
        show_usage
        return
    fi
fi

wrong_options=0