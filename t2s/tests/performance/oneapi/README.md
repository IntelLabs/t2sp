# OneAPI Performance Tests

## Performance

Below are the Single precision performance results using the Intel Arria 10 GX 1150 FPGA using a 8x8 Systolic arrya

| Benchmark | Logical Units              | DSP blocks             | RAM Blocks             | FMax (MHz) | GFLOPS |
| --------- | -------------------------- | ---------------------- | ---------------------- | ---------- | ------ |
| GEMM      | 186,958 / 427,200 ( 44 % ) | 1,045 / 1,518 ( 69 % ) | 1,758 / 2,713 ( 65 % ) | 159        | 302    |
| CONV      | 187,501 / 427,200 ( 44 % ) | 1,025 / 1,518 ( 68 % ) | 1,502 / 2,713 ( 55 % ) | 141        | 285    |
| CAPSULE   | 185,964 / 427,200 ( 44 % ) | 1,040 / 1,518 ( 69 % ) | 1,273 / 2,713 ( 47 % ) | 123        | 232    |

## Parameters

**NOTE:** We are currently having issues building a 8x10 systolic array for full FPGA Synthesis.

You can use the following compile parameters for each of the build

- **DTINY:** Using a 4x4 systolic array
- **DFPGA_EMULATOR:** Using FPGA Emulator 
- **DFPGA:** Using Real FPGA for syntesis

If you wish to manually adjust the size of the systolic array to a full 8x10, 
edit any of the `oneapi-target-#-parameters.h` file lines containing the following 

```c++
// GEMM (JJJ * III) (Originally 8 * 10) 
#else // FPGA
    #ifdef TINY // For verifying correctness only
        #define KKK         4
        #define JJJ         4
        #define III         4
        #define JJ          4
        #define II          4
        #define KK          4
    #else
        #define KKK         16 
        #define JJJ         8       // space loop dim
        #define III         10      // space loop dim
        #define JJ          32    
        #define II          32
        #define KK          32
    #endif
#endif

// CONV (YYY * COOO) (Originally 10 * 8)
#else // FPGA
    #ifdef TINY // For verifying correctness only
        #define CII         4
        #define CI          4
        #define COOO        4
        #define COO         4
        #define CO          4
        #define YYY         4
        #define XXX         4
        #define YY          1
        #define XX          1
        #define Y           1
        #define X           1
    #else
        #define CII         16
        #define CI          16
        #define COOO        8   // space loop dim
        #define COO         32
        #define CO          1
        #define YYY         10  // space loop dim
        #define XXX         10
        #define YY          2
        #define XX          2
        #define X           3
        #define Y           3
    #endif
#endif

// CAPSULE (COOO * YYY_XXX * YY_XX) (Originally 8 * 10 * 1)
#else // FPGA
    #ifdef TINY // For verifying correctness only
        #define CII         4
        #define CI          4
        #define COOO        4
        #define COO         4
        #define CO          1
        #define YYY_XXX     7
        #define YY_XX       1
        #define Y_X         7
    #else
        #define CII         16
        #define CI          2
        #define COOO        8       // space loop dim
        #define COO         4
        #define CO          1
        #define YYY_XXX     10      // space loop dim
        #define YY_XX       1       // space loop dim
        #define Y_X         5
    #endif
#endif

```



## Setup

Make sure you have the `dpcpp` compiler available. If you are using the Intel Devcloud A10 with DPCPP preset, you can skip this step.
This can be done on the Intel Devcloud A10 OpenCL 1.2.1 node using the following command:
```bash
## The location of the local DPCPP on A10 w/ OpenCL 1.2.1
## This step is not necessary if you use an A10 Node with the DPCPP setup
## Note, this script and directory is subject to change
source /glob/development-tools/versions/oneapi/2022.1.2/oneapi/setvars.sh

# Using a premade configuration file
# (NOTE) The current configuraiton file requires modification
source /glob/development-tools/versions/oneapi/2022.1.2/oneapi/setvars.sh --config="${T2S_PATH}/oneapi_config.txt" 
```


## Build & Run

If you wish to only run a speicific test (i.e. Gemm, Conv, Capsule), simply comment each of the tests on the following lines 
within the `src/test.sh` script

```bash
EMU=( # For Emulation
    "oneapi-target-4-gemm.cpp" "oneapi-target-4-run.cpp" "test.fpga_emu" "$EMU_DEFINES"
    "oneapi-target-5-conv.cpp" "oneapi-target-5-run.cpp" "test.fpga_emu" "$EMU_DEFINES"
    "oneapi-target-6-capsule.cpp" "oneapi-target-6-run.cpp" "test.fpga_emu" "$EMU_DEFINES"
)

FPGA=( # For Full FPGA Synthesis 
    "oneapi-target-4-gemm.cpp" "oneapi-target-4-run.cpp" "test.fpga" "$FPGA_DEFINES"
    "oneapi-target-5-conv.cpp" "oneapi-target-5-run.cpp" "test.fpga" "$FPGA_DEFINES"
    "oneapi-target-6-capsule.cpp" "oneapi-target-6-run.cpp" "test.fpga" "$FPGA_DEFINES"
)
```


Each of the build commands are shown below for foll FPGA synthesis.

```bash
## GEMM ##
# Case: oneapi-target-4-gemm.cpp oneapi-target-4-run.cpp test.fpga -DFPGA
# BOARD: pac_a10
# COMPILE T2S COMMAND:
        g++ oneapi-target-4-gemm.cpp -I /home/u128916/tutorial/T2S/t2sp_OneAPI_private/t2s/src/ -I /home/u128916/tutorial/T2S/t2sp_OneAPI_private/t2s/tests/correctness/util  -I /home/u128916/tutorial/T2S/t2sp_OneAPI_private/Halide/include -L /home/u128916/tutorial/T2S/t2sp_OneAPI_private/Halide/bin -lz -lpthread -ldl -std=c++11 -lHalide -DFPGA 
# EXECUTE CODE GENERATOR:
        ./a.out
# COMPILE ONEAPI COMMAND: 
        dpcpp oneapi-target-4-run.cpp -lz -lpthread -ldl -fintelfpga -fsycl -fsycl-device-code-split=off -DFPGA  -Xshardware -Xsboard=intel_a10gx_pac:pac_a10 -o ./test.fpga -reuse-exe=./test.fpga
# RUN EXECUTABLE
        ./test.fpga


## CONVOLUTION ##
# Case: oneapi-target-5-conv.cpp oneapi-target-5-run.cpp test.fpga -DFPGA
# BOARD: pac_a10
# COMPILE T2S COMMAND:
        g++ oneapi-target-5-conv.cpp -I /home/u128916/tutorial/T2S/t2sp_OneAPI_private/t2s/src/ -I /home/u128916/tutorial/T2S/t2sp_OneAPI_private/t2s/tests/correctness/util  -I /home/u128916/tutorial/T2S/t2sp_OneAPI_private/Halide/include -L /home/u128916/tutorial/T2S/t2sp_OneAPI_private/Halide/bin -lz -lpthread -ldl -std=c++11 -lHalide -DFPGA 
# EXECUTE CODE GENERATOR:
        ./a.out
# COMPILE ONEAPI COMMAND: 
        dpcpp oneapi-target-5-run.cpp -lz -lpthread -ldl -fintelfpga -fsycl -fsycl-device-code-split=off -DFPGA  -Xshardware -Xsboard=intel_a10gx_pac:pac_a10 -o ./test.fpga -reuse-exe=./test.fpga
# RUN EXECUTABLE
        ./test.fpga


## CAPSULE ##
# Case: oneapi-target-6-capsule.cpp oneapi-target-6-run.cpp test.fpga -DFPGA
# BOARD: pac_a10
# COMPILE T2S COMMAND:
        g++ oneapi-target-6-capsule.cpp -I /home/u128916/tutorial/T2S/t2sp_OneAPI_private/t2s/src/ -I /home/u128916/tutorial/T2S/t2sp_OneAPI_private/t2s/tests/correctness/util  -I /home/u128916/tutorial/T2S/t2sp_OneAPI_private/Halide/include -L /home/u128916/tutorial/T2S/t2sp_OneAPI_private/Halide/bin -lz -lpthread -ldl -std=c++11 -lHalide -DFPGA 
# EXECUTE CODE GENERATOR:
        ./a.out
# COMPILE ONEAPI COMMAND: 
        dpcpp oneapi-target-6-run.cpp -lz -lpthread -ldl -fintelfpga -fsycl -fsycl-device-code-split=off -DFPGA  -Xshardware -Xsboard=intel_a10gx_pac:pac_a10 -o ./test.fpga -reuse-exe=./test.fpga
# RUN EXECUTABLE
        ./test.fpga
```

