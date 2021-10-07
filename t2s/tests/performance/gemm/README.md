# Matrix Multiply

## Performance
| Device | Frequency | GFlops | LUTs/ALMs | DSPs | BRAMs | DSP Efficiency |
| ------ | --------- | ------ | --------- | ---- | ----- | -------------- |
| Intel Arria 10 GX FPGA | 201 Mhz | 504 GFlops | 49% | 85% | 53% | 97%   |

## Key Implementation Details

This design contains several important optimizations:
- Generate a 10x8 systolic array with space-time transformation
- Generate double buffers for input data
- Generate multiple banks connected as a daisy chain
- Vectorize input data to utilize memory bandwidth
- Automatically identify inner-product pattern and rewrite it
- Generate two-level output buffers on register
- Promote channels outside the unrolled loops to save channels

## Building the design
### JIT Mode
1. Set up the environments in the T2SP directory:
```
source setenv.sh (local | devcloud a10)
```
2. Compile the source code in this directory:
```
g++ gemm.cpp -g -I ../util -I $T2S_PATH/Halide/include -L $T2S_PATH/Halide/bin -lHalide -lz -lpthread -ldl -std=c++11
```
3. Run the emulation:
```
env CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA=1 BITSTREAM=a.aocx AOC_OPTION='-march=emulator -board=a10gx -emulator-channel-depth-model=strict' ./a.out
```
4. Run the static analysis and synthesis (Please refer to the Best Practice Guide)
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
5. Run the emulation:
```
env CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA=1 BITSTREAM=a.aocx ./a.out
```
