#!/bin/bash

function cleanup {
    rm -rf a.* a/ ${workload}-interface.* *.out exec_time.txt *.png *.o *.isa ${workload}_genx.cpp signed* temp* profile.mon
}

function libhalide_to_link {
    if [ "$platform" == "emulator" ]; then
        lib="$EMULATOR_LIBHALIDE_TO_LINK"
    else
        lib="$HW_LIBHALIDE_TO_LINK"
    fi
    echo "$lib"
}

function aoc_options {
    if [ "$platform" == "emulator" ]; then
        aoc_opt="$EMULATOR_AOC_OPTION -board=$FPGA_BOARD -emulator-channel-depth-model=strict"
    else 
        aoc_opt="-v -profile -fpc -fp-relaxed -board=$FPGA_BOARD"
    fi
    echo "$aoc_opt"
}

function generate_fpga_kernel {
    # Compile the specification
    g++ ${workload}.cpp -g -I ../util -I $T2S_PATH/Halide/include -L $T2S_PATH/Halide/bin $(libhalide_to_link) -lz -lpthread -ldl -std=c++11 -D$size
    
    # Generate a device kernel, and a C interface for the host to invoke the kernel:
    # The bitstream generated is a.aocx, as indicated by the environment variable, BITSTREAM.
    # The C interface generated is ${workload}-interface.cpp, as specified in ${workload}.cpp.
    env BITSTREAM=a.aocx AOC_OPTION="$(aoc_options)" ./a.out

    # DevCloud A10PAC (1.2.1) only: further convert the signed bitstream to unsigned:
    if [ "$target" == "a10" -a "$platform" == "hw" ]; then
        cp a.aocx a_signed.aocx # Keep a signed copy in case the conversion fails below and we can look at the issue manually
        { echo "Y"; echo "Y"; echo "Y"; echo "Y"; } | source $AOCL_BOARD_PACKAGE_ROOT/linux64/libexec/sign_aocx.sh -H openssl_manager -i a.aocx -r NULL -k NULL -o a_unsigned.aocx 
        mv a_unsigned.aocx a.aocx
    fi
}

function test_fpga_kernel {
    # Compile the host file (${workload}-run-fpga.cpp) and link with the C interface (${workload}-interface.cpp):
    g++ ${workload}-run-fpga.cpp ${workload}-interface.cpp ../../../src/AOT-OpenCL-Runtime.cpp ../../../src/Roofline.cpp ../../../src/SharedUtilsInC.cpp  -g -DLINUX -DALTERA_CL -fPIC -I../../../src/ -I $T2S_PATH/Halide/include -I$INTELFPGAOCLSDKROOT/examples_aoc/common/inc $INTELFPGAOCLSDKROOT/examples_aoc/common/src/AOCLUtils/opencl.cpp $INTELFPGAOCLSDKROOT/examples_aoc/common/src/AOCLUtils/options.cpp -I$INTELFPGAOCLSDKROOT/host/include -L$INTELFPGAOCLSDKROOT/linux64/lib -L$AOCL_BOARD_PACKAGE_ROOT/linux64/lib -L$INTELFPGAOCLSDKROOT/host/linux64/lib -lOpenCL -L $T2S_PATH/Halide/bin -lelf $(libhalide_to_link) -D$size -lz -lpthread -ldl -std=c++11 -o ./b.out

    if [ "$platform" == "emulator" ]; then
        env BITSTREAM=a.aocx CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA=1 INTEL_FPGA_OCL_PLATFORM_NAME="$EMULATOR_PLATFORM" ./b.out
    else 
        # Offload the bitstream to the device.
        aocl program acl0 a.aocx  
    
        # Run the host binary. The host offloads the bitstream to an FPGA and invokes the kernel through the interface:
        env BITSTREAM=a.aocx INTEL_FPGA_OCL_PLATFORM_NAME="$HW_PLATFORM" ./b.out
    fi
}

function generate_test_fpga_kernel {
    source ../../../setenv.sh $location fpga
    cd $workload
    cleanup
    generate_fpga_kernel
    test_fpga_kernel
}

function gpu_iterations {
    # TOFIX: TINY in the GPU workloads do not seem to mean the input size for now,
    # but how many iterations of the test to repeat. Should change to reflect the input size.
    if [ "$size" == "TINY" ]; then
        iterations=1
    else
        iterations=100
    fi
    echo "$iterations"
}

function generate_gpu_kernel {
    if [ "$target" == "gen9" ]; then
        GPU_ARCH=SKL # This is for both GEN9 and GEN9.5 GPU
    else
        GPU_ARCH=TGLLP
    fi
    set -x 
    # Compile the specification
    g++ ${workload}.cpp -g -I ../util -I $T2S_PATH/Halide/include -L $T2S_PATH/Halide/bin $HW_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -DGPU

    # Run the specification to generate  a device kernel file ${workload}_genx.cpp
    ./a.out
    
    # Compile the kernel into binary
    cmc ${workload}_genx.cpp -fcmocl -mcpu=$GPU_ARCH -m64 -isystem ${CSDK_DIR}/usr/include -o ${workload}_genx.bin
}

function test_gpu_kernel {
    # Link the host and kernel code:
    g++ -DITER=$(gpu_iterations) -m64 -I${CSDK_DIR}/usr/include -I../util -L${CSDK_DIR}/usr/lib/x86_64-linux-gnu ${workload}-run-gpu.cpp -lze_loader -std=gnu++1z -o ${workload}-run-gpu.out

    # Run the host binary. The host offloads the kernel to a GPU:
    ./${workload}-run-gpu.out
}

function generate_test_gpu_kernel {
    source ../../../setenv.sh $location gpu
    cd $workload
    cleanup
    generate_gpu_kernel
    test_gpu_kernel
}
