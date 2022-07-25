#include "CodeGen_DPC_Dev.h"
#include "IROperator.h"
#include "Simplify.h"
#include "Substitute.h"
#include "fstream"
#include <cmath>
#include <sstream>
#include <stack>
#include <string>
namespace Halide {
namespace Internal {

using std::ostringstream;
using std::stack;
using std::string;
using std::to_string;
using std::vector;
static bool is_broadcast;
stack<string> range_info_Block;
stack<string> range_info_thread;
string global_declaration;
CodeGen_DPC_Dev::CodeGen_DPC_Dev(Target t)
    : clc(src_stream, t) {
}

string CodeGen_DPC_Dev::CodeGen_DPC_C::print_type(Type type, AppendSpaceIfNeeded space) {
    ostringstream oss;
    if (type.is_float()) {
        switch (type.bits()) {
        case 16:
            oss << "half";
            break;
        case 32:
            oss << "float";
            break;
        case 64:
            oss << "double";
            break;
        default:
            user_error << "Can't represent a float with this many bits in DPC C: " << type << "\n";
        }
    } else {
        if (type.is_uint()) {
            oss << "unsigned ";
        }
        switch (type.bits()) {
        case 1:
            oss << "char";
            break;
        case 8:
            oss << "char";
            break;
        case 16:
            oss << "short";
            break;
        case 32:
            oss << "int";
            break;
        case 64:
            oss << "long long";
            break;
            break;
        default:
            user_error << "Can't represent an integer with this many bits in DPC C: " << type << "\n";
        }
    }
    if (space == AppendSpace) {
        oss << " ";
    }
    return oss.str();
}

namespace {
string simt_intrinsic(const string &name) {
    if (ends_with(name, ".__thread_id_x")) {
        return "ndi.get_local_id(0)";
    } else if (ends_with(name, ".__thread_id_y")) {
        return "ndi.get_local_id(1)";
    } else if (ends_with(name, ".__thread_id_z")) {
        return "ndi.get_local_id(2)";
    } else if (ends_with(name, ".__block_id_x")) {
        return "ndi.get_group(0)";
    } else if (ends_with(name, ".__block_id_y")) {
        return "ndi.get_group(1)";
    } else if (ends_with(name, ".__block_id_z")) {
        return "ndi.get_group(2)";
    }
    internal_error << "simt_intrinsic called on bad variable name: " << name << "\n";
    return "";
}
}  // namespace

string CodeGen_DPC_Dev::CodeGen_DPC_C::print_vector_op(const string &tmpl, char prefix) {
    auto cached = cache.find(tmpl);
    if (cached == cache.end()) {
        string gen;
        id = unique_name(prefix);
        gen = replace_all(tmpl, "$ID$", id);
        stream << gen;
        cache[tmpl] = id;
    } else {
        id = cached->second;
    }

    return id;
}

string CodeGen_DPC_Dev::CodeGen_DPC_C::get_vector_declaration(Type t,
                                                              const string &id) {
    ostringstream os_tmpl;
    os_tmpl << get_indent() << "simd<"
            << print_type(t) << ", "
            << t.lanes() << "> "
            << id << ";\n";
    return os_tmpl.str();
}

string CodeGen_DPC_Dev::CodeGen_DPC_C::get_vector_declaration(Type t,
                                                              const string &id,
                                                              const string &init) {
    ostringstream os_tmpl;
    os_tmpl << get_indent() << "simd<"
            << print_type(t) << ", "
            << t.lanes() << "> "
            << id << "("
            << init << ");\n";
    return os_tmpl.str();
}

string CodeGen_DPC_Dev::CodeGen_DPC_C::get_vector_init_tmpl(Type t,
                                                            const string &base,
                                                            const string &stride) {
    ostringstream os_tmpl;
    os_tmpl << get_vector_declaration(t, "$ID");
    for (int i = 0; i < t.lanes(); i++) {
        os_tmpl << get_indent() << "$ID$[" << to_string(i) << "]=(" << base << "+";
        while (i) {
            os_tmpl << stride << "+";
            i--;
        }
        os_tmpl << "0);\n";
    }
    return os_tmpl.str();
}
string CodeGen_DPC_Dev::CodeGen_DPC_C::get_vector_read_tmpl(Type t,
                                                            const string &name,
                                                            const string &global_offset,
                                                            const string &element_offset,
                                                            bool with_decl = false) {
    ostringstream os_tmpl;
    if (with_decl)
        os_tmpl << get_vector_declaration(t, "$ID$");
    os_tmpl << get_indent() << "$ID$ = gather<" << print_type(t) << "," <<  t.lanes() << ">(" << print_name(name) << "," << element_offset << "," << global_offset << ")";
    return os_tmpl.str();
}
string CodeGen_DPC_Dev::CodeGen_DPC_C::get_vector_read_tmpl(Type t,
                                                            const string &name,
                                                            const string &offset,
                                                            bool dword_aligned = false) {
    ostringstream os_tmpl;
    os_tmpl << get_vector_declaration(t, "$ID$");
    os_tmpl << get_indent() << "$ID$=gather<" << print_type(t) << ", " << t.lanes() << ">(" << print_name(name) << "," << offset << ");";
    return os_tmpl.str();
}

string CodeGen_DPC_Dev::CodeGen_DPC_C::get_slm_read_tmpl(Type t,
                                                         const string &name,
                                                         const string &global_offset,
                                                         const string &element_offset) {
    ostringstream os_tmpl;
    os_tmpl << get_vector_declaration(t, "$ID$");
    os_tmpl << "$ID$ = " << "slm_gather<" << print_type(t) << ", " << t.lanes() << ">(" << element_offset << ");\n";
    return os_tmpl.str();
}

string CodeGen_DPC_Dev::CodeGen_DPC_C::get_vector_write_tmpl(Type t,
                                                             const string &name,
                                                             const string &global_offset,
                                                             const string &element_offset) {
    ostringstream os_tmpl;
    os_tmpl << get_indent() << "scatter<" <<print_type(t) << ", " << t.lanes()<<">("
            << print_name(name) << ", "
            << element_offset << ", "
            << "$ID$" << ","
            << global_offset << ");\n";
    return os_tmpl.str();
}

string CodeGen_DPC_Dev::CodeGen_DPC_C::get_vector_write_tmpl(Type t,
                                                             const string &name,
                                                             const string &offset) {
    ostringstream os_tmpl;
    os_tmpl << get_indent() << "scatter<" <<print_type(t) << ", " << t.lanes()<<">("
            << print_name(name) << ", "
            << offset << ", "
            << "$ID$);\n";
    return os_tmpl.str();
}

string CodeGen_DPC_Dev::CodeGen_DPC_C::get_slm_write_tmpl(Type t,
                                                          const string &name,
                                                          const string &global_offset,
                                                          const string &element_offset) {
    ostringstream os_tmpl;
    os_tmpl << get_indent() << "slm_scatter<" << print_type(t) <<", " << t.lanes() << ">("
            << element_offset << ", "
            << "$ID$);\n";
    return os_tmpl.str();
}

string CodeGen_DPC_Dev::CodeGen_DPC_C::get_vector_select(const string &name,
                                                         const string &base,
                                                         int size,
                                                         int stride) {
    ostringstream os_tmpl;
    os_tmpl << print_name(name)
            << ".select<" << size << ", " << stride << ">"
            << "(" << base << ")";
    return os_tmpl.str();
}

string CodeGen_DPC_Dev::CodeGen_DPC_C::get_vector_iselect(const string &name,
                                                          const string &idx) {
    ostringstream os_tmpl;
    os_tmpl << print_name(name)
            << ".iselect(" << idx << ")";
    return os_tmpl.str();
}

string CodeGen_DPC_Dev::CodeGen_DPC_C::print_assignment(Type t, const std::string &rhs) {
    auto cached = cache.find(rhs);
    if (cached != cache.end())
        return cached->second;

    id = unique_name('_');
    if (t.lanes() > 1) {
        stream << get_vector_declaration(t, id);
        stream << get_indent() << id << " = " << rhs << ";\n";
    } else {
        stream << get_indent() << print_type(t, AppendSpace)
               << id << " = " << rhs << ";\n";
    }
    cache[rhs] = id;
    return id;
}

string CodeGen_DPC_Dev::CodeGen_DPC_C::get_vector_element(const string &name,
                                                          const string &index) {
    ostringstream rhs;
    rhs << print_name(name);
    rhs << "[" << index << "]";
    return rhs.str();
}

void CodeGen_DPC_Dev::CodeGen_DPC_C::visit(const Min *op) {
    internal_assert(op->a.type() == op->b.type());
    id = "min<" + print_type(op->a.type()) + ">(" + print_expr(op->a) + ", " + print_expr(op->b) + ")";
}

void CodeGen_DPC_Dev::CodeGen_DPC_C::visit(const Max *op) {
    internal_assert(op->a.type() == op->b.type());
    id = "max<" + print_type(op->a.type()) + ">(" + print_expr(op->a) + ", " + print_expr(op->b) + ")";
}

void CodeGen_DPC_Dev::CodeGen_DPC_C::visit(const Mod *op) {
    visit_binop(op->type, op->a, op->b, "%");
}

void CodeGen_DPC_Dev::CodeGen_DPC_C::visit(const Div *op) {
    visit_binop(op->type, op->a, op->b, "/");
}

void CodeGen_DPC_Dev::CodeGen_DPC_C::visit(const FloatImm *op) {
    id = to_string(op->value);
}

void CodeGen_DPC_Dev::CodeGen_DPC_C::visit_binop(Type t, Expr a, Expr b, const char *op) {
    string sa = print_expr(a);
    string sb = print_expr(b);
    id = "(" + sa + op + sb + ")";
}

void CodeGen_DPC_Dev::CodeGen_DPC_C::visit(const For *loop) {
    user_assert(loop->for_type != ForType::GPULane)
        << "The CM backend does not support the gpu_lanes() scheduling directive.";
    user_assert(loop->for_type != ForType::Parallel)
        << "Cannot use parallel loops inside CM kernel\n";

    if (is_gpu_var(loop->name)) {
        internal_assert((loop->for_type == ForType::GPUBlock) ||
                        (loop->for_type == ForType::GPUThread))
            << "kernel loop must be either gpu block or gpu thread\n";
        internal_assert(is_zero(loop->min));
        // call a function:store extent
        //  stream << "flag before reset";
        //  stream.seekp(0);
        stream << get_indent() << print_type(Int(32)) << " "
               << print_name(loop->name) << " = "
               << simt_intrinsic(loop->name) << ";\n";
        if (loop->for_type == ForType::GPUBlock) {
            range_info_Block.push(print_expr(loop->extent));
        } else if (loop->for_type == ForType::GPUThread) {
            range_info_thread.push(print_expr(loop->extent));
        }
        loop->body.accept(this);
        return;
    }
    if (loop->for_type == ForType::PragmaUnrolled)
        stream << get_indent() << "#pragma unroll\n";
    CodeGen_C::visit(loop);
}

void CodeGen_DPC_Dev::CodeGen_DPC_C::visit(const Ramp *op) {
    string base = print_expr(op->base);
    string stride = print_expr(op->stride);

    Type t = op->type.with_lanes(op->lanes);
    string tmpl = get_vector_init_tmpl(t, base, stride);
    print_vector_op(tmpl);
}

void CodeGen_DPC_Dev::CodeGen_DPC_C::visit(const Broadcast *op) {
    // Since CM supports operations on scalar and vector, just treat Broadcast node as scalar
    is_broadcast = true;
    op->value.accept(this);
}

void CodeGen_DPC_Dev::CodeGen_DPC_C::visit(const Variable *op) {
    if (op->type.is_handle()) {
        id = "_" + op->name;
        return;
    }
    CodeGen_C::visit(op);
}

void CodeGen_DPC_Dev::CodeGen_DPC_C::visit(const Evaluate *op) {
    print_expr(op->value);
}


void CodeGen_DPC_Dev::CodeGen_DPC_C::print_media_block_rw(Type t, vector<Expr> args, bool is_write) {
    int cols = args[5].as<IntImm>()->value;
    int rows = args[6].as<IntImm>()->value;
    // internal_assert(cols <= 8);
    int bytes = t.bits() / 8;
    int max_cols_at_once = (cols < 8 ? cols : 8);
    int max_rows_at_once = 256 / (max_cols_at_once * bytes);

    for (int i = 0; i < rows; i += max_rows_at_once) {
        int rows_at_once = i + max_rows_at_once <= rows ? max_rows_at_once : rows - i;
        for (int j = 0; j < cols; j += max_cols_at_once) {
            int cols_at_once = j + max_cols_at_once <= cols ? max_cols_at_once : cols - j;
            auto ramp = args[4].as<Ramp>();
            // Replace the buffer name with the one specified in stensors
            string name = is_write ? args[7].as<StringImm>()->value : print_expr(args[0]);
            if (is_write == false) {
                stream << get_indent() << print_expr(args[3]) << ".select<"
                       << ramp->lanes << ", " << ramp->stride << ">(" << ramp->base << ")"
                       << ".bit_cast_view<" << print_type(t) << ", " << rows << ", " << cols << ">()"
                       << ".select<" << rows_at_once << ", 1, " << cols_at_once << ", 1>("
                       << i << ", " << j << ")=";
                stream << "media_block_load<" << print_type(t) << ", " << rows_at_once << ", " << cols_at_once << ">(";
                stream << print_name(name) << ", ";
                stream << print_expr(args[1] * bytes) << ", ";
                stream << print_expr(args[2] + i) << ");\n";
            } else if (is_write == true) {
                stream << get_indent() << "media_block_store<" << print_type(t) << ", " << rows_at_once << ", " << cols_at_once << ">(";
                stream << print_name(name) << ", ";
                stream << print_expr(args[1] * bytes) << ", ";
                stream << print_expr(args[2] + i) << ", ";
                auto ramp = args[4].as<Ramp>();
                stream << print_expr(args[3]) << ".select<"
                       << ramp->lanes << ", " << ramp->stride << ">(" << ramp->base << ")"
                       << ".bit_cast_view<" << print_type(t) << ", " << rows << ", " << cols << ">()"
                       << ".select<" << rows_at_once << ", 1, " << cols_at_once << ", 1>("
                       << i << ", " << j << "));\n";
            }
        }
    }
}
string print_corr_buf_idx_DPC(vector<Expr> args, string id) {
    ostringstream oss;
    oss << "inline int cm_corr_buf_idx_" << id << "(int i) {\n";
    auto &cond = args[1].as<Shuffle>()->vectors;
    auto &acc = args[2].as<Shuffle>()->vectors;
    for (size_t i = 0; i < cond.size(); i++) {
        oss << "  if (i < " << cond[i] << ") return (i - " << acc[i] << ");\n";
    }
    oss << "  return i;\n}\n";
    return oss.str();
}

void CodeGen_DPC_Dev::CodeGen_DPC_C::visit(const Call *op) {
    if (op->is_intrinsic(Call::gpu_thread_barrier)) {
        stream << get_indent() << "cm_barrier();\n";
        return;
    }
    if (op->is_intrinsic(Call::cm_load_2d)) {
        print_media_block_rw(op->type, op->args, false);
        return;
    }
    if (op->is_intrinsic(Call::cm_store_2d)) {
        print_media_block_rw(op->type, op->args, true);
        return;
    }
    if (op->is_intrinsic(Call::cm_corr_buf_idx)) {
        string id = unique_name('f');
        string rhs = "cm_corr_buf_idx_" + id + "(" + print_expr(op->args[0]) + ")";
        init_funcs += print_corr_buf_idx_DPC(op->args, id);
        print_assignment(op->type, rhs);
        return;
    }
    CodeGen_C::visit(op);
}

void CodeGen_DPC_Dev::CodeGen_DPC_C::visit(const Load *op) {
    user_assert(is_one(op->predicate))
        << "Predicated load is not supported inside CM kernel.\n";

    internal_assert(allocations.contains(op->name))
        << op->name << "is not allocated\n";
    const auto &alloc = allocations.get(op->name);
    is_broadcast = false;
    if (alloc.memory_type == MemoryType::Register) {
        if (op->index.type().is_vector()) {
            auto ramp = op->index.as<Ramp>();
            auto stride = ramp->stride.as<IntImm>()->value;
            auto num_base = ramp->base.as<IntImm>();
            auto ramp_base = print_expr(ramp->base);

            if (stride < 0) {
                Type t = UInt(16).with_lanes(ramp->lanes);
                string tmpl = get_vector_init_tmpl(t, ramp_base, to_string(stride));
                print_vector_op(tmpl);
                id = get_vector_iselect(op->name, id);
                return;
            }

            if (num_base && num_base->value < 0) {
                size_t lanes = ramp->lanes + num_base->value;
                id = unique_name('_');
                stream << get_vector_declaration(op->type, id);
                string rhs = get_vector_select(op->name, "0", lanes, stride);
                string lhs = get_vector_select(id, to_string(0 - num_base->value), lanes, stride);
                stream << get_indent() << lhs << " = " << rhs << ";\n";
                return;
            }

            id = get_vector_select(op->name, ramp_base, ramp->lanes, stride);
        } else {
            string index = print_expr(op->index);
            id = get_vector_element(op->name, index);
        }
        return;
    }
    // If we're loading a contiguous ramp into a vector, use block read instead.
    bool is_continuous = strided_ramp_base(op->index).defined();
    if (is_continuous) {
        auto ramp = op->index.as<Ramp>();
        string ramp_base = print_expr(simplify(ramp->base * op->type.bits() / 8));
        if (ramp->stride.as<IntImm>()->value == -1)
            ramp_base += "-" + to_string(ramp->lanes * op->type.bits() / 8);

        if (alloc.memory_type == MemoryType::Heap) {
            string tmpl = get_vector_read_tmpl(op->type, op->name, ramp_base);
            print_vector_op(tmpl);
        }
    } else if (op->index.type().is_vector()) {
        // If index is a vector, gather vector elements.
        internal_assert(op->type.is_vector());
        Type idx_t = op->index.type().with_code(halide_type_uint);
        op->index.set_type(idx_t);
        string index = print_expr(op->index);

        if (alloc.memory_type == MemoryType::Heap) {
            if (in_buffer) {
                id = get_vector_read_tmpl(op->type, op->name, "0", index);
            } else {
                string tmpl = get_vector_read_tmpl(op->type, op->name, "0", index, true);
                print_vector_op(tmpl);
            }
        }
    } else {
        if (alloc.memory_type == MemoryType::Heap) {
            size_t bytes = op->type.bits() / 8;
            internal_assert(bytes == 4);
            Type t = op->type.with_lanes(16 / bytes);
            string index = print_expr(simplify(op->index * 4));
            string tmpl = get_vector_read_tmpl(t, op->name, index, true);
            print_vector_op(tmpl);
            id = get_vector_element(id, "0");
        }
    }
}

void CodeGen_DPC_Dev::CodeGen_DPC_C::visit(const Store *op) {
    user_assert(is_one(op->predicate))
        << "Predicated store is not supported inside CM kernel.\n";

    internal_assert(allocations.contains(op->name))
        << op->name << "is not allocated\n";
    const auto &alloc = allocations.get(op->name);
    if (ends_with(op->name, "_buf"))
        in_buffer = true;
    string value = print_expr(op->value);
    Type t = op->value.type();
    in_buffer = false;
    if (alloc.memory_type == MemoryType::Register) {
        if (op->index.type().is_vector()) {
            auto ramp = op->index.as<Ramp>();
            string ramp_base = print_expr(ramp->base);
            size_t lanes = ramp->lanes;
            size_t stride = ramp->stride.as<IntImm>()->value;
            string dest = get_vector_select(op->name, ramp_base, lanes, stride);
            if (value.find("$ID$") != value.npos)
                stream << replace_all(value, "$ID$", dest);
            else {
                if (is_broadcast == true && (value.find("_buf") != value.npos)) {
                    // stream << "here is my test,value:=" << value << '\n';
                    is_broadcast = false;
                    int buf_pos = value.find("_buf");
                    buf_pos += 4;
                    string select_stmt = ".select<" + to_string(lanes) + ",0>";
                    value.insert(buf_pos, select_stmt);
                    stream << get_indent() << dest << " = " << value << ";\n";
                } else
                    stream << get_indent() << dest << " = " << value << ";\n";
            }
        } else {
            string index = print_expr(op->index);
            stream << get_indent() << get_vector_element(op->name, index)
                   << " = " << value << ";\n";
        }
        return;
    }
    bool is_continuous = strided_ramp_base(op->index).defined() || strided_ramp_base(op->index, -1).defined();
    if (is_continuous) {
        auto ramp = op->index.as<Ramp>();
        string ramp_base = print_expr(simplify(ramp->base * t.bits() / 8));
        if (ramp->stride.as<IntImm>()->value == -1)
            ramp_base += "-" + to_string(ramp->lanes * t.bits() / 8);

        if (alloc.memory_type == MemoryType::Heap) {
            string tmpl = get_vector_write_tmpl(t, op->name, ramp_base);
            stream << replace_all(tmpl, "$ID$", value);
        }
    } else {
        Type idx_t = op->index.type().with_code(halide_type_uint);
        op->index.set_type(idx_t);
        string index = print_expr(op->index);

        if (alloc.memory_type == MemoryType::Heap) {
            string tmpl = get_vector_write_tmpl(t, op->name, "0", index);
            stream << replace_all(tmpl, "$ID$", value);
        }
    }
}

void CodeGen_DPC_Dev::CodeGen_DPC_C::visit(const Select *op) {
    ostringstream rhs;
    string cond = print_expr(op->condition);
    string true_val = print_expr(op->true_value);
    string false_val = print_expr(op->false_value);
    // trick: replace logical operation with bitwise operation
    cond = replace_all(cond, "&&", "&");
    cond = replace_all(cond, "||", "|");

    if (!op->condition.type().is_scalar()) {
        id = unique_name('_');
        stream << get_vector_declaration(op->type, id);

        stream << get_indent() << "SIMD_IF_BEGIN(" << cond << ") {\n";
        indent += 2;
        stream << get_indent() << id << " = " << op->true_value << ";\n";
        indent -= 2;
        stream << get_indent() << "} SIMD_ELSE {\n";
        indent += 2;
        stream << get_indent() << id << " = " << false_val << ";\n";
        indent -= 2;
        stream << get_indent() << "} SIMD_IF_END;\n";
        return;
    }

    rhs << "(" << cond
        << " ? " << true_val
        << " : " << false_val
        << ")";
    id = rhs.str();
}

void CodeGen_DPC_Dev::CodeGen_DPC_C::visit(const Allocate *op) {
    user_assert(!op->new_expr.defined()) << "Allocate node inside CM kernel has custom new expression.\n"
                                         << "(Memoization is not supported inside GPU kernels at present.)\n";

    if (op->memory_type == MemoryType::GPUShared) {
        Allocation alloc;
        alloc.type = op->type;
        alloc.memory_type = op->memory_type;
        allocations.push(op->name, alloc);
        open_scope();

        int32_t size = op->constant_allocation_size();
        internal_assert(size % 4 == 0);
        stream << get_indent() << "cm_slm_init(" << size << ");\n";
        stream << get_indent() << "unsigned int " << print_name(op->name)
               << " = cm_slm_alloc(" << size << ");\n";
        op->body.accept(this);

        close_scope("alloc " + print_name(op->name));
        // Should have been freed internally
        internal_assert(!allocations.contains(op->name));
    } else {
        int32_t size = op->constant_allocation_size();
        user_assert(size > 0)
            << "Allocation " << op->name << " has a dynamic size. "
            << "Only fixed-size allocations are supported on the gpu. "
            << "Try storing into shared memory instead.";
        internal_assert(size % 4 == 0);

        Allocation alloc;
        alloc.type = op->type;
        alloc.memory_type = MemoryType::Register;
        allocations.push(op->name, alloc);

        stream << get_vector_declaration(op->type.with_lanes(size), print_name(op->name));
        op->body.accept(this);

        internal_assert(!allocations.contains(op->name));
    }
}

void CodeGen_DPC_Dev::CodeGen_DPC_C::visit(const Free *op) {
    // Should have been freed internally
    internal_assert(allocations.contains(op->name));
    allocations.pop(op->name);
}

void CodeGen_DPC_Dev::CodeGen_DPC_C::visit(const AssertStmt *op) {
    user_warning << "Ignoring assertion inside CM kernel: " << op->condition << "\n";
}

void CodeGen_DPC_Dev::CodeGen_DPC_C::visit(const LetStmt *op) {
    Stmt body = op->body;
    Expr exp_val = op->value;

    if (starts_with(op->name, "val.")) {
        string value = print_expr(exp_val);
        Expr new_var = Variable::make(exp_val.type(), value);
        body = substitute(op->name, new_var, body);
    }
    if (starts_with(op->name, "var.") || starts_with(op->name, "ref.")) {
        string value = print_expr(exp_val);
        string id = unique_name('_');
        stream << get_indent() << print_type(exp_val.type(), AppendSpace)
               << id << " = " << value << ";\n";
        Expr new_var = Variable::make(op->value.type(), id);
        body = substitute(op->name, new_var, body);
    }
    body.accept(this);
}

void CodeGen_DPC_Dev::CodeGen_DPC_C::visit(const Shuffle *op) {

    if (op->is_concat()) {
        size_t op_lanes = op->type.lanes();
        std::stringstream vec;
        string vec_id = unique_name("init_v");
        vec << "const int " << vec_id << "[" << op_lanes << "] = {";
        for (size_t i = 0; i < op_lanes; i++) {
            auto item = op->vectors[i].as<IntImm>();
            user_assert(item)
                << "We only support const data item\n";
            vec << item->value << ", ";
        }
        vec << "};\n";
        init_vecs += vec.str();

        id = unique_name('_');
        Type t = op->type.with_code(halide_type_uint);
        // vector<int,6>A(init_v) index
        // init_v = {6,5,4,3,2,1}
        // simd<int,size_2>init_v
        // simd<int,size>A{init_v}
        stream << get_vector_declaration(t, id, vec_id);
        return;
    }
    if (op->is_slice()) {
        string vec_id = print_expr(op->vectors[0]);
        string base = to_string(op->slice_begin());
        id = get_vector_select(vec_id, base, op->indices.size(), op->slice_stride());
        return;
    }
    CodeGen_C::visit(op);
}

class FindRefName : public IRVisitor {
    const string &buf_name;

public:
    using IRVisitor::visit;
    string ref_name;

    void visit(const Call *op) override {
        if (op->is_intrinsic(Call::cm_store_2d)) {
            internal_assert(op->args[0].as<Variable>());
            auto &name = op->args[0].as<Variable>()->name;
            if (name == buf_name && op->args.size() == 8) {
                internal_assert(op->args[7].as<StringImm>());
                ref_name = op->args[7].as<StringImm>()->value;
            }
        }
    }

    FindRefName(const string &_b)
        : buf_name(_b) {
    }
};

void CodeGen_DPC_Dev::add_kernel(Stmt s,
                                 const string &name,
                                 const vector<DeviceArgument> &args) {
    debug(2) << "CodeGen_DPC_Dev::compile " << name << "\n";

    // TODO: do we have to uniquify these names, or can we trust that they are safe?
    cur_kernel_name = name;
    clc.add_kernel(s, name, args);
    global_declaration = clc.init_vecs + clc.init_funcs;
}

void CodeGen_DPC_Dev::CodeGen_DPC_C::add_kernel(Stmt s,
                                                const string &name,
                                                const vector<DeviceArgument> &args) {
    debug(2) << "Adding oneapi_gpu kernel " << name << "\n";
    stream << get_indent() << "auto e = q.submit([&](handler &cgh) {\n";
    indent += 2;
    for (size_t i = 0; i < args.size(); i++) {
        if (args[i].is_buffer) {
            string name = args[i].name;
            // Trick: replace the buffer name with the one specified in stensor
            FindRefName frn(name);
            s.accept(&frn);
            if (!frn.ref_name.empty()) {
                name = frn.ref_name;
            }
            stream << get_indent() << "auto " << print_name(name) << "=img" << print_name(name) << ".get_access<uint4,access::mode::" << (args[i].read ? "read" : "write") << ">(cgh)";
            Allocation alloc;
            alloc.type = args[i].type;
            alloc.memory_type = MemoryType::Heap;
            allocations.push(args[i].name, alloc);
            if (i < args.size()) {
                stream << ";\n";
            }
        }
    }
    stream << get_indent() << "cgh.parallel_for<class Test>(\n";
    indent += 2;
    stream << get_indent() << "nd_range{GlobalRange, LocalRange},\n";
    stream << get_indent() << "[=](nd_item<nd_item_dimension> ndi) SYCL_ESIMD_KERNEL {\n";
    indent += 2;
    stream << get_indent() << "using namespace sycl::ext::intel::esimd;\n";
    // stream << ")\n";

    // open_scope();
    print(s);
    close_scope("kernel " + name);
    indent -= 2;
    // assert(indent != 0);
    stream << get_indent() << ");\n";
    indent -= 2;
    // assert(indent != 0);
    stream << get_indent() << "});\n";
    // indent -= 2;
    // stream << get_indent() << "}\n";
    for (size_t i = 0; i < args.size(); i++) {
        // Remove buffer arguments from allocation scope
        if (args[i].is_buffer) {
            allocations.pop(args[i].name);
        }
    }
    // auto end_pos = stream.tellp();
}

void CodeGen_DPC_Dev::CodeGen_DPC_C::open_scope() {
    stream << get_indent();
    indent += 2;
    stream << "{\n";
}

void CodeGen_DPC_Dev::CodeGen_DPC_C::close_scope(const std::string &comment) {
    indent -= 2;
    stream << get_indent();
    if (!comment.empty()) {
        stream << "} // " << comment << "\n";
    } else {
        stream << "}\n";
    }
}

void CodeGen_DPC_Dev::init_module() {
    debug(2) << "oneapi_gpu device codegen init_module\n";

    // wipe the internal kernel source
    src_stream.str("");
    src_stream.clear();
    cur_kernel_name = "";
}

vector<char> CodeGen_DPC_Dev::compile_to_src() {
    string str = src_stream.str();
    debug(1) << "DPC kernel:\n"
             << str << "\n";
    int block_range_dim = range_info_Block.size();
    int thread_range_dim = range_info_thread.size();
    int nd_item_dim = thread_range_dim > block_range_dim ? thread_range_dim : block_range_dim;
    int range_delta = block_range_dim - thread_range_dim;
    string glb_range = "range<" + to_string(nd_item_dim) + "> GlobalRange(";
    string lcl_range = "range<" + to_string(nd_item_dim) + "> LocalRange(";
    //thread range size > block range size
    if (range_delta < 0) {
        while (!range_info_Block.empty()) {
            glb_range += range_info_Block.top() + "*" + range_info_thread.top() + ",";
            lcl_range += range_info_thread.top() + ",";
            range_info_Block.pop();
            range_info_thread.pop();
        }
        while (!range_info_thread.empty())
        {
            glb_range += to_string(1) + "*" + range_info_thread.top() + ",";
            lcl_range += range_info_thread.top() + ",";
            range_info_thread.pop();
        }
    }
    else if (range_delta == 0) {
    while (!range_info_Block.empty()) {
        glb_range += range_info_Block.top() + "*" + range_info_thread.top() + ",";
        lcl_range += range_info_thread.top() + ",";
        range_info_Block.pop();
        range_info_thread.pop();
    }
    }
    else if (range_delta > 0) {
        while (!range_info_thread.empty()) {
            glb_range += range_info_Block.top() + "*" + range_info_thread.top() + ",";
            lcl_range += range_info_thread.top() + ",";
            range_info_Block.pop();
            range_info_thread.pop();
        }
        while (!range_info_Block.empty())
        {
            glb_range += range_info_Block.top() + "*" + to_string(1) + ",";
            lcl_range += to_string(1) + ",";
            range_info_Block.pop();
        }
    }
    glb_range.pop_back();
    glb_range += ");\n";
    lcl_range.pop_back();
    lcl_range += ");\n";
    string range_str = "#ifdef GPU\n";
    range_str += "queue q(esimd_test::ESIMDSelector{}, esimd_test::createExceptionHandler(),property::queue::enable_profiling{});\n" + glb_range + lcl_range;
    range_str += "const int nd_item_dimension =" + to_string(nd_item_dim) + ";\n";
    range_str += str;
    range_str += "#endif\n";
    std::ifstream reading_sbl("oneapi.gpu.run.cpp");
    if (!reading_sbl) {
        user_error << "An error occurred while opening the file. Please check whether you have created this file!";
    }
    string total_buff_read;
    string buff_read;
    while (getline(reading_sbl, buff_read)) {
        if ((buff_read.find("Halide") != std::string::npos) || (buff_read.find("util.h") != std::string::npos)) {
            total_buff_read += "#ifndef GPU\n";
        }
        total_buff_read += buff_read;
        total_buff_read += '\n';
        if ((buff_read.find("Halide") != std::string::npos) || (buff_read.find("util.h") != std::string::npos)) {
            total_buff_read += "#endif\n";
        }
    }
    total_buff_read.insert(total_buff_read.size() - 2, range_str);
    total_buff_read = global_declaration + total_buff_read;
    debug(1) << "this is the file we will generate" << total_buff_read;
    vector<char> buffer(total_buff_read.begin(), total_buff_read.end());
    return buffer;
}

}  // namespace Internal
}  // namespace Halide
