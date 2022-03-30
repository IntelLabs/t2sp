# T2S OneAPI Preprocessor

The T2S Preprocessor is a clang lib-tool to create OneAPI generated code using source-to-source code translation.

## Overview

The preprocessor is an executable source-to-source translation tool built using Clang.
All source code can be found in the `t2s/preprocessor/src/` directory.

The preprocessor generates two files given a file e.g. `filename.cpp`:
- (1) `post.t2s.<filename>.cpp`: to create a OneAPI Code Generator & generate a header file to be used by the file 2
- (2) `post.run.<filename>.cpp`: to generate a final executable 

The preprocessor uses the following `#pragma`s & `.compile_to_oneapi()` method to identify:
- (1) function argument elements
- (2) T2S Specifications
- (3) header file name created by the OneAPI Code Generater through the `.compile_to_oneapi()` method

```C++
/* filename.cpp */
int main(){
 // 
 float *a = new float[DIM_1 * DIM_2];
 float *b = new float[DIM_3 * DIM_4 * DIM_5];

 int a_dim[2] = {DIM_1, DIM_2};
 int b_dim[3] = {DIM_3, DIM_4, DIM_5};

 #pragma t2s_arg A, a, a_dim
 #pragma t2s_arg C, c, c_dim
 #pragma t2s_arg B, b, b_dim
 #pragma t2s_spec_start

  ImageParam A("A", TTYPE, 2), B("B", TTYPE, 2);
  // T2S Speficiations ... 
  C.compile_to_oneapi( { A, B }, "gemm", IntelFPGA);

 #pragma t2s_spec_end

}
```

## Build

### Preprocessor

To build the preprocessor executable use the following steps for Intel DevCloud:
```bash
## Setup

cd ~/path/to/t2sp/
# If on an Intel Devcloud Node without OneAPI
source setenv.sh 
cd t2s/preprocessor/src/

## Compile the Preprocessor. Should now have an executable named ``
make 


```

### Sample

Currently, there is one example, `~/path/to/t2sp/preprocessor/sample/sample_01/`. 
The core file used by the preprocessor is located at `~/path/to/t2sp/preprocessor/sample/sample_01/test.t2s.cpp`.

To build `sample_01`, execute the following steps assuming you have already built the preprocessor:

```bash
cd ~/path/to/t2sp/
cd ./t2s/preprocessor/sample/
# To run the preprocessor on sample_01, run the following Pre-made script
./run_01.sh 

# To compile and execute the sample_01, run the following Pre-made script:
# You can ignore warrnings generated.
# The final executable should be ./sample_01/test.fpga_emu 
./build_01.sh

# To run the executable manually, execute the following steps:
cd sample_01/
./test.fpga_emu
```

## Troubleshooting

For further help assuming you have built the preprocessor, use the following steps for more information

```bash
cd ~/path/to/t2sp/
cd t2s/preprocessor/src/
./t2spreprocessor --help
# t2spreprocessor: started! ...
# USAGE: t2spreprocessor [options] <source0> [... <sourceN>]
#
# OPTIONS:
#
# Generic Options:
#
#   --help                      - Display available options (--help-hidden for more)
#   --help-list                 - Display list of available options (--help-list-hidden for more)
#   --version                   - Display the version of this program
#
# t2spreprocessor options:
#
#   --extra-arg=<string>        - Additional argument to append to the compiler command line
#   --extra-arg-before=<string> - Additional argument to prepend to the compiler command line
#   -p=<string>                 - Build path
#
# -p <build-path> is used to read a compile command database.
#
#         For example, it can be a CMake build directory in which a file named
#         compile_commands.json exists (use -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
#         CMake option to get this output). When no build path is specified,
#         a search for compile_commands.json will be attempted through all
#         parent paths of the first input file . See:
#         https://clang.llvm.org/docs/HowToSetupToolingForLLVM.html for an
#         example of setting up Clang Tooling on a source tree.
#
# <source0> ... specify the paths of source files. These paths are
#         looked up in the compile command database. If the path of a file is
#         absolute, it needs to point into CMake's source tree. If the path is
#         relative, the current working directory needs to be in the CMake
#         source tree and the file must be in a subdirectory of the current
#         working directory. "./" prefixes in the relative files will be
#         automatically removed, but the rest of a relative path must be a
#         suffix of a path in the compile command database.
#
# t2spreprocessor is inteded to convert mixed t2s specification and execution code into compiliable code
# t2spreprocessor: Expected usage goes as follows: 
#
# int main(){
#
#  float *a = new float[DIM_1 * DIM_2];
#  float *b = new float[DIM_3 * DIM_4 * DIM_5];
#
#  int a_dim[2] = {DIM_1, DIM_2};
#  int b_dim[3] = {DIM_3, DIM_4, DIM_5};
#
#  #pragma t2s_arg A, a, a_dim
#  #pragma t2s_arg C, c, c_dim
#  #pragma t2s_arg B, b, b_dim
#  #pragma t2s_spec_start
#
#   ImageParam A("A", TTYPE, 2), B("B", TTYPE, 2);
#   // T2S Speficiations ... 
#   C.compile_to_oneapi( { A, B }, "gemm", IntelFPGA);
#
#  #pragma t2s_spec_end
#
# }
```

