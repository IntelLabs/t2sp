# T2S OneAPI Preprocessor

The T2S Preprocessor is a clang lib-tool to create OneAPI generated code using source-to-source code translation.

## Overview

The preprocessor is an executable source-to-source translation tool built using Clang.
All source code can be found in the `t2s/preprocessor/src/` directory.

The preprocessor generates two files given a file e.g. `filename.cpp`:
- (1) `post.t2s.<filename>.cpp`: to create a OneAPI Code Generator & generate a header file to be used by the file 2
- (2) `post.run.<filename>.cpp`: to generate a final executable 

The preprocessor uses the following `#pragma`s & `.compile_to_oneapi()` method to identify:
- (1) Function Arguments
- (2) T2S Specifications
- (3) Header file name created by the OneAPI Code Generater through the `.compile_to_oneapi()` method

Below is a boiled down example of the file structure the preprocessor is expecting.

```C++
/* filename.cpp */
int main(){
	// Initialize data 
	const int TOTAL_I = III * II * I;
	const int TOTAL_J = JJJ * JJ * J;
	const int TOTAL_K = KKK * KK * K;
	float *a = new float[TOTAL_K * TOTAL_I];
	float *b = new float[TOTAL_J * TOTAL_K];
	float *c = new float[TOTAL_J * TOTAL_I];
	for(unsigned int i = 0; i < (TOTAL_K * TOTAL_I); i++){ a[i] = random(); }
	for(unsigned int i = 0; i < (TOTAL_J * TOTAL_K); i++){ b[i] = random(); }
	for(unsigned int i = 0; i < (TOTAL_J * TOTAL_I); i++){ c[i] = 0.0f; }

	// Dimensions of the data
	int a_dim[2] = {TOTAL_K, TOTAL_I};
	int b_dim[2] = {TOTAL_J, TOTAL_K};
	int c_dim[6] = {JJJ, III, JJ, II, J, I};


#pragma t2s_spec_start
	// ...
	ImageParam A("A", TTYPE, 2), B("B", TTYPE, 2);
	// T2S Speficiations ... 
	C.compile_to_oneapi( { A, B }, "gemm", IntelFPGA);

#pragma t2s_spec_end

#pragma t2s_submit gemm (A, a, a_dim) (B, b, b_dim) (C, c, c_dim)

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

## Compile the Preprocessor. Should now have an executable named `t2spreprocessor`
make 
# clang++ -std=c++17 -fno-exceptions -fno-rtti -O3 -Os t2spreprocessor.cpp `/home/u128916/tutorial/T2S/t2sp_OneAPI_private/install/bin/llvm-config --cxxflags --ldflags` -I /home/u128916/tutorial/T2S/t2sp_OneAPI_private/downloads/llvm9.0/tools/clang/include/ -I /home/u128916/tutorial/T2S/t2sp_OneAPI_private/install  -I /home/u128916/tutorial/T2S/t2sp_OneAPI_private/install/lib/clang/9.0.1/include/ -Wl,--start-group -lclang -lclangIndex -lclangCodeGen -lclangToolingCore -lclangFrontend -lclangRewriteFrontend -lclangSema -lclangSerialization -lclangParse -lclangASTMatchers -lclangAST -lclangARCMigrate -lclangAnalysis -lclangEdit -lclangRewrite -lclangFrontendTool -lclangDriver -lclangDynamicASTMatchers -lclangFormat -lclangStaticAnalyzerCore -lclangTooling -lclangStaticAnalyzerCheckers -lclangStaticAnalyzerFrontend -lclangBasic -lclangLex -lLLVMSupport -Wl,--end-group `/home/u128916/tutorial/T2S/t2sp_OneAPI_private/install/bin/llvm-config --libs --system-libs` -o t2spreprocessor -I /home/u128916/tutorial/T2S/t2sp_OneAPI_private/install/gcc-7.5.0/lib/gcc/x86_64-pc-linux-gnu/7.5.0/include/ -I /home/u128916/tutorial/T2S/t2sp_OneAPI_private/install/lib/clang/9.0.1/include/ 
$ ls -l
# Makefile
# t2spreprocessor
# t2spreprocessor.cpp
# t2spreprocessor.h
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
#t2spreprocessor: started! ...
#USAGE: t2spreprocessor [options] <source0> [... <sourceN>]
#
#OPTIONS:
#
#Generic Options:
#
#  --help                      - Display available options (--help-hidden for more)
#  --help-list                 - Display list of available options (--help-list-hidden for more)
#  --version                   - Display the version of this program
#
#t2spreprocessor options:
#
#  --extra-arg=<string>        - Additional argument to append to the compiler command line
#  --extra-arg-before=<string> - Additional argument to prepend to the compiler command line
#  -p=<string>                 - Build path
#
#-p <build-path> is used to read a compile command database.
#
#        For example, it can be a CMake build directory in which a file named
#        compile_commands.json exists (use -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
#        CMake option to get this output). When no build path is specified,
#        a search for compile_commands.json will be attempted through all
#        parent paths of the first input file . See:
#        https://clang.llvm.org/docs/HowToSetupToolingForLLVM.html for an
#        example of setting up Clang Tooling on a source tree.
#
#<source0> ... specify the paths of source files. These paths are
#        looked up in the compile command database. If the path of a file is
#        absolute, it needs to point into CMake's source tree. If the path is
#        relative, the current working directory needs to be in the CMake
#        source tree and the file must be in a subdirectory of the current
#        working directory. "./" prefixes in the relative files will be
#        automatically removed, but the rest of a relative path must be a
#        suffix of a path in the compile command database.
#
#t2spreprocessor is inteded to convert mixed t2s specification and execution code into compiliable code
#t2spreprocessor: Expected usage goes as follows: 
#
#
#/* filename.cpp */
#int main(){
#        // Initialize data 
#        const int TOTAL_I = III * II * I;
#        const int TOTAL_J = JJJ * JJ * J;
#        const int TOTAL_K = KKK * KK * K;
#        float *a = new float[TOTAL_K * TOTAL_I];
#        float *b = new float[TOTAL_J * TOTAL_K];
#        float *c = new float[TOTAL_J * TOTAL_I];
#        for(unsigned int i = 0; i < (TOTAL_K * TOTAL_I); i++){ a[i] = random(); }
#        for(unsigned int i = 0; i < (TOTAL_J * TOTAL_K); i++){ b[i] = random(); }
#        for(unsigned int i = 0; i < (TOTAL_J * TOTAL_I); i++){ c[i] = 0.0f; }
#
#        // Dimensions of the data
#        int a_dim[2] = {TOTAL_K, TOTAL_I};
#        int b_dim[2] = {TOTAL_J, TOTAL_K};
#        int c_dim[6] = {JJJ, III, JJ, II, J, I};
#
#
##pragma t2s_spec_start
#        // ...
#        ImageParam A("A", TTYPE, 2), B("B", TTYPE, 2);
#        // T2S Speficiations ... 
#        C.compile_to_oneapi( { A, B }, "gemm", IntelFPGA);
#
##pragma t2s_spec_end
#
##pragma t2s_submit gemm (A, a, a_dim) (B, b, b_dim) (C, c, c_dim)
#
#}
```

