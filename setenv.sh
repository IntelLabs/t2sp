#!/bin/bash

function show_usage {
    echo "DevCloud usage:"
    echo "  source setenv.sh devcloud (fpga | gpu) (gen9 | gen12)"
    echo "Local usage:"
    echo "  source setenv.sh local    (fpga | gpu) (gen9 | gen12)"
}       

function setup_dpcpp_devcloud {
    if ! command -v dpcpp &> /dev/null
    then
        echo "sourcing dpcpp setup script"

        # Using default configuration 
        source /glob/development-tools/versions/oneapi/2022.1.2/oneapi/setvars.sh 
        
        # Using config file
        # (NOTE) config file needs modification
        # source /glob/development-tools/versions/oneapi/2022.1.2/oneapi/setvars.sh --config="${T2S_PATH}/oneapi_config.txt" 
    else
        echo "dpcpp command exists"
    fi
}

if [ $0 == $BASH_SOURCE ]; then
   echo "This script should be sourced, not run."
   exit
fi 

if [ "$1" != "devcloud" -a "$1" != "local" ]; then
    show_usage
    return 
fi

if [ "$1" == "devcloud" ]; then
    if [ "$HOSTNAME" == "login-2" ]; then
        echo "Error: The script should be run on a compute node of DevCloud or a local machine, not on the head node of DevCloud (login-2)."
        return
    fi
fi

if [ "$2" != "fpga" -a "$2" != "gpu" ]; then
    show_usage
    return 
fi

export T2S_PATH="$( cd "$(dirname $(realpath "$BASH_SOURCE") )" >/dev/null 2>&1 ; pwd -P )" # The path to this script
TOOLS_PATH=$HOME/old-t2sp/install

# Modify these 3 paths if you installed your own versions of gcc or llvm-clang
# gcc should be located at $GCC_PATH/bin
GCC_PATH=$TOOLS_PATH/gcc-7.5.0
export LLVM_CONFIG=$TOOLS_PATH/bin/llvm-config
export CLANG=$TOOLS_PATH/bin/clang

if [ "$1" = "local" -a "$2" = "fpga" ]; then
    # Modify according to your machine's setting
    ALTERA_PATH=$HOME/intelFPGA_pro
    AOCL_VERSION=19.1
    FPGA_BOARD_PACKAGE=a10_ref
    export FPGA_BOARD=a10gx
    export LM_LICENSE_FILE=1800@altera02p.elic.intel.com
fi

#### No need to change below this point ##########

if [ "$2" = "fpga" ]; then
    if [ "$1" = "local" ]; then
        # Intel OpenCL related setting
        export ALTERAOCLSDKROOT=$ALTERA_PATH/$AOCL_VERSION/hld
        export INTELFPGAOCLSDKROOT=$ALTERAOCLSDKROOT
        export AOCL_SO=$ALTERAOCLSDKROOT/host/linux64/lib/libalteracl.so
        export AOCL_BOARD_SO=$ALTERA_PATH/$AOCL_VERSION/hld/board/${FPGA_BOARD_PACKAGE}/linux64/lib/libaltera_${FPGA_BOARD_PACKAGE}_mmd.so
        export AOCL_LIBS="-L$ALTERA_PATH/$AOCL_VERSION/hld/board/${FPGA_BOARD_PACKAGE}/host/linux64/lib -L$ALTERA_PATH/$AOCL_VERSION/hld/board/${FPGA_BOARD_PACKAGE}/linux64/lib -L$ALTERA_PATH/$AOCL_VERSION/hld/host/linux64/lib -Wl,--no-as-needed -lalteracl -laltera_${FPGA_BOARD_PACKAGE}_mmd"
        export QSYS_ROOTDIR=$ALTERAOCLSDKROOT/../qsys/bin
        export QUARTUS_ROOTDIR_OVERRIDE=$ALTERAOCLSDKROOT/../quartus
        export LD_LIBRARY_PATH=$ALTERAOCLSDKROOT/host/linux64/lib:$ALTERAOCLSDKROOT/board/${FPGA_BOARD_PACKAGE}/linux64/lib:$LD_LIBRARY_PATH
        export PATH=$QUARTUS_ROOTDIR_OVERRIDE/bin:$ALTERAOCLSDKROOT/bin:$PATH
        source $ALTERAOCLSDKROOT/init_opencl.sh
        unset CL_CONTEXT_EMULATOR_DEVICE_ALTERA
        export EMULATOR_LIBHALIDE_TO_LINK="-lHalide"
        export HW_LIBHALIDE_TO_LINK="-lHalide"
    fi

    if [ "$1" = "devcloud" ]; then
        if [ -f /data/intel_fpga/devcloudLoginToolSetup.sh ]; then
            source /data/intel_fpga/devcloudLoginToolSetup.sh
        fi
        pbsnodes $(hostname) >& tmp.txt
        if grep "fpga,arria10" tmp.txt; then
        if grep "fpga_opencl" tmp.txt; then
              tools_setup -t A10DS 1.2.1
        fi
            export FPGA_BOARD=pac_a10
        else
            if grep "fpga,stratix10" tmp.txt; then
        if grep "fpga_opencl" tmp.txt; then
                  tools_setup -t S10DS
        fi
                export FPGA_BOARD=pac_s10_dc
            else
                echo The current compute node does not have either an A10 or an S10 card with it.
                echo Please choose another compute node from 'Nodes with Arria 10 Release 1.2.1',
                echo or 'Nodes with Stratix 10'
                return
            fi
        fi
        export AOCL_SO=$ALTERAOCLSDKROOT/host/linux64/lib/libalteracl.so
        export AOCL_BOARD_SO=$AOCL_BOARD_PACKAGE_ROOT/linux64/lib/libintel_opae_mmd.so
        #export AOCL_LIBS="-L$INTELFPGAOCLSDKROOT/linux64/lib -L$AOCL_BOARD_PACKAGE_ROOT/linux64/lib -L$INTELFPGAOCLSDKROOT/host/linux64/lib -Wl,--no-as-needed -lalteracl  -lintel_opae_mmd"
    export AOCL_LIBS="$(aocl link-config)"

        # The aoc on DevCloud has an LLVM whose version is different from that of the LLVM libHalide.so has linked with. Consequently, calling
        # aoc dynamically will have two LLVMs messed up: the aoc has some LLVM calls to some symbols that are not resolved to be in the aoc linked
        # LLVM, but instead the libHalide.so linked LLVM, because libHalide.so was loaded the first.
        # Using the static libHalide.a instead of th dynamic libHalide.so is the only solution we found so far.
        export EMULATOR_LIBHALIDE_TO_LINK="$T2S_PATH/Halide/lib/libHalide.a"
        export HW_LIBHALIDE_TO_LINK="$T2S_PATH/Halide/lib/libHalide.a $AOCL_LIBS"

        source  /glob/development-tools/versions/intel-parallel-studio-2019/debugger_2019/bin/debuggervars.sh
        alias gdb='gdb-ia'
    fi

    # Figure out the emulator and hardware run platform
    AOC_VERSION=$(aoc -version | grep -E 'Version\ [^[:space:]]*' -o  |grep -E '[0-9]+[\.[0-9]+]*' -o)
    OLDER_VERSION=0
    if [ "$AOC_VERSION" != "19.4.0" ]; then
        if [ $(printf '%s\n' "$AOC_VERSION" "19.4.0" | sort -V | head -n1) == "$AOC_VERSION" ]; then
            # aoc is a version older than 19.4.0.
            OLDER_VERSION=1
        fi
    fi
    if [ "$OLDER_VERSION" == "1" ]; then
        export EMULATOR_PLATFORM="Intel(R) FPGA SDK for OpenCL(TM)"
    else
        export EMULATOR_PLATFORM="Intel(R) FPGA Emulation Platform for OpenCL(TM)"
    fi
    export HW_PLATFORM="Intel(R) FPGA SDK for OpenCL(TM)"

    # Figure out aoc options for emulator
    if [ "$AOC_VERSION" != "19.2.0" ]; then
        export EMULATOR_AOC_OPTION="-march=emulator"
    else
        export EMULATOR_AOC_OPTION="-march=emulator -legacy-emulator"
    fi
fi

if [ "$2" = "gpu" ]; then
    if [ "$3" = "gen9" ]; then
        export CM_ROOT=$T2S_PATH/install/Linux_C_for_Metal_Development_Package_20200119
        export LIBVA_DRIVERS_PATH=$CM_ROOT/drivers/media_driver/release/extract/usr/lib/x86_64-linux-gnu/dri
        export PATH=$CM_ROOT/compiler/bin:$PATH
        export LD_LIBRARY_PATH=$CM_ROOT/drivers/IGC/extract/usr/local/lib:$CM_ROOT/drivers/media_driver/release/extract/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH
    fi
    export HW_LIBHALIDE_TO_LINK="-lHalide"
fi

#A place to store generated Intel OpenCL files
mkdir -p ~/tmp

# Add tools
export PATH=$TOOLS_PATH/bin:$PATH
export LD_LIBRARY_PATH=$TOOLS_PATH/lib64:$TOOLS_PATH/lib:$LD_LIBRARY_PATH

# Add gcc
if [ "$1" != "devcloud" -o "$2" != "gpu" ]; then
    export PATH=$GCC_PATH/bin:$PATH
    export LD_LIBRARY_PATH=$GCC_PATH/bin:$GCC_PATH/lib64:$LD_LIBRARY_PATH
fi

# Add Halide
export PATH=$T2S_PATH/Halide/bin:$PATH
export LD_LIBRARY_PATH=$T2S_PATH/Halide/bin:$LD_LIBRARY_PATH

# Common options for compiling a specification
export COMMON_OPTIONS_COMPILING_SPEC="-I $T2S_PATH/Halide/include -L $T2S_PATH/Halide/bin -lz -lpthread -ldl -std=c++11"

# Common options for running a specification to synthesize a kernel for emulation or execution
export COMMON_AOC_OPTION_FOR_EMULATION="$EMULATOR_AOC_OPTION -board=$FPGA_BOARD -emulator-channel-depth-model=strict"
export COMMON_AOC_OPTION_FOR_EXECUTION="-v -profile -fpc -fp-relaxed -board=$FPGA_BOARD"

# Common options for comping a host file
export COMMON_OPTIONS_COMPILING_HOST="$T2S_PATH/t2s/src/AOT-OpenCL-Runtime.cpp $T2S_PATH/t2s/src/Roofline.cpp $T2S_PATH/t2s/src/SharedUtilsInC.cpp -DLINUX -DALTERA_CL -fPIC -I$T2S_PATH/t2s/src/ -I $T2S_PATH/Halide/include -I$INTELFPGAOCLSDKROOT/examples_aoc/common/inc $INTELFPGAOCLSDKROOT/examples_aoc/common/src/AOCLUtils/opencl.cpp $INTELFPGAOCLSDKROOT/examples_aoc/common/src/AOCLUtils/options.cpp -I$INTELFPGAOCLSDKROOT/host/include -L$INTELFPGAOCLSDKROOT/linux64/lib -L$AOCL_BOARD_PACKAGE_ROOT/linux64/lib -L$INTELFPGAOCLSDKROOT/host/linux64/lib -lOpenCL -L $T2S_PATH/Halide/bin -lelf -lz -lpthread -ldl -std=c++11"
export COMMON_OPTIONS_COMPILING_HOST_FOR_EMULATION="$COMMON_OPTIONS_COMPILING_HOST $EMULATOR_LIBHALIDE_TO_LINK"
export COMMON_OPTIONS_COMPILING_HOST_FOR_EXECUTION="$COMMON_OPTIONS_COMPILING_HOST $HW_LIBHALIDE_TO_LINK"

