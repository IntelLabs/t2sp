# oneapi-integration

This is a correctness test directory for the T2S OneAPI Code Generator.

**NOTE:** There are some issues with full FPGA emulation for convolution and capsule tests. 
If you wish do use real FPGA, it is encouraged to include the `-DTINY` in the compiling commands as to reduce hardware size and issues.

**NOTE:** If you are using A10 on the Intel Devcloud FPGA, and having trouble with the dpcpp compiler, you can use the `setvars.sh` 
script to set up the compiler. There is also a `oneapi_config.txt`
```bash
## The location of the local DPCPP on A10 w/ OpenCL 1.2.1
## This step is not necessary if you use an A10 Node with the DPCPP setup
## Note, this script and directory is subject to change
source /glob/development-tools/versions/oneapi/2022.1.2/oneapi/setvars.sh

# Using a premade configuration file
# (NOTE) The current configuraiton file requires modification
source /glob/development-tools/versions/oneapi/2022.1.2/oneapi/setvars.sh --config="${T2S_PATH}/oneapi_config.txt" 
```

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
