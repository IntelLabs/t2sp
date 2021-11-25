# Matrix Multiply

## Performance for single precision matrix multiply
| Device | Frequency | Throughput | Logic utilization | DSPs | BRAMs | Efficiency | Matrix Size | Device compiler |
| ------ | --------- | ------ | --------- | ---- | ----- | -------------- | ----- | -------------- |
| Intel Arria 10 GX 1150 FPGA | 215 MHz | 532 GFLOPS | 211,199 / 427,200 ( 49 % ) | 1,304 / 1,518 ( 86 % ) | 2,087 / 2,713 ( 77 % ) | 95% DSP efficiency | 10K * 16K matrix times 16K * 8K matrix | aoc 19.4.0 |
| Intel GEN9.5 GPU | 1200 MHz | 423 GFLOPS | - | - | - | 92% machine peak | 2K * 1K matrix times 1K * 2K matrix | CM Dev Package 20200119 |

Note:

- The DSP efficiency of an FPGA equals measured throughput/theoretical peak throughput.
  - Measured throughput in GFLOPS = #operations / execution time in nanoseconds.
  - #operations =  2 (add + mul) * (#rows of matrix A) * (#columns of matrix A) * (#columns of matrix B).
  - Theoretical peak throughput = frequency * 2 (add + mul)  * 1518 (#DSPs).
- The machine peak of GEN9.5 for single-precision computes is calculated as 1200Mhz (peak frequency) * 2 (add + mul) * 2 (FPUs) * 4 (SIMD4) * 24 (EUs) = 460.8 GFlOPS.  Refer to [GEN architecture document](https://www.intel.com/content/dam/develop/external/us/en/documents/the-compute-architecture-of-intel-processor-graphics-gen9-v1d0.pdf) for more details.

## Design

Consider matrix multiply:

![Matrix multiply](figures/gemm-equation.png)   (1)

The following diagram shows the design:

![Design](figures/gemm-design.png)

In this design, the original 3 loops are manually tiled and ordered; `III`, `JJJ` and `KKK` are static constants and are the extents of loop `iii`, `jjj`, and `kkk`, respectively. 

On a GPU, some loops are made block loops,  and some loops are made thread loops, to create parallel threads. On an FPGA, there  is only one thread, since multi-threading is not efficient on FPGAs.  To drain results out of the device, we also add extra drain loops for each thread. Any loop that is not specially annotated is a sequential loop.

Data move as the loops run. For matrix `A` and `B` ,  we create abstract memories for them, `DA` and `DB`, that are resident in device DRAM, and serve as global user-managed caches. Above them, we create another level of abstract memories, `SA` and `SB`, that are resident in device SRAM, and serve as per-block user-managed caches.

Each thread contains a systolic array as its compute engine. This systolic array is created with UREs and a space-time transform. After the systolic array finishes execution, its results are drained through two levels of abstract memories, `RC2` and `RC1`,  that are resident in registers, and into the last abstract memory, `DC`, which is on the device's DRAM.

Note that an abstract memory outputs a tensor each time. For example, `DA` outputs a vector of size `KKK` each time. So the abstract memory is named streaming tensor (stensor).   

## Test the design

This design is specified to compile ahead-of-time (AOT), since AOT mode makes sense for the time-consuming FPGA compilation, although we can slightly change the specification to make it work just-in-time (JIT) as well.

### 1. [FPGA] Run emulation for verifying correctness with a tiny systolic array and inputs:

- [DevCloud]  Logging into a compute node with an A10 card and 1.2.1 software stack. See instruction [here](../../../../README.md#open-an-interactive-terminal).
  
- Set up the environment in the T2SP directory, if not yet:
  
    ```
    source ../../../../setenv.sh (local | devcloud) fpga
    ```
    
- Just to be safe, remove previously generated bitstream and intermediate files, if any:
  
    ```
    rm -rf a.* a/ gemm-interface.* *.out exec_time.txt
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
    
- Compile the host file (`gemm-run.cpp`) and link with the C interface (`gemm-interface.cpp`):
  
    ```
    g++ gemm-run.cpp gemm-interface.cpp ../../../src/AOT-OpenCL-Runtime.cpp ../../../src/SharedUtilsInC.cpp -g -DLINUX -DALTERA_CL -fPIC -I../../../src/ -I $T2S_PATH/Halide/include -I$INTELFPGAOCLSDKROOT/examples_aoc/common/inc $INTELFPGAOCLSDKROOT/examples_aoc/common/src/AOCLUtils/opencl.cpp $INTELFPGAOCLSDKROOT/examples_aoc/common/src/AOCLUtils/options.cpp -I$INTELFPGAOCLSDKROOT/host/include -L$INTELFPGAOCLSDKROOT/linux64/lib -L$AOCL_BOARD_PACKAGE_ROOT/linux64/lib -L$INTELFPGAOCLSDKROOT/host/linux64/lib -lOpenCL -L $T2S_PATH/Halide/bin -lelf $EMULATOR_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -DTINY -o ./b.out
    ```
    
- Emulate:
    ```
    env BITSTREAM=a.aocx CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA=1 INTEL_FPGA_OCL_PLATFORM_NAME="$EMULATOR_PLATFORM" ./b.out
    ```
### 2. [FPGA] Synthesize and run for performance with a large systolic array and inputs:

- [DevCloud]  Logging into a compute node with an A10 card and 1.2.1 software stack. See instruction [here](../../../../README.md#open-an-interactive-terminal).
  
- Set up the environment in the T2SP directory, if not yet:
  
    ```
    source ../../../../setenv.sh (local | devcloud) fpga
    ```
    
- Just to be safe, remove previously generated bitstream and intermediate files, if any.
  
    ```
    rm -rf a.* a/ gemm-interface.* *.out exec_time.txt
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

    **DevCloud**: This step might take 4-6 hrs. To avoid the above command from being killed automatically by the system, exit your current compute node, and from the headnode `login-2`, submit a batch request. For example,  write a file at your home directory:

    ```
    # file batch.sh
    cd path/to/t2sp
    source setenv.sh devcloud
    cd t2s/tests/performance/gemm
    env BITSTREAM=a.aocx AOC_OPTION="-v -profile -fpc -fp-relaxed -board=$FPGA_BOARD" ./a.out
    ```

    Then submit a batch request like this:
    ```
    devcloud_login -b A10PAC 1.2.1 walltime=08:00:00 batch.sh
    ```

    See more details for [how to submit a batch job](https://github.com/intel/FPGA-Devcloud/tree/master/main/Devcloud_Access_Instructions#54-submitting-batch-jobs).

    After the batch job is done, log into a compute node again, and re-source `setenv.sh` (Step 1).   

    **DevCloud A10PAC 1.2.1 only**: further convert the signed bitstream to unsigned:

    ```
    # Type `y` when prompted
    source $AOCL_BOARD_PACKAGE_ROOT/linux64/libexec/sign_aocx.sh -H openssl_manager -i a.aocx -r NULL -k NULL -o a_unsigned.aocx 
    mv a_unsigned.aocx a.aocx
    ```

    For more details, see a [pac_a10 tutorial](https://github.com/intel/FPGA-Devcloud/tree/master/main/QuickStartGuides/OpenCL_Program_PAC_Quickstart/Arria%2010).

- Compile the host file (`gemm-run.cpp`) and link with the C interface (`gemm-interface.cpp`):
    ```
    g++ gemm-run.cpp gemm-interface.cpp ../../../src/AOT-OpenCL-Runtime.cpp ../../../src/Roofline.cpp ../../../src/SharedUtilsInC.cpp  -g -DLINUX -DALTERA_CL -fPIC -I../../../src/ -I $T2S_PATH/Halide/include -I$INTELFPGAOCLSDKROOT/examples_aoc/common/inc $INTELFPGAOCLSDKROOT/examples_aoc/common/src/AOCLUtils/opencl.cpp $INTELFPGAOCLSDKROOT/examples_aoc/common/src/AOCLUtils/options.cpp -I$INTELFPGAOCLSDKROOT/host/include -L$INTELFPGAOCLSDKROOT/linux64/lib -L$AOCL_BOARD_PACKAGE_ROOT/linux64/lib -L$INTELFPGAOCLSDKROOT/host/linux64/lib -lOpenCL -L $T2S_PATH/Halide/bin -lelf $HW_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -o ./b.out
    ```

- Offload the bitstream to the device.
    ```
    aocl program acl0 a.aocx  
    ```
    
- Run the host binary. The host offloads the bitstream to an FPGA and invokes the matrix multiply kernel there through the interface:
    ```
    env BITSTREAM=a.aocx INTEL_FPGA_OCL_PLATFORM_NAME="$HW_PLATFORM" AOC_OPTION="-board=$FPGA_BOARD" ./b.out
    ```
    
- Look at the results. With the execution time collected, a roofline model of performance has been automatically generated in a file `roofline.png`.

### 3. [GPU] Run on a GPU
- [DevCloud]  Logging into a compute node with a GPU. See instruction [here](../../../../README.md#open-an-interactive-terminal).

- Set up the environment in the T2SP directory, if not yet:

    ```
    source ../../../../setenv.sh (local | devcloud) gpu
    ```

- Set up your target architecture:

    ```
    export GPU_ARCH=GEN9 # This is for both GEN9 and GEN9.5 GPU
    ```
    or
    ```
    export GPU_ARCH=GEN12
    ```

- Just to be safe, remove previously generated intermediate files, if any.
  
    ```
    rm -rf a.* *.o *.isa GEMM_genx.cpp
    ```
    
- Generate a device kernel:

    ```
    g++ gemm.cpp -g -I ../util -I $T2S_PATH/Halide/include -L $T2S_PATH/Halide/bin $HW_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -DGPU
    ./a.out
    cmc GEMM_genx.cpp -march=$GPU_ARCH -isystem ../../compiler/include_llvm -o GEMM_genx.isa
    ```
    The specification is compiled in the first command and is run in the second command, which generates a device kernel file named `GEMM_genx.cpp`, which is then compiled into a binary `GEMM_genx.isa`.

- Link the host and kernel code and run:
  ```
  g++ gemm-run-gpu.cpp -w -g -I$CM_ROOT/runtime/include -I$CM_ROOT/examples -I$CM_ROOT/drivers/media_driver/release/extract/usr/include -msse4.1 -D__LINUX__ -DLINUX -O0 -std=gnu++11 -fPIC -c -DCM_$GPU_ARCH -rdynamic -ffloat-store -o gemm-run-gpu.o
  g++ gemm-run-gpu.o -L$CM_ROOT/drivers/media_driver/release/extract/usr/lib/x86_64-linux-gnu -L$CM_ROOT/drivers/IGC/extract/usr/local/lib -L$CM_ROOT/drivers/media_driver/release/extract/usr/lib/x86_64-linux-gnu/dri $CM_ROOT/runtime/lib/x64/libigfxcmrt.so -lva -ldl -fPIC -rdynamic -o gemm-run-gpu.out
  ./gemm-run-gpu.out
  ```
  
  Performance is printed in the output.