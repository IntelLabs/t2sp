#include <fstream>
#include <sstream>

#include "../../t2s/src/CodeGen_OneAPI_Dev.h"
#include "../../t2s/src/Utilities.h"
#include "CodeGen_CM_Dev.h"
#include "CodeGen_D3D12Compute_Dev.h"
#include "CodeGen_DPC_Dev.h"
#include "CodeGen_GPU_Host.h"
#include "CodeGen_Internal.h"
#include "CodeGen_Metal_Dev.h"
#include "CodeGen_OpenCL_Dev.h"
#include "CodeGen_OpenGLCompute_Dev.h"
#include "CodeGen_OpenGL_Dev.h"
#include "CodeGen_PTX_Dev.h"
#include "Debug.h"
#include "ExprUsesVar.h"
#include "IROperator.h"
#include "IRPrinter.h"
#include "LLVM_Headers.h"
#include "Simplify.h"
#include "Util.h"
#include "VaryingAttributes.h"
namespace Halide {
namespace Internal {

using std::map;
using std::pair;
using std::string;
using std::vector;

using namespace llvm;

// Sniff the contents of a kernel to extracts the bounds of all the
// thread indices (so we know how many threads to launch), and the
// amount of shared memory to allocate.
class ExtractBounds : public IRVisitor {
public:
    Expr num_threads[4];
    Expr num_blocks[4];
    Expr shared_mem_size;

    ExtractBounds()
        : shared_mem_size(0), found_shared(false) {
        for (int i = 0; i < 4; i++) {
            num_threads[i] = num_blocks[i] = 1;
        }
    }

private:
    bool found_shared;

    using IRVisitor::visit;

    void visit(const For *op) override {
        if (CodeGen_GPU_Dev::is_gpu_var(op->name)) {
            internal_assert(is_zero(op->min));
        }

        if (ends_with(op->name, ".__thread_id_x")) {
            num_threads[0] = op->extent;
        } else if (ends_with(op->name, ".__thread_id_y")) {
            num_threads[1] = op->extent;
        } else if (ends_with(op->name, ".__thread_id_z")) {
            num_threads[2] = op->extent;
        } else if (ends_with(op->name, ".__thread_id_w")) {
            num_threads[3] = op->extent;
        } else if (ends_with(op->name, ".__block_id_x")) {
            num_blocks[0] = op->extent;
        } else if (ends_with(op->name, ".__block_id_y")) {
            num_blocks[1] = op->extent;
        } else if (ends_with(op->name, ".__block_id_z")) {
            num_blocks[2] = op->extent;
        } else if (ends_with(op->name, ".__block_id_w")) {
            num_blocks[3] = op->extent;
        }

        op->body.accept(this);
    }

    void visit(const LetStmt *op) override {
        if (expr_uses_var(shared_mem_size, op->name)) {
            shared_mem_size = Let::make(op->name, op->value, shared_mem_size);
        }
        op->body.accept(this);
    }

    void visit(const Allocate *allocate) override {
        user_assert(!allocate->new_expr.defined()) << "Allocate node inside GPU kernel has custom new expression.\n"
                                                   << "(Memoization is not supported inside GPU kernels at present.)\n";

        if (allocate->name == "__shared") {
            internal_assert(allocate->type == UInt(8) && allocate->extents.size() == 1);
            shared_mem_size = allocate->extents[0];
            found_shared = true;
        }
        allocate->body.accept(this);
    }
};

template<typename CodeGen_CPU>
CodeGen_GPU_Host<CodeGen_CPU>::CodeGen_GPU_Host(Target target)
    : CodeGen_CPU(target) {
    // For the default GPU, the order of preferences is: Metal,
    // OpenCL, CUDA, OpenGLCompute, and OpenGL last.
    // The code is in reverse order to allow later tests to override
    // earlier ones.
    if (target.has_feature(Target::OpenGL)) {
        debug(1) << "Constructing OpenGL device codegen\n";
        cgdev[DeviceAPI::GLSL] = new CodeGen_OpenGL_Dev(target);
    }
    if (target.has_feature(Target::OpenGLCompute)) {
        debug(1) << "Constructing OpenGL Compute device codegen\n";
        cgdev[DeviceAPI::OpenGLCompute] = new CodeGen_OpenGLCompute_Dev(target);
    }
    if (target.has_feature(Target::CUDA)) {
        debug(1) << "Constructing CUDA device codegen\n";
        cgdev[DeviceAPI::CUDA] = new CodeGen_PTX_Dev(target);
    }
    if (target.has_feature(Target::IntelGPU) && (!target.has_feature(Target::OneAPI))) {
        debug(1) << "Constructing CM device codegen\n";
        cgdev[DeviceAPI::CM] = new CodeGen_CM_Dev(target);
    }
    if (target.has_feature(Target::OpenCL)) {
        debug(1) << "Constructing OpenCL device codegen\n";
        cgdev[DeviceAPI::OpenCL] = new CodeGen_OpenCL_Dev(target);
    }
    if (target.has_feature(Target::OneAPI)) {
        if (target.has_feature(Target::IntelFPGA)) {
            debug(1) << "Constructing OneAPI device codegen for FPGA\n";
            cgdev[DeviceAPI::OneAPI] = new CodeGen_OneAPI_Dev(target);
        } else if (target.has_feature(Target::IntelGPU)) {
            debug(1) << "Constructing OneAPI device codegen for GPU\n";
            cgdev[DeviceAPI::OneAPI] = new CodeGen_DPC_Dev(target);
        }
    }
    if (target.has_feature(Target::Metal)) {
        debug(1) << "Constructing Metal device codegen\n";
        cgdev[DeviceAPI::Metal] = new CodeGen_Metal_Dev(target);
    }
    if (target.has_feature(Target::D3D12Compute)) {
        debug(1) << "Constructing Direct3D 12 Compute device codegen\n";
        cgdev[DeviceAPI::D3D12Compute] = new CodeGen_D3D12Compute_Dev(target);
    }

    if (cgdev.empty()) {
        internal_error << "Requested unknown GPU target: " << target.to_string() << "\n";
    }
}

template<typename CodeGen_CPU>
CodeGen_GPU_Host<CodeGen_CPU>::~CodeGen_GPU_Host() {
    for (pair<const DeviceAPI, CodeGen_GPU_Dev *> &i : cgdev) {
        delete i.second;
    }
}

template<typename CodeGen_CPU>
void CodeGen_GPU_Host<CodeGen_CPU>::compile_func(const LoweredFunc &f,
                                                 const std::string &simple_name,
                                                 const std::string &extern_name) {
    function_name = simple_name;

    // Create a new module for all of the kernels we find in this function.
    for (pair<const DeviceAPI, CodeGen_GPU_Dev *> &i : cgdev) {
        i.second->init_module();

        // FPGA only: A device kernel always starts with a set of realizes of channels.
        // These channels are the output channels of the kernel. Here we declare all the
        // compiler-generated vector and struct types in the kernel, and the channels.
        if (target.has_feature(Target::IntelFPGA) && !target.has_feature(Target::OneAPI)) {
            internal_assert(cgdev.find(DeviceAPI::OpenCL) != cgdev.end());
            // Defines Pipes/Channels and their data types
            ((CodeGen_OpenCL_Dev *)cgdev[DeviceAPI::OpenCL])->print_global_data_structures_before_kernel(&f.body);

            // Gather shift registers' allocations.
            ((CodeGen_OpenCL_Dev *)cgdev[DeviceAPI::OpenCL])->gather_shift_regs_allocates(&f.body);
        }
        if (target.has_feature(Target::IntelFPGA) && target.has_feature(Target::OneAPI)) {
            internal_assert(cgdev.find(DeviceAPI::OneAPI) != cgdev.end());
            ((CodeGen_OneAPI_Dev *)cgdev[DeviceAPI::OneAPI])->print_global_data_structures_before_kernel(&f.body);

            // Gather shift registers' allocations.
            ((CodeGen_OneAPI_Dev *)cgdev[DeviceAPI::OneAPI])->gather_shift_regs_allocates(&f.body);
        }
    }

    // Call the base implementation to create the function.
    CodeGen_CPU::compile_func(f, simple_name, extern_name);

    // We need to insert code after the existing entry block, so that
    // the destructor stack slots exist before we do the assertions
    // involved in initializing gpu kernels.

    // Split the entry block just before its end.
    BasicBlock *entry = &function->getEntryBlock();
    llvm::Instruction *terminator = entry->getTerminator();
    internal_assert(terminator);
    BasicBlock *post_entry = entry->splitBasicBlock(terminator);

    // Create some code that does the GPU initialization.
    BasicBlock *init_kernels_bb = BasicBlock::Create(*context, "init_kernels",
                                                     function, post_entry);

    // The entry block should go to the init kernels block instead of
    // the post entry block.
    entry->getTerminator()->eraseFromParent();
    builder->SetInsertPoint(entry);
    builder->CreateBr(init_kernels_bb);

    // Fill out the init kernels block
    builder->SetInsertPoint(init_kernels_bb);

    for (pair<const DeviceAPI, CodeGen_GPU_Dev *> &i : cgdev) {

        CodeGen_GPU_Dev *gpu_codegen = i.second;
        std::string api_unique_name = gpu_codegen->api_unique_name();

        // If the module state for this API/function did not get created, there were
        // no kernels using this API.
        llvm::Value *module_state = get_module_state(api_unique_name, false);
        if (!module_state) {
            continue;
        }

        debug(2) << "Generating init_kernels for " << api_unique_name << "\n";

        std::vector<char> kernel_src = gpu_codegen->compile_to_src();
        if (api_unique_name == "cm") {
            debug(1) << "Currently, we do not implement CM runtime, so we just emit source code.\n";
            std::ofstream file(simple_name + "_genx.cpp", std::fstream::out);
            std::string src(kernel_src.cbegin(), kernel_src.cend());
            if (file.is_open())
                file << src;
            file.close();
            return;
        }
        if (api_unique_name == "dpc") {
            debug(1) << "Currently, we do not implement DPC++ runtime, so we just emit source code.\n";
            std::ofstream file(simple_name + "_genx.cpp", std::fstream::out);
            std::string src(kernel_src.cbegin(), kernel_src.cend());
            if (file.is_open())
                file << src;
            file.close();
            return;
        }

        if (api_unique_name == "oneapi") {
            debug(1) << "Currently, we do not implement OneAPI runtime, so we just emit source code.\n";
            debug(1) << "Emmiting " << api_unique_name << " code\n";
            std::ofstream file(simple_name + ".generated_oneapi_header.h", std::fstream::out);
            user_warning << "OneAPI CodeGenerator is ment to be used with compile_to_oneapi() function. Directly generating source code may not lead to unknown behavior\n";

            std::string src(kernel_src.cbegin(), kernel_src.cend());
            if (file.is_open())
                file << src;
            file.close();
            return;
        }

        Value *kernel_src_ptr =
            CodeGen_CPU::create_binary_blob(kernel_src,
                                            "halide_" + function_name + "_" + api_unique_name + "_kernel_src");

        if (f.args[0].name == "__user_context") {
            // The user context is first argument of the function.
            // We retrieve it here so it's available for subsequent calls of
            // get_user_context().
            sym_push("__user_context", iterator_to_pointer(function->arg_begin()));
        }

        Value *user_context = get_user_context();
        Value *kernel_size = ConstantInt::get(i32_t, kernel_src.size());
        std::string init_kernels_name;
        if (api_unique_name == "oneapi") {
            init_kernels_name = "halide_opencl_initialize_kernels";
        } else {
            init_kernels_name = "halide_" + api_unique_name + "_initialize_kernels";
        }
        Value *init = module->getFunction(init_kernels_name);
        internal_assert(init) << "Could not find function " + init_kernels_name + " in initial module\n";
        vector<Value *> init_kernels_args = {user_context, module_state, kernel_src_ptr, kernel_size};
        Value *result = builder->CreateCall(init, init_kernels_args);
        Value *did_succeed = builder->CreateICmpEQ(result, ConstantInt::get(i32_t, 0));
        CodeGen_CPU::create_assertion(did_succeed, Expr(), result);
    }

    // the init kernels block should branch to the post-entry block
    builder->CreateBr(post_entry);

    function_name = "";
}

template<typename CodeGen_CPU>
void CodeGen_GPU_Host<CodeGen_CPU>::visit(const Realize *op) {
    // Generate the kernel code.
    op->body.accept(this);
}

class KernelStoresToMemory : public IRVisitor {
    using IRVisitor::visit;

public:
    bool stores_to_memory;

    KernelStoresToMemory()
        : stores_to_memory(false) {
    }

    void visit(const Store *op) override {
        stores_to_memory = true;
        return;
    }
};

string create_kernel_name(const For *op) {
    // Remove already useless info from the loop name, so as to get a cleaner kernel name.
    string loop_name = op->name;
    string func_name = extract_first_token(loop_name);
    string kernel_name = unique_name("kernel_" + func_name);

    // If the kernel writes to memory, append "_WAIT_FINISH" so that the OpenCL runtime knows to wait for this
    // kernel to finish.
    KernelStoresToMemory checker;
    op->body.accept(&checker);
    if (checker.stores_to_memory) {
        // TOFIX: overlay does not work well with this change of name
        // kernel_name += "_WAIT_FINISH";
    }

    for (size_t i = 0; i < kernel_name.size(); i++) {
        if (!isalnum(kernel_name[i])) {
            kernel_name[i] = '_';
        }
    }
    return kernel_name;
}

template<typename CodeGen_CPU>
void CodeGen_GPU_Host<CodeGen_CPU>::visit(const For *loop) {
    if (CodeGen_GPU_Dev::is_gpu_var(loop->name) || ends_with(loop->name, ".run_on_device") || (target.has_feature(Target::OneAPI) && !target.has_feature(Target::IntelGPU))) {
        // We're in the loop over outermost block dimension
        debug(2) << "Kernel launch: " << loop->name << "\n";

        internal_assert(loop->device_api != DeviceAPI::Default_GPU)
            << "A concrete device API should have been selected before codegen.";

        ExtractBounds bounds;
        loop->accept(&bounds);

        debug(2) << "Kernel bounds: ("
                 << bounds.num_threads[0] << ", "
                 << bounds.num_threads[1] << ", "
                 << bounds.num_threads[2] << ", "
                 << bounds.num_threads[3] << ") threads, ("
                 << bounds.num_blocks[0] << ", "
                 << bounds.num_blocks[1] << ", "
                 << bounds.num_blocks[2] << ", "
                 << bounds.num_blocks[3] << ") blocks\n";

        // compile the kernel
        string kernel_name = create_kernel_name(loop);

        Value *null_float_ptr = ConstantPointerNull::get(CodeGen_LLVM::f32_t->getPointerTo());
        Value *zero_int32 = codegen(Expr(cast<int>(0)));

        Value *gpu_num_padded_attributes = zero_int32;
        Value *gpu_vertex_buffer = null_float_ptr;
        Value *gpu_num_coords_dim0 = zero_int32;
        Value *gpu_num_coords_dim1 = zero_int32;

        if (loop->device_api == DeviceAPI::GLSL) {

            // GL draw calls that invoke the GLSL shader are issued for pairs of
            // for-loops over spatial x and y dimensions. For each for-loop we create
            // one scalar vertex attribute for the spatial dimension corresponding to
            // that loop, plus one scalar attribute for each expression previously
            // labeled as "glsl_varying"

            // Pass variables created during setup_gpu_vertex_buffer to the
            // dev run function call.
            gpu_num_padded_attributes = codegen(Variable::make(Int(32), "glsl.num_padded_attributes"));
            gpu_num_coords_dim0 = codegen(Variable::make(Int(32), "glsl.num_coords_dim0"));
            gpu_num_coords_dim1 = codegen(Variable::make(Int(32), "glsl.num_coords_dim1"));

            // Look up the allocation for the vertex buffer and cast it to the
            // right type
            gpu_vertex_buffer = codegen(Variable::make(type_of<float *>(), "glsl.vertex_buffer"));
            gpu_vertex_buffer = builder->CreatePointerCast(gpu_vertex_buffer,
                                                           CodeGen_LLVM::f32_t->getPointerTo());
        }

        // compute a closure over the state passed into the kernel
        HostClosure c(loop->body, loop->name);

        // Determine the arguments that must be passed into the halide function
        vector<DeviceArgument> closure_args = c.arguments();

        // Sort the args by the size of the underlying type. This is
        // helpful for avoiding struct-packing ambiguities in metal,
        // which passes the scalar args as a struct.
        std::sort(closure_args.begin(), closure_args.end(),
                  [](const DeviceArgument &a, const DeviceArgument &b) {
                      if (a.is_buffer != b.is_buffer) {
                          return a.is_buffer < b.is_buffer;
                      } else if (a.type == b.type) {
                          return a.type.bits() > b.type.bits();
                      } else {
                          return a.type.is_float() < b.type.is_float();
                      }
                  });

        // Halide allows passing of scalar float and integer arguments. For
        // OpenGL, pack these into vec4 uniforms and varying attributes
        if (loop->device_api == DeviceAPI::GLSL) {

            int num_uniform_floats = 0;

            // The spatial x and y coordinates are passed in the first two
            // scalar float varying slots
            int num_varying_floats = 2;
            int num_uniform_ints = 0;

            // Pack scalar parameters into vec4
            for (size_t i = 0; i < closure_args.size(); i++) {
                if (closure_args[i].is_buffer) {
                    continue;
                } else if (ends_with(closure_args[i].name, ".varying")) {
                    closure_args[i].packed_index = num_varying_floats++;
                } else if (closure_args[i].type.is_float()) {
                    closure_args[i].packed_index = num_uniform_floats++;
                } else if (closure_args[i].type.is_int()) {
                    closure_args[i].packed_index = num_uniform_ints++;
                }
            }
        }

        for (size_t i = 0; i < closure_args.size(); i++) {
            if (closure_args[i].is_buffer && allocations.contains(closure_args[i].name)) {
                closure_args[i].size = allocations.get(closure_args[i].name).constant_bytes;
            }
        }

        CodeGen_GPU_Dev *gpu_codegen;

        if (target.has_feature(Target::OneAPI) && !target.has_feature(Target::IntelGPU)) {
            // OneAPI combines host and device code into the same file, so use that code generator
            // since there is no device API, force into OneAPI CodeGen
            debug(2) << "Using <OneAPI> for : " << kernel_name << "\n";
            gpu_codegen = ((CodeGen_OneAPI_Dev *)cgdev[DeviceAPI::OneAPI]);
        } else {
            gpu_codegen = cgdev[loop->device_api];
        }
        user_assert(gpu_codegen != nullptr)
            << "Loop is scheduled on device " << loop->device_api
            << " which does not appear in target " << target.to_string() << "\n";
        if (target.has_feature(Target::OneAPI) && !target.has_feature(Target::IntelGPU)) {
        }
        gpu_codegen->add_kernel(loop, kernel_name, closure_args);

        if (ends_with(loop->name, ".autorun.run_on_device")) {
            // Host side does not have to issue the kernel.
            return;
        }

        // get the actual name of the generated kernel for this loop
        kernel_name = gpu_codegen->get_current_kernel_name();
        debug(2) << "Compiled launch to kernel \"" << kernel_name << "\"\n";
        Value *entry_name_str = builder->CreateGlobalStringPtr(kernel_name, "entry_name");

        llvm::Type *target_size_t_type = (target.bits == 32) ? i32_t : i64_t;

        // build the kernel arguments array
        llvm::PointerType *arg_t = i8_t->getPointerTo();  // void*
        int num_args = (int)closure_args.size();

        // nullptr-terminated list
        llvm::Type *gpu_args_arr_type = ArrayType::get(arg_t, num_args + 1);
        Value *gpu_args_arr =
            create_alloca_at_entry(
                gpu_args_arr_type,
                num_args + 1, false,
                kernel_name + "_args");

        // nullptr-terminated list of size_t's
        llvm::Type *gpu_arg_sizes_arr_type = ArrayType::get(target_size_t_type, num_args + 1);
        llvm::ArrayType *gpu_arg_types_arr_type = ArrayType::get(type_t_type, num_args + 1);
        vector<Constant *> arg_types_array_entries;

        std::string api_unique_name = gpu_codegen->api_unique_name();

        Value *gpu_arg_sizes_arr = nullptr;
        bool runtime_run_takes_types = gpu_codegen->kernel_run_takes_types();

        if (!runtime_run_takes_types) {
            gpu_arg_sizes_arr =
                create_alloca_at_entry(
                    gpu_arg_sizes_arr_type,
                    num_args + 1, false,
                    kernel_name + "_arg_sizes");
        }

        llvm::Type *gpu_arg_is_buffer_arr_type = ArrayType::get(i8_t, num_args + 1);
        Value *gpu_arg_is_buffer_arr =
            create_alloca_at_entry(
                gpu_arg_is_buffer_arr_type,
                num_args + 1, false,
                kernel_name + "_arg_is_buffer");

        for (int i = 0; i < num_args; i++) {
            // get the closure argument
            string name = closure_args[i].name;
            Value *val;

            if (closure_args[i].is_buffer) {
                // If it's a buffer, get the .buffer symbol
                val = sym_get(name + ".buffer");
            } else if (ends_with(name, ".varying")) {
                // Expressions for varying attributes are passed in the
                // expression mesh. Pass a non-nullptr value in the argument array
                // to keep it in sync with the argument names encoded in the
                // shader header
                val = ConstantInt::get(target_size_t_type, 1);
            } else {
                // Otherwise just look up the symbol
                val = sym_get(name);
            }

            if (!closure_args[i].is_buffer) {
                // allocate stack space to mirror the closure element. It
                // might be in a register and we need a pointer to it for
                // the gpu args array.
                Value *ptr = create_alloca_at_entry(val->getType(), 1, false, name + ".stack");
                // store the closure value into the stack space
                builder->CreateStore(val, ptr);
                val = ptr;
            }

            // store a void * pointer to the argument into the gpu_args_arr
            Value *bits = builder->CreateBitCast(val, arg_t);
            builder->CreateStore(bits,
                                 builder->CreateConstGEP2_32(
                                     gpu_args_arr_type,
                                     gpu_args_arr,
                                     0,
                                     i));

            if (runtime_run_takes_types) {
                Constant *arg_type_fields[] = {
                    ConstantInt::get(i8_t, closure_args[i].type.code()),
                    ConstantInt::get(i8_t, closure_args[i].type.bits()),
                    ConstantInt::get(i16_t, 1)};
                arg_types_array_entries.push_back(ConstantStruct::get(type_t_type, arg_type_fields));
            } else {
                // store the size of the argument.
                int size_bytes = (closure_args[i].is_buffer) ? 8 : closure_args[i].type.bytes();
                builder->CreateStore(ConstantInt::get(target_size_t_type, size_bytes),
                                     builder->CreateConstGEP2_32(
                                         gpu_arg_sizes_arr_type,
                                         gpu_arg_sizes_arr,
                                         0,
                                         i));
            }

            builder->CreateStore(ConstantInt::get(i8_t, closure_args[i].is_buffer),
                                 builder->CreateConstGEP2_32(
                                     gpu_arg_is_buffer_arr_type,
                                     gpu_arg_is_buffer_arr,
                                     0,
                                     i));
        }
        // nullptr-terminate the lists
        builder->CreateStore(ConstantPointerNull::get(arg_t),
                             builder->CreateConstGEP2_32(
                                 gpu_args_arr_type,
                                 gpu_args_arr,
                                 0,
                                 num_args));
        if (runtime_run_takes_types) {
            Constant *arg_type_fields[] = {
                ConstantInt::get(i8_t, 0),
                ConstantInt::get(i8_t, 0),
                ConstantInt::get(i16_t, 0)};
            arg_types_array_entries.push_back(ConstantStruct::get(type_t_type, arg_type_fields));
        } else {
            builder->CreateStore(ConstantInt::get(target_size_t_type, 0),
                                 builder->CreateConstGEP2_32(
                                     gpu_arg_sizes_arr_type,
                                     gpu_arg_sizes_arr,
                                     0,
                                     num_args));
        }
        builder->CreateStore(ConstantInt::get(i8_t, 0),
                             builder->CreateConstGEP2_32(
                                 gpu_arg_is_buffer_arr_type,
                                 gpu_arg_is_buffer_arr,
                                 0,
                                 num_args));

        GlobalVariable *arg_types_array_storage = nullptr;
        if (runtime_run_takes_types) {
            arg_types_array_storage = new GlobalVariable(
                *module,
                gpu_arg_types_arr_type,
                /*isConstant*/ true,
                GlobalValue::PrivateLinkage,
                ConstantArray::get(gpu_arg_types_arr_type, arg_types_array_entries));
        }

        // TODO: only three dimensions can be passed to
        // cuLaunchKernel. How should we handle blkid[3]?
        internal_assert(is_one(bounds.num_threads[3]) && is_one(bounds.num_blocks[3]))
            << bounds.num_threads[3] << ", " << bounds.num_blocks[3] << "\n";
        debug(4) << "CodeGen_GPU_Host get_user_context returned " << get_user_context() << "\n";
        debug(3) << "bounds.num_blocks[0] = " << bounds.num_blocks[0] << "\n";
        debug(3) << "bounds.num_blocks[1] = " << bounds.num_blocks[1] << "\n";
        debug(3) << "bounds.num_blocks[2] = " << bounds.num_blocks[2] << "\n";
        debug(3) << "bounds.num_threads[0] = " << bounds.num_threads[0] << "\n";
        debug(3) << "bounds.num_threads[1] = " << bounds.num_threads[1] << "\n";
        debug(3) << "bounds.num_threads[2] = " << bounds.num_threads[2] << "\n";

        Constant *zero = ConstantInt::get(i32_t, 0);
        Value *zeros[] = {zero, zero};

        // Order-of-evaluation is guaranteed to be in order in brace-init-lists,
        // so the multiple calls to codegen here are fine
        if (loop->device_api == DeviceAPI::CM || (loop->device_api == DeviceAPI::OneAPI && loop->for_type == Halide::Internal::ForType::GPUBlock)) {
            debug(1) << "Currently, we do not implement CM runtime.\n";
            get_module_state(api_unique_name);
            return;
        }
        Value *launch_args[] = {
            get_user_context(),
            builder->CreateLoad(get_module_state(api_unique_name)),
            entry_name_str,
            codegen(bounds.num_blocks[0]),
            codegen(bounds.num_blocks[1]),
            codegen(bounds.num_blocks[2]),
            codegen(bounds.num_threads[0]),
            codegen(bounds.num_threads[1]),
            codegen(bounds.num_threads[2]),
            codegen(bounds.shared_mem_size),
            runtime_run_takes_types ? ConstantExpr::getInBoundsGetElementPtr(gpu_arg_types_arr_type, arg_types_array_storage, zeros) : builder->CreateConstGEP2_32(gpu_arg_sizes_arr_type, gpu_arg_sizes_arr, 0, 0, "gpu_arg_sizes_ar_ref" + api_unique_name),
            builder->CreateConstGEP2_32(
                gpu_args_arr_type,
                gpu_args_arr,
                0,
                0,
                "gpu_args_arr_ref" + api_unique_name),
            builder->CreateConstGEP2_32(
                gpu_arg_is_buffer_arr_type,
                gpu_arg_is_buffer_arr,
                0,
                0,
                "gpu_arg_is_buffer_ref" + api_unique_name),
            gpu_num_padded_attributes,
            gpu_vertex_buffer,
            gpu_num_coords_dim0,
            gpu_num_coords_dim1,
        };
        std::string run_fn_name;
        if (api_unique_name == "oneapi") {
            run_fn_name = "halide_opencl_run";
        } else {
            run_fn_name = "halide_" + api_unique_name + "_run";
        }
        llvm::Function *dev_run_fn = module->getFunction(run_fn_name);
        internal_assert(dev_run_fn) << "Could not find " << run_fn_name << " in module\n";
        Value *result = builder->CreateCall(dev_run_fn, launch_args);
        Value *did_succeed = builder->CreateICmpEQ(result, ConstantInt::get(i32_t, 0));

        CodeGen_CPU::create_assertion(did_succeed,
                                      // Should have already called halide_error inside the gpu runtime
                                      halide_error_code_device_run_failed,
                                      result);
    } else {

        CodeGen_CPU::visit(loop);
    }
}

template<typename CodeGen_CPU>
Value *CodeGen_GPU_Host<CodeGen_CPU>::get_module_state(const std::string &api_unique_name,
                                                       bool create) {
    std::string name = "module_state_" + function_name + "_" + api_unique_name;
    GlobalVariable *module_state = module->getGlobalVariable(name, true);
    if (!module_state && create) {
        // Create a global variable to hold the module state
        PointerType *void_ptr_type = llvm::Type::getInt8PtrTy(*context);
        module_state = new GlobalVariable(*module, void_ptr_type,
                                          false, GlobalVariable::InternalLinkage,
                                          ConstantPointerNull::get(void_ptr_type),
                                          name);
        debug(4) << "Created device module state global variable\n";
    }

    return module_state;
}

// Force template instantiation.
#ifdef WITH_X86
template class CodeGen_GPU_Host<CodeGen_X86>;
#endif

#if defined(WITH_ARM) || defined(WITH_AARCH64)
template class CodeGen_GPU_Host<CodeGen_ARM>;
#endif

#ifdef WITH_MIPS
template class CodeGen_GPU_Host<CodeGen_MIPS>;
#endif

#ifdef WITH_POWERPC
template class CodeGen_GPU_Host<CodeGen_PowerPC>;
#endif

#ifdef WITH_WEBASSEMBLY
template class CodeGen_GPU_Host<CodeGen_WebAssembly>;
#endif

#ifdef WITH_RISCV
template class CodeGen_GPU_Host<CodeGen_RISCV>;
#endif

}  // namespace Internal
}  // namespace Halide
