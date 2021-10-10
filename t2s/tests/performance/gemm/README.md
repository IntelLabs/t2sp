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
-Identifying and rewriting the inner-product pattern
-Moving  channel accesses at the bounaries of the systolic array outside the unrolled loops

## Building the design

The design can be built in JIT (just-in-time) or AOT (ahead-of-time) mode.

### JIT Mode
1. Set up the environments in the T2SP directory:
```
source setenv.sh (local | devcloud a10)
```
2. Compile the source code in this directory:
```
g++ gemm.cpp -g -I ../util -I $T2S_PATH/Halide/include -L $T2S_PATH/Halide/bin $EMULATOR_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -DTINY
```
3. Run emulation for verifying correctness:
```
env BITSTREAM=a.aocx CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA=1 INTEL_FPGA_OCL_PLATFORM_NAME="$EMULATOR_PLATFORM" AOC_OPTION="$EMULATOR_AOC_OPTION -board=$FPGA_BOARD -emulator-channel-depth-model=strict" ./a.out
```
4. Re-compile the source code for a large scale:
```
g++ gemm.cpp -g -I ../util -I $T2S_PATH/Halide/include -L $T2S_PATH/Halide/bin $HW_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -DCOMPILE_ONLY
env BITSTREAM=a.aocx ./a.out
```
5. Run static analysis and synthesis (Please refer to the [Best Practice Guide](https://www.intel.com/content/dam/www/programmable/us/en/pdfs/literature/hb/opencl-sdk/aocl-best-practices-guide.pdf))
```
aoc -rtl -report -board="$FPGA_BOARD" a.cl
aoc -v -profile -fpc -fp-relaxed -board="$FPGA_BOARD" a.cl
```
6. Run the bitstream on an FPGA:
```
env BITSTREAM=a.aocx INTEL_FPGA_OCL_PLATFORM_NAME="$HW_RUN_PLATFORM_NAME" AOC_OPTION="-board=$FPGA_BOARD" ./a.out
```
### AOT Mode
1. Set up the environments in the T2SP directory:
```
source setenv.sh (local | devcloud a10)
```
2. Compile the source code in this directory:
```
g++ gemm.cpp -g -I ../util -I $T2S_PATH/Halide/include -L $T2S_PATH/Halide/bin $EMULATOR_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -DTINY -DAOT
```
3. Generate device and host files:
```
env BITSTREAM=a.aocx AOC_OPTION="$EMULATOR_AOC_OPTION -board=$FPGA_BOARD -emulator-channel-depth-model=strict" ./a.out
```
4. Compile the host files:
```
g++ gemm_run.cpp host.cpp ../../../src/AOT-OpenCL-Runtime.cpp -g -DLINUX -DALTERA_CL -fPIC -I../../../src/ -I $T2S_PATH/Halide/include -I$INTELFPGAOCLSDKROOT/examples_aoc/common/inc $INTELFPGAOCLSDKROOT/examples_aoc/common/src/AOCLUtils/opencl.cpp $INTELFPGAOCLSDKROOT/examples_aoc/common/src/AOCLUtils/options.cpp -I$INTELFPGAOCLSDKROOT/host/include -L$INTELFPGAOCLSDKROOT/linux64/lib -L$AOCL_BOARD_PACKAGE_ROOT/linux64/lib -L$INTELFPGAOCLSDKROOT/host/linux64/lib -lOpenCL -L $T2S_PATH/Halide/bin -lelf $EMULATOR_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -DTINY -o ./b.out
```
5. Run emulation for verifying correctness:
```
env BITSTREAM=a.aocx CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA=1 INTEL_FPGA_OCL_PLATFORM_NAME="$EMULATOR_PLATFORM" ./b.out
```
6. Re-compile the source code for a large scale:
```
g++ gemm.cpp -g -I ../util -I $T2S_PATH/Halide/include -L $T2S_PATH/Halide/bin $HW_LIBHALIDE_TO_LINK -lz -lpthread -ldl -std=c++11 -DCOMPILE_ONLY
env BITSTREAM=a.aocx ./a.out
```
7. Run static analysis and synthesis (Please refer to the [Best Practice Guide](https://www.intel.com/content/dam/www/programmable/us/en/pdfs/literature/hb/opencl-sdk/aocl-best-practices-guide.pdf))
```
aoc -rtl -report -board="$FPGA_BOARD" a.cl
aoc -v -profile -fpc -fp-relaxed -board="$FPGA_BOARD" a.cl
```
8. Run the bitstream on an FPGA:
```
env BITSTREAM=a.aocx INTEL_FPGA_OCL_PLATFORM_NAME="$HW_RUN_PLATFORM_NAME" AOC_OPTION="-board=$FPGA_BOARD" ./b.out
```