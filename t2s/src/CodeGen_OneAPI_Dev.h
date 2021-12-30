#ifndef HALIDE_CODEGEN_ONEAPI_DEV_H
#define HALIDE_CODEGEN_ONEAPI_DEV_H

/** \file
 * Defines the code-generator for producing OpenCL C kernel code
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

    std::vector<char> compile_to_src_module(const LoweredFunc &f);


protected:
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

        std::string compile_oneapi_lower(const LoweredFunc &f, std::string str);

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

private:

    // Methods only for generating OpenCL code for Intel FPGAs
    void compile_to_aocx_oneapi(std::ostringstream &src_stream);

};

}  // namespace Internal
}  // namespace Halide

#endif
