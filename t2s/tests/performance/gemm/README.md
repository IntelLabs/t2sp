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
g++ gemm.cpp -g -I ../util -I $T2S_PATH/Halide/include -L $T2S_PATH/Halide/bin -lHalide -lz -lpthread -ldl -std=c++11
```
3. Run emulation for verifying correctness:
```
env CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA=1 BITSTREAM=a.aocx AOC_OPTION='-march=emulator -board=a10gx -emulator-channel-depth-model=strict' ./a.out
```
4. Run static analysis and synthesis (Please refer to the [Best Practice Guide](https://www.intel.com/content/dam/www/programmable/us/en/pdfs/literature/hb/opencl-sdk/aocl-best-practices-guide.pdf))
```
aoc -rtl -report -board=pac_a10 a.cl
aoc -profile -v -fpc -fp-relaxed -g -board=pac_a10 a.cl
```
### AOT Mode
1. Set up the environments in the T2SP directory:
```
source setenv.sh (local | devcloud a10)
```
2. Compile the source code in this directory:
```
g++ gemm.cpp -g -I ../util -I $T2S_PATH/Halide/include -L $T2S_PATH/Halide/bin -lHalide -lz -lpthread -ldl -std=c++11 -DAOT
```
3. Generate device and host files:
```
env BITSTREAM=a.aocx AOC_OPTION='-march=emulator -board=a10gx -emulator-channel-depth-model=strict' ./a.out
```
4. Compile the host files:
```
g++ gemm_run.cpp host.cpp ../../../src/AOT-OpenCL-Runtime.cpp -g -DLINUX -DALTERA_CL -fPIC -I../../../src/ -I ../../../../Halide/include -I$INTELFPGAOCLSDKROOT/examples_aoc/common/inc $INTELFPGAOCLSDKROOT/examples_aoc/common/src/AOCLUtils/opencl.cpp $INTELFPGAOCLSDKROOT/examples_aoc/common/src/AOCLUtils/options.cpp -I$INTELFPGAOCLSDKROOT/host/include $AOCL_LIBS -L ../../../../Halide/bin -lelf -lHalide -lz -lpthread -ldl -std=c++11
```
5. Run  emulation for verifying correctness:
```
env CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA=1 BITSTREAM=a.aocx ./a.out
```
