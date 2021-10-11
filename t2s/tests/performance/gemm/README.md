# Matrix Multiply

## Performance for single precision matrix multiply
| Device | Frequency | Throughput | LUTs/ALMs | DSPs | BRAMs | DSP Efficiency |
| ------ | --------- | ------ | --------- | ---- | ----- | -------------- |
| Intel Arria 10 GX 1150 FPGA | 201 MHz | 504 GFLOPS | 49% | 85% | 53% | 97%   |

## Key Implementation Details

This design contains several important optimizations:
- Generating a 10x8 systolic array with UREs and a space-time transform
- Generating double buffers for input data, with multiple banks connected as a daisy chain
- Vectorizing input data to utilize the bandwidth of the device's DRAM
- Generating two-level output buffers in registers

The compiler implements the above optimizations. In addition, the compiler automatically performs the following optimizations so that the generated code lends itself to the downstream EDA tools for further optimizations:
- Identifying and rewriting the inner-product pattern
- Moving  channel accesses at the bounaries of the systolic array outside the unrolled loops

## Building the design

The design can be built in JIT (just-in-time) or AOT (ahead-of-time) mode.

### JIT Mode
1. Set up the environments in the T2SP directory:
    ```
    source setenv.sh (local | devcloud)
    ```
2. Run emulation for verifying correctness with tiny inputs:
- Just to be safe, remove previously generated bitstream and intermediate files, if any.
    ```
    rm -rf a.aocx a/
    ```
- Compile the source code in this directory:
  
    ```
    g++ gemm.cpp -g -I ../util -I $T2S_PATH/Halide/include -L $T2S_PATH/Halide/bin $EMULATOR_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -DTINY
    ```
- Generate and emulate a bitstream:
    ```
    env BITSTREAM=a.aocx CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA=1 INTEL_FPGA_OCL_PLATFORM_NAME="$EMULATOR_PLATFORM" AOC_OPTION="$EMULATOR_AOC_OPTION -board=$FPGA_BOARD -emulator-channel-depth-model=strict" ./a.out
    ```
3. Synthesize and run for performance with large inputs:
- Just to be safe, remove previously generated bitstream and intermediate files, if any.
    ```
    rm -rf a.aocx a/
    ```
- Re-compile the source code with large size:  
    ```
    g++ gemm.cpp -g -I ../util -I $T2S_PATH/Halide/include -L $T2S_PATH/Halide/bin $HW_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -DCOMPILE_ONLY
    ```
- Generate and  run the bitstream on an FPGA:
    ```
    env BITSTREAM=a.aocx INTEL_FPGA_OCL_PLATFORM_NAME="$HW_RUN_PLATFORM_NAME" AOC_OPTION="-board=$FPGA_BOARD -v -profile -fpc -fp-relaxed" ./a.out
    ```

### AOT Mode
1. Set up the environments in the T2SP directory:
    ```
    source setenv.sh (local | devcloud a10)
    ```
2. Run emulation for verifying correctness with tiny inputs:
- Just to be safe, remove previously generated bitstream and intermediate files, if any.
  
    ```
    rm -rf a.aocx a/
    ```
- Compile the source code in this directory:
    ```
    g++ gemm.cpp -g -I ../util -I $T2S_PATH/Halide/include -L $T2S_PATH/Halide/bin $EMULATOR_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -DTINY -DAOT
    ```
- Generate a device bitstream for emulation, and a C interface for the host to invoke the matrix multiply kernel in the bitstream:
    ```
    rm -f a.aocx
    env BITSTREAM=a.aocx AOC_OPTION="$EMULATOR_AOC_OPTION -board=$FPGA_BOARD -emulator-channel-depth-model=strict" ./a.out
    ```
    
     The bitstream generated is `a.aocx`, as indicated by the environment variable BITSTREAM.  The C interface generated is `gemm-interface.cpp`, as specified in `gemm.cpp`.
    
- Compile the host file (`gemm_run.cpp`) and link with the C interface (`gemm-interface.cpp`):
  
    ```
    g++ gemm_run.cpp gemm-interface.cpp ../../../src/AOT-OpenCL-Runtime.cpp -g -DLINUX -DALTERA_CL -fPIC -I../../../src/ -I $T2S_PATH/Halide/include -I$INTELFPGAOCLSDKROOT/examples_aoc/common/inc $INTELFPGAOCLSDKROOT/examples_aoc/common/src/AOCLUtils/opencl.cpp $INTELFPGAOCLSDKROOT/examples_aoc/common/src/AOCLUtils/options.cpp -I$INTELFPGAOCLSDKROOT/host/include -L$INTELFPGAOCLSDKROOT/linux64/lib -L$AOCL_BOARD_PACKAGE_ROOT/linux64/lib -L$INTELFPGAOCLSDKROOT/host/linux64/lib -lOpenCL -L $T2S_PATH/Halide/bin -lelf $EMULATOR_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -DTINY -o ./b.out
    ```
    
- Emulate:
    ```
    env BITSTREAM=a.aocx CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA=1 INTEL_FPGA_OCL_PLATFORM_NAME="$EMULATOR_PLATFORM" ./b.out
    ```
3. Synthesize and run for performance with large inputs:
- Just to be safe, remove previously generated bitstream and intermediate files, if any.
  
    ```
    rm -rf a.aocx a/
    ```
- Re-compile the source code with large size:
    ```
    g++ gemm.cpp -g -I ../util -I $T2S_PATH/Halide/include -L $T2S_PATH/Halide/bin $HW_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -DAOT
    ```
- Generate a device bitstream, and a C interface for the host to invoke the matrix multiply kernel in the bitstream:
    - First, run the compiled binary.
        ```
        env BITSTREAM=a.aocx ./a.out
        ```
        In AOT mode, this command produces an intermediate OpenCL file (`a.cl`) instead of a bitstream.
    - Second, static analysis:
        ```
        aoc -rtl -report -board="$FPGA_BOARD" a.cl
        ```
        Look for the static analysis report in `a/reports/report.html`. For the options of the `aoc` command, refer to the [Best Practice Guide](https://www.intel.com/content/dam/www/programmable/us/en/pdfs/literature/hb/opencl-sdk/aocl-best-practices-guide.pdf).
    - Third, synthesis:
        ```
        aoc -v -profile -fpc -fp-relaxed -board="$FPGA_BOARD" a.cl
        ```
        This generates the bitstream `a.aocx`.
- Compile the host file (`gemm_run.cpp`) and link with the C interface (`gemm-interface.cpp`):
    ```
    g++ gemm_run.cpp gemm-interface.cpp ../../../src/AOT-OpenCL-Runtime.cpp -g -DLINUX -DALTERA_CL -fPIC -I../../../src/ -I $T2S_PATH/Halide/include -I$INTELFPGAOCLSDKROOT/examples_aoc/common/inc $INTELFPGAOCLSDKROOT/examples_aoc/common/src/AOCLUtils/opencl.cpp $INTELFPGAOCLSDKROOT/examples_aoc/common/src/AOCLUtils/options.cpp -I$INTELFPGAOCLSDKROOT/host/include -L$INTELFPGAOCLSDKROOT/linux64/lib -L$AOCL_BOARD_PACKAGE_ROOT/linux64/lib -L$INTELFPGAOCLSDKROOT/host/linux64/lib -lOpenCL -L $T2S_PATH/Halide/bin -lelf $EMULATOR_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -o ./b.out
    ```
- Run the host binary. It will offload the bitstream to an FPGA and invoke the matrix multiply kernel there:
    ```
    env BITSTREAM=a.aocx INTEL_FPGA_OCL_PLATFORM_NAME="$HW_RUN_PLATFORM_NAME" AOC_OPTION="-board=$FPGA_BOARD" ./b.out
    ```