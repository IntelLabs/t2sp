# oneapi-integration

This is a correctness test directory for the T2S OneAPI Code Generator.

**NOTE:** There are some issues with full FPGA emulation for convolution and capsule tests. 
If you wish do use real FPGA, it is encouraged to include the `-DTINY` in the compiling commands as to reduce hardware size and issues.

## Overview

There are three tests within this directory:
- (1) GEMM: Defined within the files `oneapi-target-4-gemm.cpp`, `oneapi-target-4-parameters.h`, and `oneapi-target-4-run.cpp`
- (1) CONVOLUTION: Defined within the files `oneapi-target-5-conv.cpp`, `oneapi-target-5-parameters.h`, and `oneapi-target-5-run.cp`
- (1) GEMM: Defined within the files `oneapi-target-6-capsule.cpp`, `oneapi-target-6-parameters.h`, and `oneapi-target-6-run.cpp`

## RUN

To run the all of the tests, simply use the provided `test.sh` script as so:

```bash
# To only run FPGA EMULATION
./test.sh

# To only run FULL FPGA SYNTHESIZATION
./test.sh FPGA
```
