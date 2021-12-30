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
                    const string &name,
                    const std::vector<DeviceArgument> &args) override;

    /** (Re)initialize the GPU kernel module. This is separate from compile,
     * since a GPU device module will often have many kernels compiled into it
     * for a single pipeline. */
    void init_module() override;

    std::vector<char> compile_to_src() override;

    string get_current_kernel_name() override {
        return cur_kernel_name;
    }

    void dump() override {
        std::cerr << src_stream.str() << "\n";
    }

    string print_gpu_name(const string &name) override {
        return name;
    }

    string api_unique_name() override {
        return "cm";
    }

protected:
    class CodeGen_CM_C : public CodeGen_C {
    public:
        string init_vecs;
        string init_funcs;

        CodeGen_CM_C(std::ostream &s, Target t)
            : CodeGen_C(s, t) {
        }
        void add_kernel(Stmt stmt,
                        const string &name,
                        const std::vector<DeviceArgument> &args);
    private:
        size_t trick_size = 0;
        bool in_buffer = false;
        string get_vector_element(const string &name,
                                  const string &id_index);
        string get_vector_init_tmpl(Type t,
                                    const string &base,
                                    const string &stride);
        string get_vector_declaration(Type t,
                                      const string &id);
        string get_vector_declaration(Type t,
                                      const string &id,
                                      const string &init);
        string get_vector_read_tmpl(Type t,
                                    const string &name,
                                    const string &global_offset,
                                    const string &element_offset,
                                    bool with_decl);
        string get_vector_read_tmpl(Type t,
                                    const string &name,
                                    const string &offset,
                                    bool dword_aligned);
        string get_vector_write_tmpl(Type t,
                                    const string &name,
                                    const string &global_offset,
                                    const string &element_offset);
        string get_vector_write_tmpl(Type t,
                                     const string &name,
                                     const string &offset);
        string get_slm_read_tmpl(Type t,
                                 const string &name,
                                 const string &global_offset,
                                 const string &element_offset);
        string get_slm_write_tmpl(Type t,
                                  const string &name,
                                  const string &global_offset,
                                  const string &element_offset);
        string get_vector_select(const string &name,
                                 const string &base,
                                 int size,
                                 int stride);
        string get_vector_iselect(const string &name,
                                  const string &idx);
        string print_vector_op(const string& tmpl,
                               char prefix = '_');
        void print_media_block_rw(Type t, std::vector<Expr> args, bool is_write);

    protected:
        using CodeGen_C::visit;
        string print_assignment(Type t, const std::string &rhs) override;
        string print_type(Type type, AppendSpaceIfNeeded append_space = DoNotAppendSpace) override;

        void visit(const For *) override;
        void visit(const Ramp *op) override;
        void visit(const Broadcast *op) override;
        void visit(const Min *op) override;
        void visit(const Max *op) override;
        void visit(const Mod *op) override;
        void visit(const Div *op) override;
        void visit(const Call *op) override;
        void visit(const Load *op) override;
        void visit(const Store *op) override;
        void visit(const Select *op) override;
        void visit(const LetStmt *op) override;
        void visit(const FloatImm *op) override;
        void visit(const Allocate *op) override;
        void visit(const Free *op) override;
        void visit(const AssertStmt *op) override;
        void visit(const Shuffle *op) override;
        void visit(const Variable *op) override;
        void visit(const Evaluate *op) override;
        void visit_binop(Type t, Expr a, Expr b, const char *op) override;

        void open_scope() override;
        void close_scope(const std::string &comment) override;
    };

    std::ostringstream src_stream;
    string cur_kernel_name;
    CodeGen_CM_C clc;
};

}  // namespace Internal
}  // namespace Halide

#endif
