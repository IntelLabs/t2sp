# Design for OneAPI Integration

By Hongbo Rong (hongbo.rong@intel.com),  Abenezer Wudenhe (awude001@ucr.edu)

## Overview 

This document gives a breif overview T2S integration with Intel OneAPI's DPCPP compiler


The OneAPI Code Generator can be found within `./t2s/src/CodeGen_OneAPI_Dev.h/.cpp` 
based off of `./Halide/src/CodeGen_OpenCL_Dev.h/.cpp` code.

The overal concept is to give `Func/Stensor` object a way to compile to OneAPI source code.
This is done by calling the `compile_to_oneapi(...)` function.

```c++
void compile_to_oneapi(
    const std::vector< Argument > &,
    const std::string & 	fn_name = "",
    const Target & 	target = get_target_from_environment() 
)	
```

This will emmit a header file named `<fn_name>.generated_oneapi_header.h` 
with a defined function of the same name, `<fn_name>`.
Simply `#include <fn_name>.generated_oneapi_header.h` and compile with `dpcpp`.

- `const std::vector< Argument > &`: A set of inputs to the `Func` or `Stensor` pipeline stage
- `const std::string & 	fn_name`: name of the compiled function and file prefix
- `const Target & 	target`: Set of Halide Targets. 
  - Only `IntelFPGA` target is currently supported.
  - The OneAPI target is automatically added, even if user does not manually add it.
  - Eventually will extend to other targets.


## Code Structure

Below you can fllow the structure of how the `Func/Stensor` types run 
`compile_to_oneapi()` function in the following structure:

```
-> file_name.cpp/.h
  - Calling function name
  - nested calling function
-> nested_calling_function_filename.cpp/.h
  -
and so on ...
```



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


## Conversion

Intel provides documentation on how to migrate opencl fpga designs to dpcpp
[here](https://www.intel.com/content/www/us/en/develop/documentation/migrate-opencl-fpga-designs-to-dpcpp/top/flags-attributes-directives-and-extensions.html).

The Code Generator the `class CodeGen_OneAPI_Dev` inside of `./t2s/src/CodeGen_OneAPI_Dev.h/.cpp`

The OneAPI Code Generator is bassed of the `CodeGen_OpenCL_Dev.h/.cpp` class inside of `./Halide/src/CodeGen_OpenCL_Dev.h/.cpp`



