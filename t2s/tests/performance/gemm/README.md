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
- Moving  channel accesses at the boundaries of the systolic array outside the unrolled loops

## Test the design

This design is specified to compile ahead-of-time (AOT), since AOT mode makes sense for the time-consuming FPGA compilation, although we can slightly change the specification to make it work just-in-time (JIT) as well.

### 1. Set up the environments in the T2SP directory:
```bash
   source setenv.sh (local | devcloud)
```
### 2. Run emulation for verifying correctness with a tiny systolic array and inputs:
- Just to be safe, remove previously generated bitstream and intermediate files, if any.
  
    ```
    rm -rf a.aocx a/
    ```
- Compile the source code in this directory:
    ```
    g++ gemm.cpp -g -I ../util -I $T2S_PATH/Halide/include -L $T2S_PATH/Halide/bin $EMULATOR_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -DTINY
    ```
- Generate a device bitstream for emulation, and a C interface for the host to invoke the matrix multiply kernel in the bitstream:
    ```
    env BITSTREAM=a.aocx AOC_OPTION="$EMULATOR_AOC_OPTION -board=$FPGA_BOARD -emulator-channel-depth-model=strict" ./a.out
    ```
    
     The bitstream generated is `a.aocx`, as indicated by the environment variable, BITSTREAM.  The C interface generated is `gemm-interface.cpp`, as specified in `gemm.cpp`.
    
- Compile the host file (`gemm_run.cpp`) and link with the C interface (`gemm-interface.cpp`):
  
    ```
    g++ gemm_run.cpp gemm-interface.cpp ../../../src/AOT-OpenCL-Runtime.cpp -g -DLINUX -DALTERA_CL -fPIC -I../../../src/ -I $T2S_PATH/Halide/include -I$INTELFPGAOCLSDKROOT/examples_aoc/common/inc $INTELFPGAOCLSDKROOT/examples_aoc/common/src/AOCLUtils/opencl.cpp $INTELFPGAOCLSDKROOT/examples_aoc/common/src/AOCLUtils/options.cpp -I$INTELFPGAOCLSDKROOT/host/include -L$INTELFPGAOCLSDKROOT/linux64/lib -L$AOCL_BOARD_PACKAGE_ROOT/linux64/lib -L$INTELFPGAOCLSDKROOT/host/linux64/lib -lOpenCL -L $T2S_PATH/Halide/bin -lelf $EMULATOR_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -DTINY -o ./b.out
    ```
- Emulate:
    ```
    env BITSTREAM=a.aocx CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA=1 INTEL_FPGA_OCL_PLATFORM_NAME="$EMULATOR_PLATFORM" ./b.out
    ```
### 3. Synthesize and run for performance with a large systolic array and inputs:
- Just to be safe, remove previously generated bitstream and intermediate files, if any.
  
    ```
    rm -rf a.aocx a/
    ```
    
- Re-compile the source code with large size:
    ```
    g++ gemm.cpp -g -I ../util -I $T2S_PATH/Halide/include -L $T2S_PATH/Halide/bin $HW_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11
    ```
    
- Generate a device bitstream, and a C interface for the host to invoke the matrix multiply kernel in the bitstream:

    ```
    env BITSTREAM=a.aocx AOC_OPTION="-v -profile -fpc -fp-relaxed -board=$FPGA_BOARD" ./a.out
    ```

    Look for the static analysis report in `a/reports/report.html`. For the options of the `aoc` command, refer to the [Best Practice Guide](https://www.intel.com/content/dam/www/programmable/us/en/pdfs/literature/hb/opencl-sdk/aocl-best-practices-guide.pdf).

    **DevCloud**: This step might take 4-6 hrs. To avoid the above command from being killed automatically by the system, from the headnode `login-2`, submit a batch request. For example,  write a file:

    ```
    # file batch.sh
    source /data/intel_fpga/devcloudLoginToolSetup.sh
    tools_setup -t A10DS
    env BITSTREAM=a.aocx AOC_OPTION="-v -profile -fpc -fp-relaxed -board=$FPGA_BOARD" ./a.out
    ```

    Then from the headnode `login-2`, submit a batch request like this:
    ```
    devcloud_login -b A10PAC 1.2.1 walltime=08:00:00 batch.sh
    ```

    See more details for [how to submit a batch job](https://github.com/intel/FPGA-Devcloud/tree/master/main/Devcloud_Access_Instructions#54-submitting-batch-jobs).

    **DevCloud A10PAC 1.2.1 only**: further convert the signed bitstream to unsigned:

    ```
    # Type `y` when prompted
    source $AOCL_BOARD_PACKAGE_ROOT/linux64/libexec/sign_aocx.sh -H openssl_manager -i a.aocx -r NULL -k NULL -o a_unsigned.aocx 
    mv a_unsigned.aocx a.aocx
    ```

    For more details, see [A10PAC tutorial](https://github.com/intel/FPGA-Devcloud/tree/master/main/QuickStartGuides/OpenCL_Program_PAC_Quickstart/Arria%2010).

- Compile the host file (`gemm_run.cpp`) and link with the C interface (`gemm-interface.cpp`):
    ```
    g++ gemm_run.cpp gemm-interface.cpp ../../../src/AOT-OpenCL-Runtime.cpp -g -DLINUX -DALTERA_CL -fPIC -I../../../src/ -I $T2S_PATH/Halide/include -I$INTELFPGAOCLSDKROOT/examples_aoc/common/inc $INTELFPGAOCLSDKROOT/examples_aoc/common/src/AOCLUtils/opencl.cpp $INTELFPGAOCLSDKROOT/examples_aoc/common/src/AOCLUtils/options.cpp -I$INTELFPGAOCLSDKROOT/host/include -L$INTELFPGAOCLSDKROOT/linux64/lib -L$AOCL_BOARD_PACKAGE_ROOT/linux64/lib -L$INTELFPGAOCLSDKROOT/host/linux64/lib -lOpenCL -L $T2S_PATH/Halide/bin -lelf $EMULATOR_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -o ./b.out
    ```
    
- Run the host binary. The host offloads the bitstream to an FPGA and invokes the matrix multiply kernel there through the interface:
    ```
    env BITSTREAM=a.aocx INTEL_FPGA_OCL_PLATFORM_NAME="$HW_RUN_PLATFORM_NAME" AOC_OPTION="-board=$FPGA_BOARD" ./b.out
    ```