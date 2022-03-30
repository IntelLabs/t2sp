# Design for OneAPI Integration

By Hongbo Rong (hongbo.rong@intel.com),  Abenezer Wudenhe (awude001@ucr.edu)

## Overview 

This document gives a brief overview T2S integration & Code Generation with Intel OneAPI's DPCPP compiler

The OneAPI Code Generator can be found within `./t2s/src/CodeGen_OneAPI_Dev.h/.cpp` 
based off of `./Halide/src/CodeGen_OpenCL_Dev.h/.cpp` code. 
More information on how their internals work can be found in the sections below.

The main component of the OneAPI Integration is the CodeGenerator and the method `.compile_to_oneapi(...)`. 
More information on how this method works can be found in the sections below.

**NOTE:** This is documentation may not reflect the current standing or implementation of the Code Generator as it is due to change over time.

## `.compile_to_oneapi(...)` Method

The overall concept is to give `Func/Stensor` object a way to compile to OneAPI source code.
This is done by calling the `compile_to_oneapi(...)` function.

```c++
void compile_to_oneapi(
    const std::vector< Argument > &,
    const std::string & 	fn_name = "",
    const Target & 	target = get_target_from_environment() 
)	
```

This will emit a header file named `<fn_name>.generated_oneapi_header.h` 
with a defined function of the same name, `<fn_name>`.
Simply `#include <fn_name>.generated_oneapi_header.h` and compile with `dpcpp`.

- `const std::vector< Argument > &`: A set of inputs to the `Func` or `Stensor` pipeline stage
- `const std::string & 	fn_name`: name of the compiled function and file prefix
- `const Target & 	target`: Set of Halide Targets. 
  - Only `IntelFPGA` target is currently supported.
  - The OneAPI target is automatically added, even if user does not manually add it.
  - Eventually will extend to other targets.


## Code Structure

Below you can follow the structure of how the `Func/Stensor` types executes the 
`compile_to_oneapi()` method

### Intro

Below is how the legend on how this section is written.
This is just a simple organization & notes on how different Classes/Methods interact in relation to their files.
This is by no means official documentation.

```
-> file_name.cpp/.h
  - Calling method name
  - nested calling method
-> nested_calling_method_filename.cpp/.h
  - etc...
```



###  `Func/Stensor` types breakdown in relation to `compile_to_oneapi()` method

```c++
-> Stensor.cpp
  - void Stensor::compile_to_oneapi()
  - Func f.compile_to_oneapi()
-> Func.cpp
  - void Func::compile_to_oneapi()
  - pipeline().compile_to_oneapi()
-> Pipeline.cpp
  - void Pipeline::compile_to_oneapi()
  - Module m = compile_to_module(); m.compile(single_output())
-> Module.cpp
  - void Module::compile()
  - CodeGen_LLVM *ret = new CodeGen_GPU_Host<CodeGen_X86>(t); ret->compile_to_devsrc(*this);
-> CodeGen_LLVM.cpp // i.e. base class of CodeGen_GPU_Host
  - void CodeGen_LLVM::compile_to_devsrc(Module*); 
    - CodeGen_LLVM::init_codegen()
    - CodeGen_LLVM::add_external_code(Module*)
    - input.buffers(){ compile(b); } input.functions(){ compile(b); } 
  - CodeGen::compile_func(); // (NOTE) Called only once
-> CodeGen_GPU_Host.cpp
  - void CodeGen_GPU_Host<CodeGen_CPU>::compile_func()
  - std::vector<char> kernel_src_wrapped = ((CodeGen_OneAPI_Dev*)cgdev[DeviceAPI::OneAPI])->compile_to_src_module(f);
-> CodeGen_OneAPI_Dev
  - vector<char> CodeGen_OneAPI_Dev::compile_to_src_module(const LoweredFunc &f);
  - string src = one_clc.compile_oneapi_lower(f, str); return vector<char> buffer(src.begin(), src.end());
```


## Implementation 

### Intro

Intel provides documentation on how to migrate OpenCL FPGA designs to DPCPP
[here](https://www.intel.com/content/www/us/en/develop/documentation/migrate-opencl-fpga-designs-to-dpcpp/top/flags-attributes-directives-and-extensions.html).

The Code Generator is contained within the class `CodeGen_OneAPI_Dev` inside of `./t2s/src/CodeGen_OneAPI_Dev.h/.cpp`
The OneAPI Code Generator is based of the class within `CodeGen_OpenCL_Dev` class inside of `./Halide/src/CodeGen_OpenCL_Dev.h/.cpp`

Originally, the class `CodeGen_OneAPI_Dev` and the class it was based off of, `CodeGen_OpenCL_Dev`, were both intended to only generate kernel device code. Meaning, their original purpose was to translate functions into specific languages for the same CPU code to interact with through Halide specific structures, classes, and methods.

`CodeGen_OneAPI_Dev` is designed to generate both CPU and device code, and provide a simplistic function interface for the end user to use. To implement this concept, the `CodeGen_OneAPI_Dev` class contains `protected` classes based on the `CodeGen_C` halide class to do this. `CodeGen_OneAPI_Dev` simply recieves Halides IR of the program, and generates a header file with a function that wrapps both CPU and Device code, all implemented with Halide & OneAPI.


### Code Generation

The `CodeGen_OneAPI_Dev` contains `protected` classes derived from the `CodeGen_C`. `CodeGen_C` is derived from both the `IRPrinter` and `IRVisitor` classes. Simply put, the protected classes within `CodeGen_OneAPI_Dev` override methods within the `CodeGen_C` to produce OneAPI code, as the code generator traverses Halide's Intermediate Representation (IR).

For example, the `visit(const For *loop)` method, if implemented on a CPU, is simply a for loop. If implemented on a device, becomes a device kernel, requiring memory copies, and other code to support the device kernel. The Halide IR contains information on device location among other information dependent on the operation being implemented.

### Troubleshooting

T2S & Halide provide debugging ability through the use of print statements and environment variables. Assuming any modifications have allowed halide to compile fully, you are able to set the debugging level using the following:

```bash
// Remove the previous debuging enviornment variable
unset HL_DEBUG_CODEGEN

// Set the new debuging level
export HL_DEBUG_CODEGEN=2
```

Within the source code, developers are able to display debuging messages when T2S's executable is executed:

```c++
// debug takes in the debuging leven
debug(2) << "debuging message\n";
```