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

<!-- 
## ToDo's

- Construct something for the same for IntelGPU target
- Eventually, will want to match the Halide `compile_to_host()` function with output file naming 
- No runtime is built into the compiler for DPC++ currently so we just output a header file
- Verify that accessors/buffers/pipe read & writes all work properly with basic Halide Implementaitons
 -->

## Code Structure

Below you can fllow the structure of how the `Func/Stensor` object code run 
`compile_to_oneapi()` function. Begining with the class function and what nested
function it calls.

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

Major conversions include the following below.

Note, these are examples but there may be multiple implementaitons of the same concept

Within the `class CodeGen_OneAPI_Dev` inside of `./t2s/src/CodeGen_OneAPI_Dev.h/.cpp`

```c++
// Added necessary OneAPI headers
// Note that "dpc_common.hpp" comes from dpcpp installation and 
// "pipe_array.hpp" comes from https://github.com/oneapi-src/oneAPI-samples
+    src_stream_oneapi << "#include <CL/sycl.hpp>\n";
+    src_stream_oneapi << "#include <sycl/ext/intel/fpga_extensions.hpp>\n";
+    src_stream_oneapi << "#include \"dpc_common.hpp\"\n";
+    src_stream_oneapi << "#include \"pipe_array.hpp\"\n";

// Changed "#define float_from_bits(x) as_float(x)" to "#define float_from_bits(x) cl_float(x)"
+    src_stream_oneapi << "// #define float_from_bits(x) as_float(x)\n"
+               << "#define float_from_bits(x) cl_float(x)\n"

// Changed "__fpga_reg()" to "sycl::ext::intel::fpga_reg()" instead of 
// When using sycl::ext::intel::fpga_reg(), must check if it s a complex type such as float<N>
rhs << "sycl::ext::intel::fpga_reg( sycl::ext::intel::fpga_reg(" << print_expr(op->args[0]) << "))"; 

// Using pipes instead of channels
// PipeArrays are used for when Pipes have more than a depth dimension
if( pipeBounds != ""){
    // Using pipe arrays 
    oss << "using " << parent->print_name(op->name) << " = PipeArray<class " << parent->print_name(op->name) << "Pipe, " << type << ", " << pipeAttributes << ", " << pipeBounds << ">;\n";
} else {
    // Using regular pipes 
    oss << "using " << parent->print_name(op->name) << " = sycl::ext::intel::pipe<class " << parent->print_name(op->name) << "Pipe, " << type << ", " << pipeAttributes << ">;\n";
}
// Writting to the pipe simply involves passing data into the pipe's ::write() function
rhsPipe << print_name(channel_name) << "::write( " << write_data << " );\n";
rhsPipe << print_name(channel_name) << "::PipeAt<" << pipeIndex << ">" << "::write( " << write_data << " );\n";
// Reading from the pipe simply uses the ::read() function
read_pipe_call = print_name(channel_name) + "::read()";
read_pipe_call = print_name(channel_name) + "::PipeAt<" +  string_pipe_index + ">::read()";

// Each device function implementation is wrapped around in a q_device.submit and h.submit_task()
stream << get_indent() << "q_device.submit([&](sycl::handler &h){\n";
stream << get_indent() << "h.single_task<class " + name + ">([=]()\n";

// Change address spaces to accessors to use the user defined dpcpp buffers
__address_space__unloader float *restrict _unloader
stream << get_indent() << "sycl::accessor " << print_name(args[i].name) << "( " << get_memory_space(args[i].name) << ", h, sycl::read_write);\n";
```




