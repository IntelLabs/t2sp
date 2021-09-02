#ifndef HALIDE_CODEGEN_CM_DEV_H
#define HALIDE_CODEGEN_CM_DEV_H

/** \file
 * Defines the code-generator for producing OpenCL C kernel code
 */

#include <sstream>

#include "CodeGen_C.h"
#include "CodeGen_GPU_Dev.h"
#include "Target.h"

using std::string;

namespace Halide {
namespace Internal {

class CodeGen_CM_Dev : public CodeGen_GPU_Dev {
public:
    CodeGen_CM_Dev(Target target);

    /** Compile a GPU kernel into the module. This may be called many times
     * with different kernels, which will all be accumulated into a single
     * source module shared by a given Halide pipeline. */
    void add_kernel(Stmt stmt,
                    const std::string &name,
                    const std::vector<DeviceArgument> &args) override;

    /** (Re)initialize the GPU kernel module. This is separate from compile,
     * since a GPU device module will often have many kernels compiled into it
     * for a single pipeline. */
    void init_module() override;

    std::vector<char> compile_to_src() override;

    std::string get_current_kernel_name() override;

    void dump() override;

    std::string print_gpu_name(const std::string &name) override;

    std::string api_unique_name() override {
        return "cm";
    }

protected:
    class CodeGen_CM_C : public CodeGen_C {
    public:
        CodeGen_CM_C(std::ostream &s, Target t)
            : CodeGen_C(s, t) {
        }
        void add_kernel(Stmt stmt,
                        const std::string &name,
                        const std::vector<DeviceArgument> &args);

    protected:
        using CodeGen_C::visit;
        std::string print_assignment(Type t, const std::string &rhs) override;
        std::string print_type(Type type, AppendSpaceIfNeeded append_space = DoNotAppendSpace) override;
        std::string print_reinterpret(Type type, Expr e) override;
        std::string print_extern_call(const Call *op) override;
        std::string print_array_access(const std::string &name,
                                       const Type &type,
                                       const std::string &id_index);
        std::string get_vector_init_tmpl(Type t,
                                         const string &base,
                                         const string &stride);
        std::string get_vector_assign_tmpl(Type t,
                                           const string& base,
                                           const string& stride);
        std::string get_vector_declaration(Type t, const string &id);
        std::string get_vector_read_tmpl(Type t,
                                         const string &name,
                                         const string &global_offset,
                                         const string &element_offset);
        std::string get_vector_write_tmpl(Type t,
                                          const string &name,
                                          const string &global_offset,
                                          const string &element_offset = "");
        std::string get_slm_read_tmpl(Type t,
                                      const string &name,
                                      const string &global_offset,
                                      const string &element_offset);
        std::string get_slm_write_tmpl(Type t,
                                       const string &name,
                                       const string &global_offset,
                                       const string &element_offset);
        std::string get_vector_select(const string &name,
                                      const string &size,
                                      const string &stride,
                                      const string &base);
        std::string print_vector_op(const string& tmpl, char prefix = '_');
        void add_vector_typedefs(const std::set<Type> &vector_types) override;

        std::string get_memory_space(const std::string &);

        std::string shared_name;

        void visit(const Evaluate *op) override;
        void visit(const For *) override;
        void visit(const Ramp *op) override;
        void visit(const Broadcast *op) override;
        void visit(const Call *op) override;
        void visit(const Load *op) override;
        void visit(const Store *op) override;
        void visit(const Cast *op) override;
        void visit(const Select *op) override;
#if 0
        void visit(const EQ *) override;
        void visit(const NE *) override;
        void visit(const LT *) override;
        void visit(const LE *) override;
        void visit(const GT *) override;
        void visit(const GE *) override;
#endif
        void visit(const Or *) override;
        void visit(const And *) override;
        void visit(const Allocate *op) override;
        void visit(const Free *op) override;
        void visit(const AssertStmt *op) override;
        void visit(const Shuffle *op) override;
        void visit(const Min *op) override;
        void visit(const Max *op) override;
        void visit(const Atomic *op) override;
    };

    std::ostringstream src_stream;
    std::string cur_kernel_name;
    CodeGen_CM_C clc;
};

}  // namespace Internal
}  // namespace Halide

#endif
