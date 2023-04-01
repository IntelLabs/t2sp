#ifndef HALIDE_CODEGEN_CLEAR_ONEAPI_DEV_H
#define HALIDE_CODEGEN_CLEAR_ONEAPI_DEV_H

/** \file
 * Defines the code-generator for producing OneAPI C kernel code
 */

#include <sstream>

#include "CodeGen_Clear_C.h"
#include "CodeGen_GPU_Dev.h"
#include "Target.h"
#include "IRMutator.h"

namespace Halide {
namespace Internal {

class CodeGen_Clear_OneAPI_Dev : public CodeGen_GPU_Dev {
public:
    CodeGen_Clear_OneAPI_Dev(Target target);

    /** Compile a GPU kernel into the module. This may be called many times
     * with different kernels, which will all be accumulated into a single
     * source module shared by a given Halide pipeline. */
    void add_kernel(Stmt stmt,
                    const std::string &name,
                    const std::vector<DeviceArgument> &args) override;

    // Only for Intel FPGAs: declare compiler-generated vectors/structs, and channels,
    // before the kernel code.
    void print_global_data_structures_before_kernel(const Stmt *op) {
        clc.print_global_data_structures_before_kernel(op);
    }
    void gather_shift_regs_allocates(const Stmt *op) {
        clc.gather_shift_regs_allocates(op);
    }

    /** (Re)initialize the GPU kernel module. This is separate from compile,
     * since a GPU device module will often have many kernels compiled into it
     * for a single pipeline. */
    void init_module() override;

    std::vector<char> compile_to_src() override;

    std::string get_current_kernel_name() override;

    void dump() override;

    std::string print_gpu_name(const std::string &name) override;

    std::string api_unique_name() override {
        return "oneapi";
    }

    std::string compile(const Module &input);

protected:
    class CodeGen_Clear_OneAPI_C : public CodeGen_Clear_C {
    private:
        bool needs_intermediate_expr = false;
        // Generate intermediate variables if needs_intermediate_expr.
        // As the calculation of some expressions may be complicated or have
        // side-effects, so we need to store their results in intermediate variables.
        std::string generate_intermediate_var(const std::string &);
    public:
        CodeGen_Clear_OneAPI_C(std::ostream &s, Target t)
            : CodeGen_Clear_C(s, t) {
        }
        void add_kernel(Stmt stmt,
                        const std::string &name,
                        const std::vector<DeviceArgument> &args);
        void print_global_data_structures_before_kernel(const Stmt *op);
        void gather_shift_regs_allocates(const Stmt *op);

    protected:
        using CodeGen_Clear_C::visit;
        bool is_standard_opencl_type(Type type);
        bool is_irregular(Region &bounds);
        std::string print_type(Type type, AppendSpaceIfNeeded append_space = DoNotAppendSpace) override;
        std::string print_reinterpret(Type type, Expr e) override;
        std::string print_extern_call(const Call *op) override;
        void add_vector_typedefs(const std::set<Type> &vector_types) override;
        std::string print_name(const std::string &name) override;
        bool succinct_name_is_unique(const std::string &verbose, const std::string &succinct);
        void map_verbose_to_succinct_globally(const std::string &verbose, const std::string &succinct);
        void map_verbose_to_succinct_locally(const std::string &verbose, const std::string &succinct);

        // Generate code for the compiler-generated vectors and structs.
        // This class does not really mutate the IR.
        class DefineVectorStructTypes : public IRMutator {
            using IRMutator::visit;
        private:
            CodeGen_Clear_OneAPI_C* parent;
        public:
            // Definitions of non-standard vector types.
            std::string vectors;

            // Definitions of the struct types.
            std::string structs;
            
            DefineVectorStructTypes(CodeGen_Clear_OneAPI_C* parent) : parent(parent) {}
            Expr mutate(const Expr &expr) override;
            Stmt mutate(const Stmt &stmt) override;
        };
        // Ids of struct types defined
        std::vector<int> defined_struct_ids;

        // For declaring channels
        class DeclareChannels : public IRVisitor {
            using IRVisitor::visit;
        private:
            CodeGen_Clear_OneAPI_C* parent;
        public:
            std::string channels;
            DeclareChannels(CodeGen_Clear_OneAPI_C* parent) : parent(parent) {}
            void visit(const Realize *op) override;
        };

        // For declaring temporary array variable
        class DeclareArrays : public IRVisitor {
            using IRVisitor::visit;
            CodeGen_Clear_OneAPI_C* parent;
            std::set<std::string> array_set;
        public:
            std::ostringstream arrays;
            DeclareArrays(CodeGen_Clear_OneAPI_C* parent) : parent(parent) {}
            void visit(const Call *op) override;
        };

        // For unrolling loop with different strategies
        class CheckConditionalChannelAccess : public IRVisitor {
            using IRVisitor::visit;
        private:
            CodeGen_Clear_OneAPI_C* parent;
            std::string current_loop_name;
        public:
            bool in_if_then_else;       // The current IR is in a branch
            bool conditional_access;    // There is a conditional execution of channel read/write inside the current loop
            bool irregular_loop_dep;    // There is a irregular loop inside the current loop and the irregular bound 
                                        // depends on current loop var
            CheckConditionalChannelAccess(CodeGen_Clear_OneAPI_C* parent, std::string current_loop_name) : parent(parent), current_loop_name(current_loop_name) {
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
            CodeGen_Clear_OneAPI_C* parent;
            std::map<std::string, std::vector<std::string>> &shift_regs_allocates; // For all shift regs
            std::map<std::string, size_t> &shift_regs_bounds; // Only for shift regs whose types are nonstandard_vectors
            std::map<std::string, std::vector<Expr>> &space_vars;
        public:
            GatherShiftRegsAllocates(CodeGen_Clear_OneAPI_C* parent, std::map<std::string, std::vector<std::string>> &shift_regs_allocates,
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

        // Maps from verbose names to succinct names
        std::map<std::string, std::string> global_name_map;
        std::map<std::string, std::string> kernel_name_map;

        // OneAPI-specific
        typedef std::vector<std::tuple<Stmt, std::string, std::vector<DeviceArgument>>> kernel_args_vector;
        kernel_args_vector kernel_args;
        const std::string OneAPIDefineVectorStructTypes = "\n// CodeGen_OneAPI DefineVectorStructTypes \n";
        const std::string OneAPIDeclareChannels = "\n// CodeGen_OneAPI DeclareChannels \n";

        void visit(const For *) override;
        void visit(const Ramp *op) override;
        void visit(const Broadcast *op) override;
        void visit(const Call *op) override;
        void visit(const Let *) override;
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
        void visit(const LetStmt *) override;
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
    };

    std::ostringstream src_stream;
    std::string cur_kernel_name;
    CodeGen_Clear_OneAPI_C clc;

    // Wrapper for CodeGen_Clear_OneAPI_C to generate host and device code into a single string
    class EmitOneAPIFunc : public CodeGen_Clear_OneAPI_C {
        friend class CodeGen_Clear_OneAPI_C;
        using CodeGen_Clear_OneAPI_C::visit;
        using CodeGen_Clear_C::compile;
    protected:
        std::string create_kernel_name(const For *op);
        std::string print_extern_call(const Call *) override;
        std::string print_reinterpret(Type type, Expr e) override;
        std::string print_type(Type type, AppendSpaceIfNeeded append_space = DoNotAppendSpace) override;
        void create_kernel_wrapper(const std::string &name, std::string q_device_name, const std::vector<DeviceArgument> &args, bool begining, bool is_run_on_device);

        void visit(const For *) override;
        void visit(const Allocate *) override;
        void visit(const AssertStmt *) override;
        void visit(const Free *) override;
        void visit(const Evaluate *) override;
    private:
        // Parent Class pointer
        CodeGen_Clear_OneAPI_C* parent;

        // Record to check when we need to use CodeGen_Clear_OneAPI_C implementation or not
        // parent's first name
        std::vector<std::tuple<std::string, const void *, const void *>> emit_visited_op_names;

        // set to true/false inside add_kernel()
        bool currently_inside_kernel;

        // pointer to src stream
        std::ostringstream *stream_ptr;

        std::set<std::string> UnImplementedExternFuncs;

        struct ExternCallFuncs {
            EmitOneAPIFunc *p;

            void check_valid(const Call *op, unsigned int max_args, unsigned int min_args=0);

            // user context functions
            std::string halide_device_malloc(const Call *op);
            std::string halide_host_malloc(const Call *op);
            std::string halide_copy_to_device(const Call *op);
            std::string halide_copy_to_host(const Call *op);
            std::string create_memcpy(std::string buffer_name, bool to_host);
            std::string halide_opencl_buffer_copy(const Call* op, bool to_host);
            std::string halide_device_and_host_free(const Call *op);
            std::string halide_device_and_host_malloc(const Call *op);
            std::string halide_opencl_wait_for_kernels_finish(const Call *op);

            // Defined in DeviceInterface.cpp
            std::string halide_oneapi_device_interface(const Call *op){
                return "NULL";
            }

            // Buffer Functions i.e. without user context functions

            // Make for use with visit(const Allocate)
            std::string halide_device_host_nop_free(std::string buffer_name);
        };

        ExternCallFuncs ext_funcs;

        // Used for converting print_extern_calls to OneAPI calls
        using DoFunc = std::string(ExternCallFuncs::*)(const Call *op);
        std::map<std::string, DoFunc> print_extern_call_map = {
            {"halide_copy_to_host" , &ExternCallFuncs::halide_copy_to_host},
            {"halide_copy_to_device" , &ExternCallFuncs::halide_copy_to_device},
            {"halide_device_and_host_free" , &ExternCallFuncs::halide_device_and_host_free},
            {"halide_device_malloc" , &ExternCallFuncs::halide_device_malloc},
            {"halide_device_and_host_malloc" , &ExternCallFuncs::halide_device_and_host_malloc},
            {"halide_opencl_wait_for_kernels_finish" , &ExternCallFuncs::halide_opencl_wait_for_kernels_finish},
            {"halide_oneapi_device_interface", &ExternCallFuncs::halide_oneapi_device_interface}
        };

        // Create a simple assert(false) depending on the id_cond passed in
        void create_assertion(const std::string &id_cond, const std::string &id_msg);

    public:
        EmitOneAPIFunc(CodeGen_Clear_OneAPI_C* parent, std::ostringstream &s, Target t) : 
            CodeGen_Clear_OneAPI_C(s, t), parent(parent) { // Note this assocates s with stream in CodeGen_Clear_OneAPI_C, which is inherited from IRPrinter
                stream_ptr = &s;
                ext_funcs.p = this;
                currently_inside_kernel = false;
        }

        void compile(const LoweredFunc &func) override;
        void add_kernel(Stmt stmt,
                        const std::string &name,
                        const std::vector<DeviceArgument> &args);
        std::string get_str();
        void clean_stream();
        void write_runtime_headers(); 
    };
};

}  // namespace Internal
}  // namespace Halide

#endif
