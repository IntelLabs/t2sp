#ifndef HALIDE_CODEGEN_ONEAPI_DEV_H
#define HALIDE_CODEGEN_ONEAPI_DEV_H

/** \file
 * Defines the code-generator for producing OneAPI C++ kernel code
 */

#include <sstream>

#include "../../Halide/src/CodeGen_C.h"
#include "../../Halide/src/CodeGen_GPU_Dev.h"
#include "../../Halide/src/Target.h"
#include "../../Halide/src/IRMutator.h"

namespace Halide {
namespace Internal {

class CodeGen_OneAPI_Dev : public CodeGen_GPU_Dev { 
public:
    CodeGen_OneAPI_Dev(Target target);

    std::string api_unique_name() override {
        return "oneapi";
    }

    /** Compile a GPU kernel into the module. This may be called many times
     * with different kernels, which will all be accumulated into a single
     * source module shared by a given Halide pipeline. */
    void add_kernel(Stmt stmt,
                    const std::string &name,
                    const std::vector<DeviceArgument> &args) override;

    // Only for Intel FPGAs: declare compiler-generated vectors/structs, and channels,
    // before the kernel code.
    void print_global_data_structures_before_kernel(const Stmt *op) {
        one_clc.print_global_data_structures_before_kernel(op);
    }
    void gather_shift_regs_allocates(const Stmt *op) {
        one_clc.gather_shift_regs_allocates(op);
    }

    /** (Re)initialize the GPU kernel module. This is separate from compile,
     * since a GPU device module will often have many kernels compiled into it
     * for a single pipeline. */
    void init_module() override;

    std::vector<char> compile_to_src() override;

    std::string get_current_kernel_name() override;

    void dump() override;

    std::string print_gpu_name(const std::string &name) override;

    // (TODO) Remove
    // std::vector<char> compile_to_src_module(const LoweredFunc &f);

    std::string compile_oneapi(const Module &input);
    void compile_oneapi_devsrc(const Module &input);
    void compile_oneapi_func(const LoweredFunc &f, const std::string &simple_name, const std::string &extern_name);
    // (TODO) Method to compile buffers as well
    // void compile_oneapi(const Buffer<> &buffer); 

protected:
    // (NOTE)  CodeGen_OneAPI_C     -(derived from)-> 
    //         CodeGen_C            -(derived from)-> 
    //         public IRPrinter     -(derived from)-> 
    //         public IRVisitor
    class CodeGen_OneAPI_C : public CodeGen_C {
    public:
        CodeGen_OneAPI_C(std::ostream &s, Target t)
            : CodeGen_C(s, t) {
        }
        void add_kernel(Stmt stmt,
                        const std::string &name,
                        const std::vector<DeviceArgument> &args);
        void print_global_data_structures_before_kernel(const Stmt *op);
        void gather_shift_regs_allocates(const Stmt *op);

        // (TODO) Remove
        // std::string compile_oneapi_lower(const LoweredFunc &f, std::string str);


    protected:
        using CodeGen_C::visit;
        bool is_standard_opencl_type(Type type);
        bool is_irregular(Region &bounds);
        std::string print_type(Type type, AppendSpaceIfNeeded append_space = DoNotAppendSpace) override;
        std::string print_reinterpret(Type type, Expr e) override;
        std::string print_extern_call(const Call *op) override;
        void add_vector_typedefs(const std::set<Type> &vector_types) override;

        // Generate code for the compiler-generated vectors and structs.
        // This class does not really mutate the IR.
        class DefineVectorStructTypes : public IRMutator {
            using IRMutator::visit;
        private:
            CodeGen_OneAPI_C* parent;
        public:
            // Definitions of non-standard vector types.
            std::string vectors;

            // Definitions of the struct types.
            std::string structs;
            
            DefineVectorStructTypes(CodeGen_OneAPI_C* parent) : parent(parent) {}
            Expr mutate(const Expr &expr) override;
            Stmt mutate(const Stmt &stmt) override;
        };
        // Ids of struct types defined
        std::vector<int> defined_struct_ids;

        std::string get_memory_space(const std::string &);

        std::string print_halide_buffer_name(const std::string &);

        // For declaring channels
        class DeclareChannels : public IRVisitor {
            using IRVisitor::visit;
        private:
            CodeGen_OneAPI_C* parent;
        public:
            std::string channels;
            DeclareChannels(CodeGen_OneAPI_C* parent) : parent(parent) {}
            void visit(const Realize *op) override;
        };

        // For declaring temporary array variable
        class DeclareArrays : public IRVisitor {
            using IRVisitor::visit;
            CodeGen_OneAPI_C* parent;
            std::set<std::string> array_set;
        public:
            std::ostringstream arrays;
            DeclareArrays(CodeGen_OneAPI_C* parent) : parent(parent) {}
            void visit(const Call *op) override;
        };

        // For unrolling loop with different strategies
        class CheckConditionalChannelAccess : public IRVisitor {
            using IRVisitor::visit;
        private:
            CodeGen_OneAPI_C* parent;
            std::string current_loop_name;
        public:
            bool in_if_then_else;       // The current IR is in a branch
            bool conditional_access;    // There is a conditional execution of channel read/write inside the current loop
            bool irregular_loop_dep;    // There is a irregular loop inside the current loop and the irregular bound 
                                        // depends on current loop var
            CheckConditionalChannelAccess(CodeGen_OneAPI_C* parent, std::string current_loop_name) : parent(parent), current_loop_name(current_loop_name) {
                in_if_then_else = false;
                conditional_access = false;
                irregular_loop_dep = false;
            }
            void visit(const IfThenElse *op) override;
            void visit(const For *op) override;
            void visit(const Call *op) override;
        };

        // For allocating shift registers
        class GatherShiftRegsAllocates : public IRVisitor {
            using IRVisitor::visit;
        private:
            CodeGen_OneAPI_C* parent;
            std::map<std::string, std::vector<std::string>> &shift_regs_allocates; // For all shift regs
            std::map<std::string, size_t> &shift_regs_bounds; // Only for shift regs whose types are nonstandard_vectors
            std::map<std::string, std::vector<Expr>> &space_vars;
        public:
            GatherShiftRegsAllocates(CodeGen_OneAPI_C* parent, std::map<std::string, std::vector<std::string>> &shift_regs_allocates,
                std::map<std::string, size_t> &shift_regs_bounds, std::map<std::string, std::vector<Expr>> &space_vars) :
                    parent(parent), shift_regs_allocates(shift_regs_allocates), shift_regs_bounds(shift_regs_bounds), space_vars(space_vars) {}
            void visit(const Call *op) override;
            void visit(const Realize *op) override;
            void print_irregular_bounds_allocates(std::string reg_name, std::string type, std::string name, Region space_bounds, Region time_bounds, int space_bound_level);
        };
        std::map<std::string, std::vector<std::string>> shift_regs_allocates; // For all shift regs
        std::map<std::string, size_t> shift_regs_bounds; // Only for shift regs whose types are nonstandard_vectors
        std::map<std::string, std::vector<Expr>> space_vars; // For shift regs with irregular bounds
        // For saving the pointer args streamed from scehduler
        std::map<std::string, std::string> pointer_args;
        std::vector<DeviceArgument> buffer_args;

        typedef std::vector< std::tuple<Stmt, std::string, std::vector<DeviceArgument> > > kernel_args_vector;
        kernel_args_vector kernel_args;
        const std::string OneAPIDefineVectorStructTypes = "\n// CodeGen_OneAPI DefineVectorStructTypes \n";
        const std::string OneAPIDeclareChannels = "\n// CodeGen_OneAPI DeclareChannels \n";

        void visit(const For *) override;
        void visit(const Ramp *op) override;
        void visit(const Broadcast *op) override;
        void visit(const Call *op) override;
        void visit(const Load *op) override;
        void visit(const Store *op) override;
        void visit(const Cast *op) override;
        void visit(const Select *op) override;
        void visit(const IfThenElse *op) override;
        void visit(const EQ *) override;
        void visit(const NE *) override;
        void visit(const LT *) override;
        void visit(const LE *) override;
        void visit(const GT *) override;
        void visit(const GE *) override;
        void visit(const Allocate *op) override;
        void visit(const Free *op) override;
        void visit(const AssertStmt *op) override;
        void visit(const Shuffle *op) override;
        void visit(const Min *op) override;
        void visit(const Max *op) override;
        void visit(const Atomic *op) override;
        // Only meaningful for Intel FPGAs.
        void visit(const Realize *op) override;
        void visit(const Provide *op) override;
        void visit_binop(Type t, Expr a, Expr b, const char *op) override;
        void cast_to_bool_vector(Type bool_type, Type other_type, std::string other_var);
        std::string vector_index_to_string(int idx);
        std::string print_load_assignment(Type t, const std::string &rhs);
    };

    std::ostringstream src_stream_oneapi;
    std::string cur_kernel_name_oneapi;
    CodeGen_OneAPI_C one_clc;

        
    // Wrapper for CodeGen_OneAPI_C to add host/device code 
    // Note: CodeGen_OneAPI_C still checks for user contexts and throws error
    class EmitOneAPIFunc : public CodeGen_OneAPI_C {
        friend class CodeGen_OneAPI_C;
        using CodeGen_OneAPI_C::visit;
        // using CodeGen_OneAPI_C::print_type;
        using CodeGen_C::compile;
    protected:
        // From CodeGen_GPU_Host
        std::string create_kernel_name(const For *op);
        
        // Form CodeGen_OneAPI_C
        void visit(const For *) override;
        void visit(const Allocate *) override;    
        void visit(const AssertStmt *) override;
        void visit(const Free *op) override;
        std::string print_extern_call(const Call *) override;
        std::string print_reinterpret(Type type, Expr e) override;
        std::string print_type(Type type, AppendSpaceIfNeeded append_space = DoNotAppendSpace) override;

        // New 
        void create_kernel_wrapper(const std::string &name, std::string q_device_name, const std::vector<DeviceArgument> &args, bool begining, bool is_run_on_device);

    private:
        // Parent Class pointer
        CodeGen_OneAPI_C* parent;

        // Record to check when we need to use CodeGen_OneAPI_C implementation or not
        // parent's first name
        std::vector< std::tuple<std::string, const void *, const void *> > emit_visited_op_names;

        // set to true/false inside add_kernel()
        bool currently_inside_kernel;

        // stream point to clean and reset from
        const std::string EmitOneAPIFunc_Marker = "\n// EmitOneAPIFunc MARKER \n";

        // pointer to src stream
        std::ostringstream *stream_ptr;

        std::set<std::string> UnImplementedExternFuncs;


        typedef struct {

            EmitOneAPIFunc *p;

            void check_valid(const Call *op, unsigned int max_args, unsigned int min_args=0){
                internal_assert(op);
                internal_assert(p);
                internal_assert(op->args.size() <= max_args && op->args.size() >= min_args);
            }

            // user context functions

            std::string halide_device_malloc(const Call *op) {
                check_valid(op, 2, 1);
                std::ostringstream rhs;
                std::vector<Expr> args = op->args;
                std::string buff = p->print_expr(args[0]);

                rhs << "if( !" << buff << "->device ){ // device malloc\n";
                rhs << p->get_indent() << "  std::cout << \"//\t device malloc "<< buff << "\\n\";\n";
                rhs << p->get_indent() << "  assert(" << buff << "->size_in_bytes() != 0);\n";
                rhs << p->get_indent() << "  uint64_t lowest_index = 0;\n";
                rhs << p->get_indent() << "  uint64_t highest_index = 0;\n";
                rhs << p->get_indent() << "  for (int i = 0; i < " << buff << "->dimensions; i++) {\n";
                rhs << p->get_indent() << "      if (" << buff << "->dim[i].stride < 0) {\n";
                rhs << p->get_indent() << "          lowest_index += (uint64_t)(" << buff << "->dim[i].stride) * (" << buff << "->dim[i].extent - 1);\n";
                rhs << p->get_indent() << "      }\n";
                rhs << p->get_indent() << "      if (" << buff << "->dim[i].stride > 0) {\n";
                rhs << p->get_indent() << "          highest_index += (uint64_t)(" << buff << "->dim[i].stride) * (" << buff << "->dim[i].extent - 1);\n";
                rhs << p->get_indent() << "      }\n";
                rhs << p->get_indent() << "  }\n";
                rhs << p->get_indent() << "  device_handle *dev_handle = (device_handle *)std::malloc(sizeof(device_handle));\n";
                rhs << p->get_indent() << "  dev_handle->mem = (void*)sycl::malloc_device(" << buff << "->size_in_bytes(), q_device);\n";
                rhs << p->get_indent() << "  dev_handle->offset = 0;\n";
                rhs << p->get_indent() << "  " << buff << "->device = (uint64_t)dev_handle;\n";
                rhs << p->get_indent() << "}";

                return rhs.str();
            }

            std::string halide_host_malloc(const Call *op){
                check_valid(op, 2, 1);
                std::ostringstream rhs;
                std::vector<Expr> args = op->args;   
                std::string buffer_name = p->print_expr(args[0]);
                rhs << "{ // host malloc\n";
                rhs << p->get_indent() << "  std::cout << \"//\\t host malloc "<< buffer_name << "\\n\";\n";
                rhs << p->get_indent() << "  assert(" << buffer_name << "->size_in_bytes() != 0);\n";
                rhs << p->get_indent() << "  " << buffer_name << "->host = (uint8_t*)std::malloc(" << buffer_name << "->size_in_bytes() );\n";
                rhs << p->get_indent() << "  assert(" << buffer_name << "->host != NULL);\n";
                rhs << p->get_indent() << "}";              
                return rhs.str();
            }            

            std::string halide_copy_to_device(const Call *op){
                check_valid(op, 2, 1);
                std::ostringstream rhs;
                std::vector<Expr> args = op->args;
                std::string buffer_name = p->print_expr(args[0]);
                // rhs << "std::cout << \"//\\t memcpy device->host " << buffer_name << "\\n\";\n";
                // rhs << p->get_indent()
                //     << "q_device.submit([&](handler& h){ h.memcpy("
                //     << "(void *)(((device_handle*)"<< buffer_name <<"->device)->mem), " // dst
                //     << "(void *)" << buffer_name << "->host, " // src
                //     << buffer_name << "->size_in_bytes() ); })"; // size
                
                rhs << halide_opencl_buffer_copy(op, false);
                return rhs.str();
            }

            std::string halide_copy_to_host(const Call *op){
                check_valid(op, 2, 1);
                std::ostringstream rhs;
                std::vector<Expr> args = op->args;
                std::string buffer_name = p->print_expr(args[0]);
                // rhs << "std::cout << \"//\\t memcpy host->device " << buffer_name << "\\n\";\n";
                // rhs << p->get_indent()
                //     << "q_device.submit([&](handler& h){ h.memcpy("
                //     << "(void *)" << buffer_name << "->host, " // dst
                //     << "(void *)(((device_handle*)"<< buffer_name <<"->device)->mem), " // src
                //     << buffer_name << "->size_in_bytes() ); })"; // size

                rhs << halide_opencl_buffer_copy(op, true);
                return rhs.str();
            }

            std::string create_memcpy(std::string buffer_name, bool to_host){
                std::ostringstream rhs;
                std::string dst;
                std::string src;
                if(to_host){
                    dst = buffer_name + "->host";
                    src = "(((device_handle*)" + buffer_name + "->device)->mem)";
                } else {
                    dst = "(((device_handle*)" + buffer_name + "->device)->mem)";
                    src = buffer_name + "->host";
                }
                rhs << "q_device.submit([&](handler& h){ h.memcpy("
                    << "(void *)" << dst << ", " // dst
                    << "(void *)" << src << ", " // src
                    << buffer_name << "->size_in_bytes() ); }).wait()"; // size
                return rhs.str();
            }

            std::string halide_opencl_buffer_copy(const Call* op, bool to_host){
                // (NOTE) Implemented based on AOT-OpenCL-Runtime.cpp
                check_valid(op, 2, 1);
                std::ostringstream rhs;
                std::vector<Expr> args = op->args;
                std::string buffer_name = p->print_expr(args[0]);

                rhs << "{ // memcpy \n";
                rhs << p->get_indent() << "  bool from_host = (" << buffer_name << "->device == 0) || ("<< buffer_name <<"->host_dirty() && "<< buffer_name << "->host != NULL);\n";
                rhs << p->get_indent() << "  bool to_host = "<< to_host <<";\n";
                rhs << p->get_indent() << "  if (!from_host && to_host) {\n";
                rhs << p->get_indent() << "    std::cout << \"//\t memcpy device->host " << buffer_name << "\\n\";\n";
                rhs << p->get_indent() << "    " << create_memcpy(buffer_name , true)  << ";\n";
                rhs << p->get_indent() << "  } else if (from_host && !to_host) {\n";
                rhs << p->get_indent() << "    std::cout << \"//\t memcpy host->device " << buffer_name << "\\n\";\n";
                rhs << p->get_indent() << "    " << create_memcpy(buffer_name , false)  << ";\n";
                rhs << p->get_indent() << "  } else if (!from_host && !to_host) {\n";
                rhs << p->get_indent() << "    std::cout << \"//\t memcpy device->device not implemented yet\\n\";\n";
                rhs << p->get_indent() << "    assert(false);\n";
                // (TODO), create this to be more modularly to have a src and dst buffers available
                // rhs << p->get_indent() << "  } else if ("<< buffer_name << "->host != " << buffer_name << "->host) {\n";
                // rhs << p->get_indent() << "    std::cout << \"//\t memcpy host->host " << buffer_name << " ignored...\\n\";\n";
                // rhs << p->get_indent() << "    memcpy((void *)( " << buffer_name << "->host), (void *)(" << buffer_name << "->host), "<< buffer_name << "->size_in_bytes());\n";
                rhs << p->get_indent() << "  } else {\n";
                rhs << p->get_indent() << "    std::cout << \"//\t memcpy "<< buffer_name << " Do nothing.\\n\";\n";
                rhs << p->get_indent() << "  }\n";
                rhs << p->get_indent() << "}";
                return rhs.str();
            }

            std::string halide_device_and_host_free(const Call *op){
                check_valid(op, 2, 1);
                std::ostringstream rhs;
                std::vector<Expr> args = op->args;
                std::string buffer_name = p->print_expr(args[0]);
                rhs << halide_device_host_nop_free(buffer_name);
                return rhs.str();
            }



            std::string halide_device_and_host_malloc(const Call *op){
                check_valid(op, 2, 1);
                std::ostringstream rhs;
                std::vector<Expr> args = op->args;
                // Device Malloc
                rhs << halide_device_malloc(op);
                rhs << ";\n";
                // Host Malloc
                rhs << p->get_indent() << halide_host_malloc(op);
                return rhs.str();
            }

            std::string halide_opencl_wait_for_kernels_finish(const Call *op){
                check_valid(op, 0, 0);
                std::ostringstream rhs;
                std::vector<Expr> args = op->args;
                // Wait for all kernels to finish
                rhs << "for(unsigned int i = 0; i < oneapi_kernel_events.size(); i++){ "
                    << "oneapi_kernel_events.at(i).wait(); }";
                return rhs.str();
            }

            // Defined in DeviceInterface.cpp 

            std::string halide_oneapi_device_interface(const Call *op){
                return "NULL";
            }

            // Buffer Functions i.e. without user context functions


            // Make for use with visit(const Allocate)
            std::string halide_device_host_nop_free(std::string buffer_name){
                internal_assert(p);
                std::ostringstream rhs;

                // Free device
                rhs << "if( " << buffer_name << "->device ){ // device free\n";
                rhs << p->get_indent() << "  sycl::free( ((device_handle*)" << buffer_name << "->device)->mem , q_device);\n";
                rhs << p->get_indent() << "  assert(((device_handle *)" << buffer_name << "->device)->offset == 0);\n";
                rhs << p->get_indent() << "  std::free((device_handle *)" << buffer_name << "->device);\n";
                rhs << p->get_indent() << "  "<< buffer_name <<"->set_device_dirty(false);\n";
                rhs << p->get_indent() << "}\n";

                // Free host
                rhs << p->get_indent() << "if(" << buffer_name << "->host){ // host free\n";
                rhs << p->get_indent() << "  std::free( (void*)"<< buffer_name <<"->host );\n";
                rhs << p->get_indent() << "  "<< buffer_name <<"->host = NULL;\n";
                rhs << p->get_indent() << "  "<< buffer_name <<"->set_host_dirty(false);\n";
                rhs << p->get_indent() << "}";
                return rhs.str();
            }

        } ExternCallFuncs;

        ExternCallFuncs ext_funcs;

        // Used for converting print_extern_calls to OneAPI calls
        // (TODO) implement a switch function for any external calls that are made
        // by those other than Call* op(s)
        using DoFunc = std::string(ExternCallFuncs::*)(const Call *op);
        std::map<std::string, DoFunc> print_extern_call_map = {
            // All user context functions here

            // Halide buffer type functions
            // See CodeGen_Internal.cpp for refrence

            // {"halide_buffer_copy" , NULL}, 
            {"halide_copy_to_host" , &ExternCallFuncs::halide_copy_to_host}, 
            {"halide_copy_to_device" , &ExternCallFuncs::halide_copy_to_device}, 
            // {"halide_current_time_ns" , NULL}, 
            // {"halide_debug_to_file" , NULL}, 
            // {"halide_device_free" , NULL}, 
            // {"halide_device_host_nop_free" , NULL}, 
            // {"halide_device_free_as_destructor" , NULL}, 
            {"halide_device_and_host_free" , &ExternCallFuncs::halide_device_and_host_free}, 
            // {"halide_device_and_host_free_as_destructor" , NULL}, 
            {"halide_device_malloc" , &ExternCallFuncs::halide_device_malloc}, 
            {"halide_device_and_host_malloc" , &ExternCallFuncs::halide_device_and_host_malloc}, 
            // {"halide_device_sync" , NULL}, 
            // {"halide_do_par_for" , NULL}, 
            // {"halide_do_loop_task" , NULL}, 
            // {"halide_do_task" , NULL}, 
            // {"halide_do_async_consumer" , NULL}, 
            // {"halide_error" , NULL}, 
            // {"halide_free" , NULL}, 
            // {"halide_malloc" , NULL}, 
            // {"halide_print" , NULL}, 
            // {"halide_profiler_memory_allocate" , NULL}, 
            // {"halide_profiler_memory_free" , NULL}, 
            // {"halide_profiler_pipeline_start" , NULL}, 
            // {"halide_profiler_pipeline_end" , NULL}, 
            // {"halide_profiler_stack_peak_update" , NULL}, 
            // {"halide_spawn_thread" , NULL}, 
            // {"halide_device_release" , NULL}, 
            // {"halide_start_clock" , NULL}, 
            // {"halide_trace" , NULL}, 
            // {"halide_trace_helper" , NULL}, 
            // {"halide_memoization_cache_lookup" , NULL}, 
            // {"halide_memoization_cache_store" , NULL}, 
            // {"halide_memoization_cache_release" , NULL}, 
            // {"halide_cuda_run" , NULL}, 
            // {"halide_opencl_run" , NULL}, 
            // {"halide_opengl_run" , NULL}, 
            // {"halide_openglcompute_run" , NULL}, 
            // {"halide_metal_run" , NULL}, 
            // {"halide_d3d12compute_run" , NULL}, 
            // {"halide_msan_annotate_buffer_is_initialized_as_destructor" , NULL}, 
            // {"halide_msan_annotate_buffer_is_initialized" , NULL}, 
            // {"halide_msan_annotate_memory_is_initialized" , NULL}, 
            // {"halide_hexagon_initialize_kernels" , NULL}, 
            // {"halide_hexagon_run" , NULL}, 
            // {"halide_hexagon_device_release" , NULL}, 
            // {"halide_hexagon_power_hvx_on" , NULL}, 
            // {"halide_hexagon_power_hvx_on_mode" , NULL}, 
            // {"halide_hexagon_power_hvx_on_perf" , NULL}, 
            // {"halide_hexagon_power_hvx_off" , NULL}, 
            // {"halide_hexagon_power_hvx_off_as_destructor" , NULL}, 
            // {"halide_qurt_hvx_lock" , NULL}, 
            // {"halide_qurt_hvx_unlock" , NULL}, 
            // {"halide_qurt_hvx_unlock_as_destructor" , NULL}, 
            // {"halide_vtcm_malloc" , NULL}, 
            // {"halide_vtcm_free" , NULL}, 
            // {"halide_cuda_initialize_kernels" , NULL}, 
            // {"halide_opencl_initialize_kernels" , NULL}, 
            // {"halide_opengl_initialize_kernels" , NULL}, 
            // {"halide_openglcompute_initialize_kernels" , NULL}, 
            // {"halide_metal_initialize_kernels" , NULL}, 
            // {"halide_d3d12compute_initialize_kernels" , NULL}, 
            // {"halide_get_gpu_device" , NULL}, 
            // {"halide_upgrade_buffer_t" , NULL}, 
            // {"halide_downgrade_buffer_t" , NULL}, 
            // {"halide_downgrade_buffer_t_device_fields" , NULL}, 
            // {"_halide_buffer_crop" , NULL}, 
            // {"_halide_buffer_retire_crop_after_extern_stage" , NULL}, 
            // {"_halide_buffer_retire_crops_after_extern_stage" , NULL}, 
            {"halide_opencl_wait_for_kernels_finish" , &ExternCallFuncs::halide_opencl_wait_for_kernels_finish},

            // All Non-user context functions here

            // Buffer functions
            // See IR.cpp and buffer_t.cpp for refrence

            // {"_halide_buffer_get_dimensions", &ExternCallFuncs::_halide_buffer_get_dimensions}, 
            // {"_halide_buffer_get_min", &ExternCallFuncs::_halide_buffer_get_min}, 
            // {"_halide_buffer_get_extent", &ExternCallFuncs::_halide_buffer_get_extent}, 
            // {"_halide_buffer_get_stride", &ExternCallFuncs::_halide_buffer_get_stride}, 
            // {"_halide_buffer_get_max", NULL}, 
            // {"_halide_buffer_get_host", &ExternCallFuncs::_halide_buffer_get_host}, 
            // {"_halide_buffer_get_device", NULL}, 
            // {"_halide_buffer_get_device_interface", NULL}, 
            // {"_halide_buffer_get_shape", &ExternCallFuncs::_halide_buffer_get_shape}, 
            // {"_halide_buffer_get_host_dirty", NULL}, 
            // {"_halide_buffer_get_device_dirty", NULL}, 
            // {"_halide_buffer_get_type", &ExternCallFuncs::_halide_buffer_get_type}, 
            // {"_halide_buffer_set_host_dirty", NULL}, 
            // {"_halide_buffer_set_device_dirty", NULL}, 
            // {"_halide_buffer_is_bounds_query", NULL}, 
            // {"_halide_buffer_init", NULL}, 
            // {"_halide_buffer_init_from_buffer", NULL}, 
            // {"_halide_buffer_crop", NULL}, 
            // {"_halide_buffer_set_bounds", NULL}, 
            // {"halide_trace_helper", NULL},

            // Defined in DeviceInterface.cpp 
            // Replaced here by directly returning Null insteaf of an AOT-...-Runtime.cpp

            {"halide_oneapi_device_interface", &ExternCallFuncs::halide_oneapi_device_interface}

        };

        // Create a simple assert(false) depending on the id_cond passed in
        void create_assertion(const std::string &id_cond, const std::string &id_msg){
            // (NOTE) Implementation mimincing CodeGen_C::create_assertion(const string &id_cond, const string &id_msg)
            if (target.has_feature(Target::NoAsserts)) return;

            stream << get_indent() << "if (!" << id_cond << ")\n";
            open_scope();
            stream << get_indent() << "std::cout << \"Condition '" <<  id_cond << "' failed "
                   << "with error id_msg: " << id_msg
                   << "\\n\";\n";
            stream << get_indent() << "assert(false);\n"; 
            // stream << get_indent() << "return " << id_msg << ";\n";
            close_scope("");
        }

        std::string RunTimeHeaders(){
            std::ostringstream rhs;
            rhs << "#include <CL/sycl.hpp>\n";
            rhs << "#include <sycl/ext/intel/fpga_extensions.hpp>\n";
            rhs << "#include \"dpc_common.hpp\"\n";
            rhs << "#include \"pipe_array.hpp\"\n";
            rhs << "using namespace sycl;\n";
            rhs << "void halide_device_and_host_free_as_destructor(void *user_context, void *obj) {\n";
            rhs << "}\n";
            rhs << "struct device_handle {\n";
            rhs << "    // Important: order these to avoid any padding between fields;\n";
            rhs << "    // some Win32 compiler optimizer configurations can inconsistently\n";
            rhs << "    // insert padding otherwise.\n";
            rhs << "    uint64_t offset;\n";
            rhs << "    void* mem;\n";
            rhs << "};\n"; 
            return rhs.str();        
        }


    public:
        EmitOneAPIFunc(CodeGen_OneAPI_C* parent, std::ostringstream &s, Target t) : 
            CodeGen_OneAPI_C(s, t) {
                parent = parent;
                stream << EmitOneAPIFunc_Marker;
                stream << RunTimeHeaders();
                stream_ptr = &s;
                ext_funcs.p = this;
                currently_inside_kernel = false;
        }

        // (TODO) Make a function to use instead of void CodeGen_C::compile(const Module &input) 
        // b/c it is not overridable

        // From CodeGen_C 
        void compile(const LoweredFunc &func) override;

        // From CodeGen_OneAPI_C 
        void add_kernel(Stmt stmt,
                        const std::string &name,
                        const std::vector<DeviceArgument> &args);

        std::string get_str(){
            std::set<std::string>::iterator it = UnImplementedExternFuncs.begin();
            while (it != UnImplementedExternFuncs.end()){
                // Output all the unimplemented external functions that have been called but not caught
                stream << get_indent() << "// " << (*it) << " to-be-implemented\n";
                it++;
            }

            // return everything after the EmitOneAPIFunc_Marker  
            // This removes any extra output returned from the parent class initalization
            std::string str = stream_ptr->str();
            // size_t pos = str.find(EmitOneAPIFunc_Marker) + EmitOneAPIFunc_Marker.size();
            // if(pos == std::string::npos) pos = 0;
            // std::string ret = str.substr(pos);
            // return ret;
            return str;
        };

    };  

private:

    // Methods only for generating OpenCL code for Intel FPGAs
    void compile_to_aocx_oneapi(std::ostringstream &src_stream);

};

}  // namespace Internal
}  // namespace Halide

#endif
