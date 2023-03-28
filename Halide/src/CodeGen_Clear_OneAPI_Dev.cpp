#include <algorithm>
#include <sstream>
#include <fstream>

#include "CSE.h"
#include "CodeGen_Internal.h"
#include "CodeGen_Clear_OneAPI_Dev.h"
#include "Debug.h"
#include "EliminateBoolVectors.h"
#include "EmulateFloat16Math.h"
#include "ExprUsesVar.h"
#include "IRMutator.h"
#include "IROperator.h"
#include "Simplify.h"
#include "Substitute.h"
#include "../../t2s/src/DebugPrint.h"
#include "../../t2s/src/Utilities.h"

namespace Halide {
namespace Internal {

using std::ostringstream;
using std::sort;
using std::string;
using std::vector;

CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_Dev(Target t)
    : clc(src_stream, t) {
}

string CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::generate_intermediate_var(const string &old_val) {
    // Expressions that are too long are likely to be complicated to calculate
    // So we generate intermediate variables for them.
    if (needs_intermediate_expr || old_val.size() >= 32) {
        auto name = "_" + unique_name('D');
        stream << get_indent() << "auto " << name << " = " << old_val << ";\n";
        needs_intermediate_expr = false;
        return name;
    }
    return old_val;
}

string CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::vector_index_to_string(int idx) {
    std::ostringstream oss;
    if (idx >= 10 && idx < 16) {
        char c = idx - 10 + 'a';
        oss << string((const char *)&c, 1);
    } else {
        oss << idx;
    }
    return std::move(oss.str());
}

bool CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::is_standard_opencl_type(Type type) {
    int bits = type.bits();
    int lanes = type.lanes();
    bool standard_bits = false;
    bool standard_lanes = false;
    if (type.is_float()) {
        if ((bits == 16 && target.has_feature(Target::CLHalf)) ||
            (bits == 32) || (bits == 64)) {
            standard_bits = true;
        }
    } else {
        if ((bits == 1 && lanes == 1) || (bits == 8) ||
            (bits == 16) || (bits == 32) || (bits == 64)) {
            standard_bits = true;
        }
    }
    if ((lanes == 1) || (lanes == 2) || (lanes == 3) ||(lanes == 4) ||
        (lanes == 8) || (lanes == 16)) {
        standard_lanes = true;
    }
    return standard_bits && standard_lanes;
}

string CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::print_type(Type type, AppendSpaceIfNeeded space) {
    ostringstream oss;
    if (type.is_generated_struct()) {
        int type_id = type.bits();
        const std::pair<string, vector<Type>> &entry = GeneratedStructType::structs[type_id];
        string struct_name = entry.first;
        oss << struct_name;
    } else if (!is_standard_opencl_type(type)) {
        Type basic_type = type.with_lanes(1);
        string type_name = print_type(basic_type);
        oss << type_name;
    } else if (type.is_float()) {
        if (type.bits() == 16) {
            user_assert(target.has_feature(Target::CLHalf))
                << "OpenCL kernel uses half type, but CLHalf target flag not enabled\n";
            oss << "half";
        } else if (type.bits() == 32) {
            oss << "float";
        } else if (type.bits() == 64) {
            oss << "double";
        } else {
            user_error << "Can't represent a float with this many bits in OpenCL C: " << type << "\n";
        }

    } else {
        if (type.is_uint() && type.bits() > 1) oss << 'u';
        switch (type.bits()) {
        case 1:
            internal_assert(type.lanes() == 1) << "Encountered vector of bool\n";
            oss << "bool";
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
            oss << "long";
            break;
        default:
            user_error << "Can't represent an integer with this many bits in OpenCL C: " << type << "\n";
        }
    }
    if (type.lanes() != 1) {
        switch (type.lanes()) {
        case 2:
        case 3:
        case 4:
        case 8:
        case 16:
            oss << type.lanes();
            break;
        default:
            if (GeneratedStructType::nonstandard_vector_type_exists(type)) {
                oss << type.lanes();
                break;
            } else {
                user_error << "Unsupported vector width in OpenCL C: " << type << "\n";
            }
        }
    }
    if (space == AppendSpace) {
        oss << ' ';
    }
    return oss.str();
}

// These are built-in types in OpenCL
void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::add_vector_typedefs(const std::set<Type> &vector_types) {
}

Expr CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::DefineVectorStructTypes::mutate(const Expr &op) {
    Type type = op.type();

    if (type.lanes() > 1 && !parent->is_standard_opencl_type(type)) {
        // This is a nonstandard vector type
        if (!GeneratedStructType::nonstandard_vector_type_exists(type)) {
            GeneratedStructType::record_nonstandard_vector_type(type);
            // Define a vector type like this:
            //   typedef union {
            //      float __attribute__ ((aligned(4*17 rounded up to power of 2))) s[17];
            //      struct {float s0, s1, s2, s3, s4, s5, s6,s7, s8, s9, sa, sb, sc, sd, se, sf, s16;};
            //   } float17;
            // The first 16 elements follow the standard OpenCL vector notation.
             std::ostringstream oss;
             oss << "typedef union {\n"
                 << parent->print_type(type.element_of())
                 << " __attribute__ ((aligned(" << closest_power_of_two(type.lanes() * type.with_lanes(1).bytes())
                 << ")))" << " s[" << type.lanes() << "];\n"
                 << "struct {" << parent->print_type(type.element_of());
             for (int i = 0; i < type.lanes(); i++) {
                 oss << (i == 0 ? "" : ", ") << " s" << parent->vector_index_to_string(i);
             }
             oss << ";};\n"
                 << "} " << parent->print_type(type.element_of()) << type.lanes() << ";\n";
             vectors += oss.str();
        }
    } else if (type.is_generated_struct()) {
        std::ostringstream oss;
        int type_id = type.bits();
        if (std::find(parent->defined_struct_ids.begin(), parent->defined_struct_ids.end(), type_id) == parent->defined_struct_ids.end()) {
            parent->defined_struct_ids.push_back(type_id);
            const std::pair<string, vector<Type>> &entry = GeneratedStructType::structs[type_id];
            // Define a struct like: struct foo {int f0, char f1, int f2};
            string struct_name = entry.first;
            oss << "typedef struct {\n";
            for (size_t i = 0; i < entry.second.size(); i++) {
                oss << "\t" << parent->print_type(entry.second[i]) << " f" << i << ";\n";
            }
            oss << "} " << struct_name << ";\n";
            structs += oss.str();
        }
    }

    return IRMutator::mutate(op);
}

Stmt CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::DefineVectorStructTypes::mutate(const Stmt &op) {
    return IRMutator::mutate(op);
}

string CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::print_reinterpret(Type type, Expr e) {
    ostringstream oss;
    oss << "as_" << print_type(type) << "(" << print_expr(e) << ")";
    return oss.str();
}

namespace {
string simt_intrinsic(const string &name) {
    if (ends_with(name, ".__thread_id_x")) {
        return "get_local_id(0)";
    } else if (ends_with(name, ".__thread_id_y")) {
        return "get_local_id(1)";
    } else if (ends_with(name, ".__thread_id_z")) {
        return "get_local_id(2)";
    } else if (ends_with(name, ".__thread_id_w")) {
        return "get_local_id(3)";
    } else if (ends_with(name, ".__block_id_x")) {
        return "get_group_id(0)";
    } else if (ends_with(name, ".__block_id_y")) {
        return "get_group_id(1)";
    } else if (ends_with(name, ".__block_id_z")) {
        return "get_group_id(2)";
    } else if (ends_with(name, ".__block_id_w")) {
        return "get_group_id(3)";
    }
    internal_error << "simt_intrinsic called on bad variable name: " << name << "\n";
    return "";
}
}  // namespace

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::visit(const For *loop) {
    if (!ends_with(loop->name, ".run_on_device")) {
        string stripped = extract_after_tokens(loop->name, 2); // Remove function and stage prefix
        map_verbose_to_succinct_locally(loop->name, CodeGen_Clear_C::print_name(stripped));
    } // Otherwise, the loop is dummy

    if (is_gpu_var(loop->name)) {
        internal_assert((loop->for_type == ForType::GPUBlock) ||
                        (loop->for_type == ForType::GPUThread))
            << "kernel loop must be either gpu block or gpu thread\n";
        internal_assert(is_zero(loop->min));

        stream << get_indent() << print_type(Int(32)) << " " << print_name(loop->name)
               << " = " << simt_intrinsic(loop->name) << ";\n";

        loop->body.accept(this);

    } else if (ends_with(loop->name, ".run_on_device")) {
        // The loop just tells the compiler to generate an OpenCL kernel for
        // the loop body.
        string name = extract_first_token(loop->name);
        if (shift_regs_allocates.find(name) != shift_regs_allocates.end()) {
            for (size_t i = 0; i < shift_regs_allocates[name].size(); i++) {
                stream << get_indent() << shift_regs_allocates[name][i];
            }
        }
        name += "_temp";
        if (shift_regs_allocates.find(name) != shift_regs_allocates.end()) {
            for (size_t i = 0; i < shift_regs_allocates[name].size(); i++) {
                stream << get_indent() << shift_regs_allocates[name][i];
            }
        }
        loop->body.accept(this);
    } else if (ends_with(loop->name, ".infinite")) {
        stream << get_indent() << "while(1) ";
        open_scope();
        loop->body.accept(this);
        close_scope("while "+print_name(loop->name));
    } else if (ends_with(loop->name, "remove")) {
        loop->body.accept(this);
    } else if (loop->for_type == ForType::DelayUnroll) {
        Expr extent = simplify(loop->extent);
        Stmt body = loop->body;
        const IntImm *e = extent.as<IntImm>();
        user_assert(e)
                << "Can only unroll for loops over a constant extent.\n"
                << "Loop over " << loop->name << " has extent " << extent << ".\n";
        if (e->value == 1) {
            user_warning << "Warning: Unrolling a for loop of extent 1: " << loop->name << "\n";
        }

        Stmt iters;
        for (int i = 0; i < e->value; i++) {
            Stmt iter = simplify(substitute(loop->name, loop->min + i, body));
            iter.accept(this);
        }
    } else if (loop->for_type == ForType::PragmaUnrolled) {
        stream << get_indent() << "#pragma unroll\n";
        CodeGen_Clear_C::visit(loop);
    } else {
        /*
        If not explicitly define a env variable(DELAYUNROLL or PRAGMAUNROLL), the unrolling strategy will be automatically
        decided by the compiler.
        To physically unroll a loop, we require that:
            1. there is a conditional execution of channel read/write inside the current loop, e.g.
                unrolled k = 0 to K
                    if (cond)
                        read/write channel
            2. there is a irregular loop inside the current loop and the irregular bound depends on current loop var, e.g.
                unrolled k = 0 to K
                    unrolled j = k to J
            3. there is a shift register whose bounds depends on current loop var, e.g.,
                float _Z_shreg_0, _Z_shreg_1, _Z_shreg_2, _Z_shreg_3;
                unrolled j = 0 to J
                    access _Z_shreg_j   // j needs to be replaced with 0, 1, 2, 3

        For other cases, we simply insert the #pragma unroll directive before a loop. The offline compiler attempts to fully
        unroll the loop.
        */
        user_assert(loop->for_type != ForType::Parallel) << "Cannot use parallel loops inside OpenCL kernel\n";
        if (loop->for_type == ForType::Unrolled) {
            CheckConditionalChannelAccess checker(this, loop->name);
            loop->body.accept(&checker);

            // Check the condition 1 and 2
            bool needs_unrolling = checker.conditional_access || checker.irregular_loop_dep;
            // Check the condition 3
            for (auto &kv : space_vars) {
                for (auto &v : kv.second) {
                    if (v.as<Variable>()->name == loop->name)
                        needs_unrolling |= true;
                }
            }
            if (needs_unrolling) {
                Expr extent = simplify(loop->extent);
                Stmt body = loop->body;
                const IntImm *e = extent.as<IntImm>();
                user_assert(e)
                        << "Can only unroll for loops over a constant extent.\n"
                        << "Loop over " << loop->name << " has extent " << extent << ".\n";
                if (e->value == 1) {
                    user_warning << "Warning: Unrolling a for loop of extent 1: " << loop->name << "\n";
                }

                Stmt iters;
                for (int i = 0; i < e->value; i++) {
                    Stmt iter = simplify(substitute(loop->name, simplify(loop->min + i), body));
                    iter.accept(this);
                }
            } else {
                stream << get_indent() << "#pragma unroll\n";
                CodeGen_Clear_C::visit(loop);
            }
        } else {
            CodeGen_Clear_C::visit(loop);
        }
    }

    // The loop scope ends and its name is dead
    kernel_name_map.erase(loop->name);
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::CheckConditionalChannelAccess::visit(const IfThenElse *op) {
    bool old_cond = in_if_then_else;
    in_if_then_else = true;
    op->then_case.accept(this);

    if (op->else_case.defined()) {
        in_if_then_else = true;
        op->else_case.accept(this);
    }
    in_if_then_else = old_cond;
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::CheckConditionalChannelAccess::visit(const Call *op) {
    if (op->is_intrinsic(Call::read_channel) || op->is_intrinsic(Call::write_channel) ||
        op->is_intrinsic(Call::read_channel_nb) || op->is_intrinsic(Call::write_channel_nb)) {
        conditional_access |= in_if_then_else;
    }
    IRVisitor::visit(op);
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::CheckConditionalChannelAccess::visit(const For *op) {
    if (op->for_type == ForType::Serial || op->for_type == ForType::Unrolled) {
        if (!is_const(op->min)) {
            irregular_loop_dep = true;
            debug(4) << "Loop min: " << op->min << "\n";
            debug(4) << "Physically unroll loop " << current_loop_name << " because of the irregular loop "
                     << op->name << " inside. \n";
        } else if (!is_const(op->extent)) {
            irregular_loop_dep = true;
            debug(4) << "Loop extent: " << op->extent << "\n";
            debug(4) << "Physically unroll loop " << current_loop_name << " because of the irregular loop "
                     << op->name << " inside. \n";
        }
    }
    IRVisitor::visit(op);
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::visit(const Ramp *op) {
    string id_base = print_expr(op->base);
    string id_stride = print_expr(op->stride);

    ostringstream rhs;
    if (!is_standard_opencl_type(op->type)) {
        rhs << print_type(op->type.with_lanes(op->lanes)) << "{{" << id_base;
        for (int i = 1; i < op->lanes; ++i) {
            rhs << ", (" << id_base << " + " << id_stride << " * " << i << ")";
        }
        rhs << "}}";
    } else {
        rhs << id_base << " + " << id_stride << " * "
            << print_type(op->type.with_lanes(op->lanes)) << "{0";
        // Note 0 written above.
        for (int i = 1; i < op->lanes; ++i) {
            rhs << ", " << i;
        }
        rhs << "}";
    }
    needs_intermediate_expr = true;
    set_latest_expr(op->type.with_lanes(op->lanes), rhs.str());
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::visit(const Broadcast *op) {
    string id_value = print_expr(op->value);
    if (is_standard_opencl_type(op->type)) {
        id_value = print_type(op->type.with_lanes(op->lanes)) + "{" + id_value + "}";
        set_latest_expr(op->type.with_lanes(op->lanes), id_value);
    } else {
        string s = "(" + print_type(op->type) + "){{";
        for (int i = 0; i < op->lanes; i++) {
            s += ((i == 0) ? "" : ", ") + id_value;
        }
        s += "}}";
        set_latest_expr(op->type.with_lanes(op->lanes), s);
    }
    needs_intermediate_expr = true;
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::visit(const Let *op) {
    map_verbose_to_succinct_locally(op->name, CodeGen_Clear_C::print_name(op->name));
    CodeGen_Clear_C::visit(op);
    kernel_name_map.erase(op->name);
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::visit(const LetStmt *op) {
    map_verbose_to_succinct_locally(op->name, CodeGen_Clear_C::print_name(op->name));
    CodeGen_Clear_C::visit(op);
    kernel_name_map.erase(op->name);
}


void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::visit(const Call *op) {
    if (op->is_intrinsic(Call::bool_to_mask)) {
        if (op->args[0].type().is_vector()) {
            // The argument is already a mask of the right width. Just
            // sign-extend to the expected type.
            op->args[0].accept(this);
        } else {
            // The argument is a scalar bool. Casting it to an int
            // produces zero or one. Convert it to -1 of the requested
            // type.
            Expr equiv = -Cast::make(op->type, op->args[0]);
            equiv.accept(this);
        }
    } else if (op->is_intrinsic(Call::cast_mask)) {
        // Sign-extension is fine
        Expr equiv = Cast::make(op->type, op->args[0]);
        equiv.accept(this);
    } else if (op->is_intrinsic(Call::select_mask)) {
        internal_assert(op->args.size() == 3);
        string cond = print_expr(op->args[0]);
        string true_val = print_expr(op->args[1]);
        string false_val = print_expr(op->args[2]);

        // Yes, you read this right. OpenCL's select function is declared
        // 'select(false_case, true_case, condition)'.
        ostringstream rhs;
        rhs << "select(" << false_val << ", " << true_val << ", " << cond << ")";
        set_latest_expr(op->type, rhs.str());
    } else if (op->is_intrinsic(Call::abs)) {
        if (op->type.is_float()) {
            ostringstream rhs;
            rhs << "abs_f" << op->type.bits() << "(" << print_expr(op->args[0]) << ")";
            set_latest_expr(op->type, rhs.str());
        } else {
            ostringstream rhs;
            rhs << "abs(" << print_expr(op->args[0]) << ")";
            set_latest_expr(op->type, rhs.str());
        }
    } else if (op->is_intrinsic(Call::absd)) {
        ostringstream rhs;
        rhs << "abs_diff(" << print_expr(op->args[0]) << ", " << print_expr(op->args[1]) << ")";
        set_latest_expr(op->type, rhs.str());
    } else if (op->is_intrinsic(Call::gpu_thread_barrier)) {
        stream << get_indent() << "barrier(CLK_LOCAL_MEM_FENCE);\n";
        set_latest_expr(op->type, "0");
    } else if (op->is_intrinsic(Call::shift_left) || op->is_intrinsic(Call::shift_right)) {
        CodeGen_Clear_C::visit(op);
    } else if (op->is_intrinsic(Call::read_channel)) {
        string string_pipe_index{};
        const StringImm *v = op->args[0].as<StringImm>();
        for (unsigned i = 1; i < op->args.size(); i++) {
            Expr e = op->args[i];
            assert(e.type() == Int(32));
            string sindex = print_expr(e);
            assert(!sindex.empty());
            string_pipe_index += sindex + ",";
        }
        if (string_pipe_index.back() == ',') string_pipe_index.pop_back();
        latest_expr = '_' + unique_name('_');
        int size = v->value.rfind(".");
        string channel_name = v->value;
        debug(4) << "channel name: " << channel_name << "\n";
        if (size < (int)(v->value.size()) - 1) {
            string suffix = (v->value).substr(size + 1, (int)v->value.size() - size - 1);
            // eliminate useless suffix
            while (suffix != "channel") {
                channel_name = channel_name.substr(0, size);
                size = channel_name.rfind(".");
                if (size < (int)(channel_name.size()) - 1) {
                    suffix = channel_name.substr(size + 1, (int)channel_name.size() - size - 1);
                } else {
                    break;
                }
            }
            if (suffix != "channel") {
                channel_name = v->value;
            }
        }
        debug(4) << "modified channel name: " << channel_name << "\n";
        string type = print_type(op->type);
        if (op->type.is_handle() && !op->type.is_generated_struct()) {
            type = print_name(channel_name + ".array.t");
        }
        latest_expr = print_name(channel_name) + "::read<" + string_pipe_index + ">()";
        needs_intermediate_expr = true;
    } else if (op->is_intrinsic(Call::read_channel_nb)) {
        string string_channel_index;
        const StringImm *v = op->args[0].as<StringImm>();
        const StringImm *read_success = op->args[1].as<StringImm>();
        for (unsigned i = 2; i < op->args.size(); i++) {
            Expr e = op->args[i];
            assert(e.type() == Int(32));
            string sindex = print_expr(e);
            assert(!sindex.empty());
            string_channel_index += "["+sindex+"]";
        }
        latest_expr = '_' + unique_name('_');
        int size = (v->value).rfind(".");
        string channel_name = v->value;
        debug(4) << "channel name: " << channel_name << "\n";
        if (size < (int)(v->value.size())-1) {
            string suffix = (v->value).substr(size + 1, (int)v->value.size() - size - 1);
            // eliminate useless suffix
            while (suffix != "channel") {
                channel_name = (channel_name).substr(0, size);
                size = (channel_name).rfind(".");
                if (size < (int)(channel_name.size())-1) {
                    suffix = (channel_name).substr(size + 1, (int)channel_name.size() - size - 1);
                } else {
                    break;
                }
            }
            if (suffix != "channel") {
                channel_name = v->value;
            }
        }
        debug(4) << "modified channel name: " << channel_name << "\n";
        // if ((int)suffix.size() > 0 && suffix[0] >= '0' && suffix[0] <= '9') {
        //     channel_name = (v->value).substr(0, size);
        // }
        stream << get_indent() << print_type(op->type) << " " << latest_expr << " = read_channel_nb_intel("
                << print_name(channel_name) << string_channel_index << ", &" << print_name(read_success->value) << ");\n";
    } else if (op->is_intrinsic(Call::write_channel)) {
        const StringImm *v = op->args[0].as<StringImm>();
        int size = v->value.rfind(".");
        string channel_name = v->value;
        debug(4) << "channel name: " << channel_name << "\n";
        if (size < (int)(v->value.size()) - 1) {
            string suffix = v->value.substr(size + 1, (int)v->value.size() - size - 1);
            // eliminate useless suffix
            while (suffix != "channel") {
                channel_name = channel_name.substr(0, size);
                size = channel_name.rfind(".");
                if (size < (int)(channel_name.size()) - 1) {
                    suffix = channel_name.substr(size + 1, (int)channel_name.size() - size - 1);
                } else {
                    break;
                }
            }
            if (suffix != "channel") {
                channel_name = v->value;
            }
        }
        debug(4) << "modified channel name: " << channel_name << "\n";
        string pipe_index{};
        for (unsigned i = 2; i < op->args.size(); i++) {
            Expr e = op->args[i];
            assert(e.type() == Int(32));
            auto print_e = print_expr(e);
            pipe_index += print_e + ",";
        }
        if (pipe_index.back() == ',') pipe_index.pop_back();

        ostringstream rhs;
        string write_data = print_expr(op->args[1]);
        rhs << print_name(channel_name) << "::write<" << pipe_index << ">(" << write_data << ")";
        stream << get_indent() << rhs.str() << ";\n";
    } else if (op->is_intrinsic(Call::write_channel_nb)) {
        const StringImm *v = op->args[0].as<StringImm>();
        const StringImm *write_success = op->args[2].as<StringImm>();
        // Do not directly print to stream: there might have been a cached value useable.
        ostringstream rhs;
        int size = (v->value).rfind(".");
        string channel_name = v->value;
        debug(4) << "channel name: " << channel_name << "\n";
        if (size < (int)(v->value.size())-1) {
            string suffix = (v->value).substr(size + 1, (int)v->value.size() - size - 1);
            // eliminate useless suffix
            while (suffix != "channel") {
                channel_name = (channel_name).substr(0, size);
                size = (channel_name).rfind(".");
                if (size < (int)(channel_name.size())-1) {
                    suffix = (channel_name).substr(size + 1, (int)channel_name.size() - size - 1);
                } else {
                    break;
                }
            }
            if (suffix != "channel") {
                channel_name = v->value;
            }
        }
        debug(4) << "modified channel name: " << channel_name << "\n";
        rhs << print_name(channel_name);
        for (unsigned i = 3; i < op->args.size(); i++) {
            Expr e = op->args[i];
            assert(e.type() == Int(32));
            rhs << "[";
            rhs << print_expr(e);
            rhs << "]";
        }
        rhs << ", ";
        string write_data = print_expr(op->args[1]);
        rhs << write_data;
        stream << get_indent() << print_name(write_success->value) << " = write_channel_nb_intel(" << rhs.str() << ");\n";
    } else if (op->is_intrinsic(Call::read_array)) {
        string arr_name = op->args[0].as<StringImm>()->value;
        // read the entire array as a whole
        if (op->args.size() == 1) {
            string string_index = op->type.is_handle() ? "" : ".s";
            latest_expr = print_name(arr_name) + string_index;
        } else {
            string string_index = ".s";
            for (size_t i = 1; i < op->args.size(); i++) {
                string_index += "[" + print_expr(op->args[i]) + "]";
            }
            latest_expr = print_name(arr_name) + string_index;
        }
    } else if (op->is_intrinsic(Call::write_array)) {
        string arr_name = op->args[0].as<StringImm>()->value;
        // write the entire array as a whole
        if (op->args.size() == 2) {
            string write_data = print_expr(op->args[1]);
            string string_index = op->type.is_handle() ? "" : ".s";
            stream << get_indent() << print_name(arr_name) << string_index << " = " << write_data << ";\n";
        } else {
            string write_data = print_expr(op->args[1]);
            string string_index = ".s";
            for (size_t i = 2; i < op->args.size(); i++)
                string_index += "[" + print_expr(op->args[i]) + "]";
            stream << get_indent() << print_name(arr_name) << string_index << " = " << write_data << ";\n";
        }
    } else if (op->is_intrinsic(Call::read_shift_reg)) {
        string string_index;
        const StringImm *v = op->args[0].as<StringImm>();
        string reg_name = extract_first_token(v->value);
        // shift reg has regular bounds
        if (space_vars.find(reg_name) == space_vars.end()) {
            for (size_t i = 1; i < op->args.size(); i++) {
                Expr e = op->args[i];
                assert(e.type() == Int(32));
                string sindex = print_expr(e);
                assert(!sindex.empty());
                if (i == op->args.size() - 1 && (shift_regs_bounds.find(v->value) != shift_regs_bounds.end())) {
                    // This is the last arg, and every shift reg is a non-standard vector
                    internal_assert(shift_regs_bounds[v->value] == op->args.size() - 1 || // this last arg is indexing a vector
                                    shift_regs_bounds[v->value] == op->args.size() - 2);  // this last arg is indexing a vector element
                    if (shift_regs_bounds[v->value] == op->args.size() - 1) {
                        string_index += "["+sindex+"]";
                    } else {
                        string_index += ".s["+sindex+"]";
                    }
                } else {
                    string_index += "["+sindex+"]";
                }
            }
            latest_expr = '_' + unique_name('_');
            ostringstream rhs;

            int size = (v->value).rfind(".");
            string shreg_name = v->value;
            debug(4) << "shreg name: " << shreg_name << "\n";
            if (size < (int)(v->value.size())-1) {
                string suffix = (v->value).substr(size + 1, (int)v->value.size() - size - 1);
                // eliminate useless suffix
                while (suffix != "shreg") {
                    shreg_name = (shreg_name).substr(0, size);
                    size = (shreg_name).rfind(".");
                    if (size < (int)(shreg_name.size())-1) {
                        suffix = (shreg_name).substr(size + 1, (int)shreg_name.size() - size - 1);
                    } else {
                        break;
                    }
                }
                if (suffix != "shreg") {
                    shreg_name = v->value;
                }
            }
            debug(4) << "modified shreg name: " << shreg_name << "\n";

            rhs << print_name(shreg_name) << string_index;
            set_latest_expr(op->type, rhs.str());
        } else {
            auto vars = space_vars.at(reg_name);
            // print irregular space loop index
            string suffix_index = "";
            for (size_t i = vars.size(); i >= 1; i--) {
                Expr e = op->args[i];
                assert(e.type() == Int(32));
                string sindex = print_expr(e);
                assert(!sindex.empty());
                suffix_index += "_"+sindex;
            }
            // print regular time loop index
            for (size_t i = 1 + vars.size(); i < op->args.size(); i++) {
                Expr e = op->args[i];
                assert(e.type() == Int(32));
                string sindex = print_expr(e);
                assert(!sindex.empty());
                if (i == op->args.size() - 1 && (shift_regs_bounds.find(v->value) != shift_regs_bounds.end())) {
                    // This is the last arg, and every shift reg is a non-standard vector
                    internal_assert(shift_regs_bounds[v->value] == op->args.size() - 1 || // this last arg is indexing a vector
                                    shift_regs_bounds[v->value] == op->args.size() - 2);  // this last arg is indexing a vector element
                    if (shift_regs_bounds[v->value] == op->args.size() - 1) {
                        string_index += "["+sindex+"]";
                    } else {
                        string_index += ".s["+sindex+"]";
                    }
                } else {
                    string_index += "["+sindex+"]";
                }
            }
            ostringstream rhs;
            int size = (v->value).rfind(".");
            string shreg_name = v->value;
            debug(4) << "shreg name: " << shreg_name << "\n";
            if (size < (int)(v->value.size())-1) {
                string suffix = (v->value).substr(size + 1, (int)v->value.size() - size - 1);
                // eliminate useless suffix
                while (suffix != "shreg") {
                    shreg_name = (shreg_name).substr(0, size);
                    size = (shreg_name).rfind(".");
                    if (size < (int)(shreg_name.size())-1) {
                        suffix = (shreg_name).substr(size + 1, (int)shreg_name.size() - size - 1);
                    } else {
                        break;
                    }
                }
                if (suffix != "shreg") {
                    shreg_name = v->value;
                }
            }
            debug(4) << "modified shreg name: " << shreg_name << "\n";

            rhs << print_name(shreg_name) << suffix_index << string_index;
            set_latest_expr(op->type, rhs.str());
        }
    } else if (op->is_intrinsic(Call::write_shift_reg)) {
        const StringImm *v = op->args[0].as<StringImm>();
        string reg_name = extract_first_token(v->value);
        // shift reg has regular bounds
        if (space_vars.find(reg_name) == space_vars.end()) {
            ostringstream rhs;
            int size = (v->value).rfind(".");
            string shreg_name = v->value;
            debug(4) << "shreg name: " << shreg_name << "\n";
            if (size < (int)(v->value.size())-1) {
                string suffix = (v->value).substr(size + 1, (int)v->value.size() - size - 1);
                // eliminate useless suffix
                while (suffix != "shreg") {
                    shreg_name = (shreg_name).substr(0, size);
                    size = (shreg_name).rfind(".");
                    if (size < (int)(shreg_name.size())-1) {
                        suffix = (shreg_name).substr(size + 1, (int)shreg_name.size() - size - 1);
                    } else {
                        break;
                    }
                }
                if (suffix != "shreg") {
                    shreg_name = v->value;
                }
            }
            debug(4) << "modified shreg name: " << shreg_name << "\n";

            rhs << print_name(shreg_name);
            // write shift register should has at leat 2 arguments
            internal_assert((int)op->args.size() >= 2);
            for (size_t i = 1; i < op->args.size()-1; i++) {
                Expr e = op->args[i];
                assert(e.type() == Int(32));
                string sindex = print_expr(e);
                assert(!sindex.empty());
                if (i == op->args.size() - 2 && (shift_regs_bounds.find(v->value) != shift_regs_bounds.end())) {
                    // This is the last arg, and every shift reg is a non-standard vector
                    internal_assert(shift_regs_bounds[v->value] == op->args.size() - 2 || // this last arg is indexing a vector
                                    shift_regs_bounds[v->value] == op->args.size() - 3);  // this last arg is indexing a vector element
                    if (shift_regs_bounds[v->value] == op->args.size() - 2) {
                        rhs << "["+sindex+"]";
                    } else {
                        rhs << ".s["+sindex+"]";
                    }
                } else {
                    rhs << "["+sindex+"]";
                }
            }
            string write_data = print_expr(op->args[op->args.size()-1]);
            stream << get_indent() << rhs.str() << " = " << write_data << ";\n";
        } else {
            ostringstream rhs;
            auto vars = space_vars.at(reg_name);
            int size = (v->value).rfind(".");
            string shreg_name = v->value;
            debug(4) << "shreg name: " << shreg_name << "\n";
            if (size < (int)(v->value.size())-1) {
                string suffix = (v->value).substr(size + 1, (int)v->value.size() - size - 1);
                // eliminate useless suffix
                while (suffix != "shreg") {
                    shreg_name = (shreg_name).substr(0, size);
                    size = (shreg_name).rfind(".");
                    if (size < (int)(shreg_name.size())-1) {
                        suffix = (shreg_name).substr(size + 1, (int)shreg_name.size() - size - 1);
                    } else {
                        break;
                    }
                }
                if (suffix != "shreg") {
                    shreg_name = v->value;
                }
            }
            debug(4) << "modified shreg name: " << shreg_name << "\n";

            rhs << print_name(shreg_name);
            // write shift register should has at leat 2 arguments
            internal_assert((int)op->args.size() >= 2);
            for (size_t i = vars.size(); i >= 1; i--) {
                Expr e = op->args[i];
                assert(e.type() == Int(32));
                rhs << "_";
                rhs << print_expr(e);
            }
            for (size_t i = 1 + vars.size(); i < op->args.size()-1; i++) {
                Expr e = op->args[i];
                assert(e.type() == Int(32));
                string sindex = print_expr(e);
                assert(!sindex.empty());
                if (i == op->args.size() - 2 && (shift_regs_bounds.find(v->value) != shift_regs_bounds.end())) {
                    // This is the last arg, and every shift reg is a non-standard vector
                    internal_assert(shift_regs_bounds[v->value] == op->args.size() - 2 || // this last arg is indexing a vector
                                    shift_regs_bounds[v->value] == op->args.size() - 3);  // this last arg is indexing a vector element
                    if (shift_regs_bounds[v->value] == op->args.size() - 2) {
                        rhs << "["+sindex+"]";
                    } else {
                        rhs << ".s["+sindex+"]";
                    }
                } else {
                    rhs << "["+sindex+"]";
                }
            }
            string write_data = print_expr(op->args[op->args.size()-1]);
            stream << get_indent() << rhs.str() << " = " << write_data << ";\n";
        }
    } else if (ends_with(op->name, ".ibuffer")) {
        ostringstream rhs;
        rhs << print_name(op->name + '.' + std::to_string(op->value_index));
        for (size_t i = 0; i < op->args.size(); i++) {
            rhs << "[";
            rhs << print_expr(op->args[i]);
            rhs << "]";
        }
        set_latest_expr(op->type, rhs.str());
    } else if (ends_with(op->name, ".temp")) {
        string name = op->name;
        // Do not directly print to stream: there might have been a cached value useable.
        ostringstream rhs;
        rhs << print_name(name);
        for (size_t i = 0; i < op->args.size(); i++) {
            rhs << "[";
            rhs << print_expr(op->args[i]);
            rhs << "]";
        }
        set_latest_expr(op->type, rhs.str());
    } else if (op->is_intrinsic(Call::make_struct)) {
        ostringstream rhs;
        if (op->args.empty()) {
            internal_assert(op->type.handle_type) << "The return type of the no-arg call to make_struct must be a pointer type.\n";
            rhs << "(" << print_type(op->type) + ")(nullptr)";
        } else if (op->type == type_of<halide_dimension_t *>()) {
            vector<string> values;
            values.reserve(op->args.size());
            for (size_t i = 0; i < op->args.size(); i++) {
                values.push_back(print_expr(op->args[i]));
            }
            internal_assert(values.size() % 4 == 0);
            int dimension = values.size() / 4;
            auto shape_name = unique_name('s');
            stream << get_indent() << "struct halide_dimension_t " << shape_name
                   << "[" << dimension << "] = {\n";
            indent++;
            for (int i = 0; i < dimension; i++)
                stream << get_indent() << "{"
                       << values[i * 4 + 0] << ", "
                       << values[i * 4 + 1] << ", "
                       << values[i * 4 + 2] << ", "
                       << values[i * 4 + 3] << "},\n";
            indent--;
            stream << get_indent() << "};\n";
            rhs << shape_name;
        } else {
            vector<string> values;
            values.reserve(op->args.size());
            for (size_t i = 0; i < op->args.size(); i++) {
                values.push_back(print_expr(op->args[i]));
            }
            rhs << "(" << print_type(op->type) + "){";
            for (size_t i = 0; i < op->args.size(); i++) {
                rhs << values[i];
                if (i < op->args.size() - 1) rhs << ", ";
            }
            rhs << "}";
        }
        set_latest_expr(op->type, rhs.str());
    } else if (op->is_intrinsic(Call::postincrement)) {
        // Args: an expression e, Expr(offset), Expr("addr.temp"), indexes of addr.temp
        // Print: latest_expr = expression e
        //        addr.temp[indexes] = addr.temp[indexes] + offset
        string offset = print_expr(op->args[1]);
        string addr_temp = print_name(op->args[2].as<StringImm>()->value);
        for (size_t i = 3; i < op->args.size(); i++) {
            if (op->args[i].as<Variable>() != NULL) {
                addr_temp += "[" + print_name(op->args[i].as<Variable>()->name) + "]";
            } else {
                internal_assert(op->args[i].as<IntImm>() != NULL);
                addr_temp += "[" + std::to_string(op->args[i].as<IntImm>()->value) + "]";
            }
        }
        stream << get_indent() << print_expr(op->args[0]) << ";\n";
        stream << get_indent() << addr_temp << " = " << addr_temp << " + " << offset << ";\n";
    } else if (op->is_intrinsic(Call::fpga_reg)) {
        int times = 2;
        if (op->args.size() > 1) {
            auto val = op->args[1].as<IntImm>();
            internal_assert(val);
            times = val->value;
        }
        ostringstream rhs;
        if (op->type.is_vector()) {
            std::string print_e = print_expr(op->args[0]);
            auto num_lanes = op->type.lanes();
            rhs << print_type(op->type) << "{\n";
            for (int i = 0; i < num_lanes; i++) {
                rhs << get_indent() << "sycl::ext::intel::fpga_reg(sycl::ext::intel::fpga_reg(" << print_e << "["<< i << "]))";
                if (i != num_lanes - 1) rhs << ",";
                rhs << "\n";
            }
            rhs << get_indent() << "}";
        } else {
            for (int i = 0; i < times; i++)
                rhs << "sycl::ext::intel::fpga_reg(";
            rhs << print_expr(op->args[0]);
            for (int i = 0; i < times; i++) rhs << ")";
        }
        set_latest_expr(op->type, rhs.str());
    } else if (op->is_intrinsic(Call::annotate)) {
        if (op->args[0].as<StringImm>()->value == "Pragma") {
            stream << get_indent() << "#pragma " << op->args[1].as<StringImm>()->value;
            if (op->args.size() == 3) {
                stream << " safelen(" << op->args[2].as<IntImm>()->value << ")\n";
            }
        }
    } else if (op->is_intrinsic(Call::overlay_switch)) {
        internal_assert(op->args.size() > 0);

        // Prepare task object to be dispatched
        internal_assert(op->args[0].as<StringImm>());
        string type = op->args[0].as<StringImm>()->value;

        if (type == "before_switch") {
            ostringstream rhs;
            rhs << "0";
            set_latest_expr(op->type, rhs.str());

            stream << get_indent() << "index_t index = {0, {";
            for (unsigned int k = 1; k < op->args.size(); k++) {
                stream << print_expr(op->args[k]);
                if (k != op->args.size() - 1) stream << ", ";
            }
            stream << "}};\n";

            stream << get_indent() << "arg_t inputs;\n";
            stream << get_indent() << "task_t task;\n";
            stream << get_indent() << "task.index = index;\n";
            stream << get_indent() << "task.done = false;\n";
            stream << get_indent() << "task.last = false;\n\n";

        } else if (type == "kernel_begin") {
            ostringstream rhs;
            rhs << "0";
            set_latest_expr(op->type, rhs.str());

            internal_assert(op->args.size() > 1);
            internal_assert(op->args[1].as<IntImm>());
            int queue_index = op->args[1].as<IntImm>()->value;
            stream << get_indent() << "arg_t inputs = read_channel_intel(q["
                << queue_index << "]);\n";
            stream << get_indent() << "mem_fence(CLK_CHANNEL_MEM_FENCE);\n\n";

        } else if (type == "kernel_end") {
            ostringstream rhs;
            rhs << "0";
            set_latest_expr(op->type, rhs.str());

            internal_assert(op->args.size() > 1);
            internal_assert(op->args[1].as<IntImm>());
            int queue_index = op->args[1].as<IntImm>()->value;
            // stream << get_indent()
            //     << "mem_fence(CLK_CHANNEL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);\n";
            // stream << get_indent() << "index_t f" << queue_index
            //     << " = read_channel_intel(q" << queue_index << "_end);\n";
            stream << get_indent() << "mem_fence(CLK_CHANNEL_MEM_FENCE);\n";
            stream << get_indent() << "write_channel_intel(q_ret[" << queue_index << "], inputs.finish);\n";

        // Print data from task channel for autorun kernels
        } else if (type == "data") {
            ostringstream rhs;
            internal_assert(op->args[1].as<StringImm>());
            rhs << op->args[1].as<StringImm>()->value;
            set_latest_expr(op->type, rhs.str());

        } else if (type == "end_signal") {
            ostringstream rhs;
            rhs << "0";
            set_latest_expr(op->type, rhs.str());
            stream << get_indent() << "push_end = true;\n";

        } else if (type == "before_main_body") {
            ostringstream rhs;
            rhs << "0";
            set_latest_expr(op->type, rhs.str());
            stream << get_indent() << "bool push_end = false;\n";

        } else if (type == "after_main_body") {
            ostringstream rhs;
            rhs << "0";
            set_latest_expr(op->type, rhs.str());
            stream << R"(
 if (push_end) {
    task_t end;
    end.last = true;
    write_channel_intel(qt, end);
    mem_fence(CLK_CHANNEL_MEM_FENCE);

    int count = read_channel_intel(qf);
    mem_fence(CLK_CHANNEL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);
 }
)";
        }

    // Print task switching loop body
    } else if (op->is_intrinsic(Call::overlay)) {

        char *overlay_num = getenv("HL_OVERLAY_NUM");
        internal_assert(overlay_num != NULL);

        internal_assert(op->args.size() > 0);
        ostringstream rhs;
        rhs << "0";
        set_latest_expr(op->type, rhs.str());

        struct status {
            vector<Expr> iter_vars;
            vector<Expr> conditions;
        };

        struct input {
            string  name;
            string  value;
        };

        int dep_queue_index = -1;
        std::map<int, status> info;
        vector<input> inputs;

        auto task_id = op->args[0].as<IntImm>()->value;
        auto queueNo = op->args[1].as<IntImm>()->value;

        for (int k = 2; k < (signed)op->args.size(); k++) {
            auto e = op->args[k];
            if (auto name = e.as<StringImm>()) {
                string arg_name = name->value;

                // args as ( dep_0, parent queue index, itervar, cond...)
                // Example:
                // overlay("task_0", 2, (Task2.s0.i-1), (1<+Task2.s0.i), "buffer::b1")
                //
                if (starts_with(arg_name, "inputs.args")) {
                    string delimiter = "::";
                    size_t pos = arg_name.find(delimiter);
                    string id = arg_name.substr(0, pos);
                    // arg_name is like inputs.args0::b1. Look up the declared name  without the arg index, i.e. inputs.args::b1.
                    string printed_arg_name = print_name("inputs.args::" + arg_name.substr(pos + 2, arg_name.size()));
                    string value = printed_arg_name + " + " + print_expr(op->args[k+1]);
                    input entry = {id, value};
                    k++;
                    inputs.push_back(entry);
                } else if (starts_with(arg_name, "inputs.constants")) {
                    input entry = {arg_name, print_expr(op->args[k+1])};
                    k++;
                    inputs.push_back(entry);
                } else {
                    internal_assert(false) << "Unrecognized arg input "
                        << arg_name << " in the enqueue() function given...";
                }

            // Depending queue index
            } else if (auto queue = e.as<IntImm>()) {
                dep_queue_index = queue->value;
                info[dep_queue_index] = {{}, {}};

            // Depending task iter var index
            } else if (e.as<Add>() || e.as<Variable>() || e.as<Sub>()) {
                info[dep_queue_index].iter_vars.push_back(e);

            // Depending task conditions
            } else if (e.as<GT>() || e.as<LT>() || e.as<NE>() || e.as<GE>() || e.as<LE>()) {
                info[dep_queue_index].conditions.push_back(e);
            }
        }

        // Print task dispatching logic
        int dep_num = 0;
        stream << get_indent() << "// Sending task" << task_id << " to the scheduler...\n";
        stream << get_indent() << "task.queue = " << queueNo << ";\n";
        stream << get_indent() << "task.index.task_id = " << task_id << ";\n\n";

        int cond_num = 0;
        ostringstream temp;
        for (auto& kv : info) {
            string cond;
            // Print depending task iter var index
            if (kv.second.conditions.size() > 0) {
                cond_num += 1;
                // TO FIX: incomplete conds
                for (unsigned int o = 0; o < kv.second.conditions.size(); o++) {
                    cond += print_expr(kv.second.conditions[o]);
                    if (o != kv.second.conditions.size() - 1) {
                        cond += " && ";
                    }
                }

                temp << get_indent()  << "index_t dep_" << task_id << "_" << dep_num
                     << " = {" << kv.first << ", {";

                for (unsigned int o = 0; o < kv.second.iter_vars.size(); o++) {
                    temp << cond << " ? "
                        << print_expr(kv.second.iter_vars[o])
                        << " : -1";
                    if (o != kv.second.iter_vars.size() - 1) temp << ", ";
                }
            } else {
                temp << get_indent() << "index_t dep_" << task_id << "_" << dep_num
                     << " = {" << kv.first << ", {";
                for (unsigned int o = 0; o < kv.second.iter_vars.size(); o++) {
                    auto var = print_expr(kv.second.iter_vars[o]);
                    temp << var;
                    if (o != kv.second.iter_vars.size() - 1) temp << ", ";
                }
            }

            temp << "}};\n";
            temp << get_indent() << "task.deps[" << dep_num << "]"
                   << " = dep_" << task_id << "_" << dep_num << ";\n";
            dep_num ++;
        }

        stream << temp.str();
        stream << get_indent() << "task.num_of_deps = " << dep_num << ";\n\n";
        // if (cond_num == 0) {
        //   stream << get_indent() << "task.num_of_deps = "
        //          << dep_num << ";\n\n";
        // } else {
        //   stream << get_indent() << "task.num_of_deps = ("
        //          << cond << " ? " << dep_num << " : "
        //          << dep_num - cond_num << ");\n\n";
        // }

        // Prepare input information
        stream << get_indent() << "inputs.finish = task.index;\n";
        for (auto& input : inputs) {
            string name = input.name;
            stream << get_indent() << input.name << " = "
                << input.value << ";\n";
        }
        stream << get_indent() << "task.inputs = inputs;\n";
        stream << get_indent() << "write_channel_intel(qt, task);\n";
        stream << get_indent() << "mem_fence(CLK_CHANNEL_MEM_FENCE);\n\n";
    } else {
        // Other intrinsics
        CodeGen_Clear_C::visit(op);
    }
}

string CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::print_extern_call(const Call *op) {
    internal_assert(!function_takes_user_context(op->name));
    vector<string> args(op->args.size());
    for (size_t i = 0; i < op->args.size(); i++) {
        args[i] = print_expr(op->args[i]);
    }
    ostringstream rhs;
    rhs << op->name << "(" << with_commas(args) << ")";
    return rhs.str();
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::visit(const Load *op) {
    user_assert(is_one(op->predicate)) << "Predicated load is not supported inside OpenCL kernel.\n";

    // If we're loading a contiguous ramp into a vector, use vload instead.
    Expr ramp_base = strided_ramp_base(op->index);
    if (ramp_base.defined()) {
        internal_assert(op->type.is_vector());
        string id_ramp_base = generate_intermediate_var(print_expr(ramp_base));

        ostringstream rhs;

        if (!is_standard_opencl_type(op->type)) {
            // For compiler-generated vector types, read in the way like *((float6*) address)
            rhs << "*((" << print_type(op->type) << "*)(" << print_name(op->name)
                   << " + " << id_ramp_base << "))";
        } else {
            rhs << print_type(op->type) << "{\n";
            indent++;
            for (int i = 0; i < op->type.lanes(); i++)
                rhs << get_indent() << print_name(op->name) << "[" << id_ramp_base << " + " << i << "]"
                    << (i != op->type.lanes() - 1 ? ",\n" : "\n");
            indent--;
            rhs << get_indent()  << "}";
        }
        needs_intermediate_expr = true;
        set_latest_expr(op->type, rhs.str());
        return;
    }

    string id_index = generate_intermediate_var(print_expr(op->index));

    // Get the rhs just for the cache.
    bool type_cast_needed = !(allocations.contains(op->name) &&
                              allocations.get(op->name).type == op->type);
    ostringstream rhs;
    if (pointer_args.count(op->name)) {
        rhs << pointer_args[op->name];
    } else if (type_cast_needed) {
        rhs << "(("
            << print_type(op->type) << " *)"
            << print_name(op->name)
            << ")";
    } else {
        rhs << print_name(op->name);
    }
    rhs << "[" << id_index << "]";
    if (op->index.type().is_vector()) {
        // If index is a vector, gather vector elements.
        internal_assert(op->type.is_vector());

        latest_expr = "_" + unique_name('V');
        stream << get_indent() << print_type(op->type)
               << " " << latest_expr << ";\n";

        for (int i = 0; i < op->type.lanes(); i++) {
            stream << get_indent();
            stream
                << latest_expr << "[" << i << "]"
                << " = (("
                << print_type(op->type.element_of()) << "*)"
                << print_name(op->name) << ")"
                << "[" << id_index << "[" << i << "]];\n";
        }
    } else {
        set_latest_expr(op->type, rhs.str());
    }
    needs_intermediate_expr = false;
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::visit(const Store *op) {
    user_assert(is_one(op->predicate)) << "Predicated store is not supported inside OpenCL kernel.\n";

    if (emit_atomic_stores) {
        // Currently only support scalar atomics.
        user_assert(op->value.type().is_scalar()) << "OpenCL atomic store does not support vectorization.\n";
        user_assert(op->value.type().bits() >= 32) << "OpenCL only support 32 and 64 bit atomics.\n";
        if (op->value.type().bits() == 64) {
            user_assert(target.has_feature(Target::CLAtomics64))
                << "Enable feature CLAtomics64 for 64-bit atomics in OpenCL.\n";
        }
        // Detect whether we can describe this as an atomic-read-modify-write,
        // otherwise fallback to a compare-and-swap loop.
        // Current only test for atomic add.
        Expr val_expr = op->value;
        Type t = val_expr.type();
        Expr equiv_load = Load::make(t, op->name, op->index, Buffer<>(), op->param, op->predicate, op->alignment);
        Expr delta = simplify(common_subexpression_elimination(op->value - equiv_load));
        // For atomicAdd, we check if op->value - store[index] is independent of store.
        // The atomicAdd operations in OpenCL only supports integers so we also check that.
        bool is_atomic_add = t.is_int_or_uint() && !expr_uses_var(delta, op->name);
        bool type_cast_needed = !(allocations.contains(op->name) &&
                                  allocations.get(op->name).type == t);
        auto print_store_var = [&]() {
            if (type_cast_needed) {
                stream << "(("
                       << print_type(t)
                       << " *)"
                       << print_name(op->name)
                       << ")";
            } else {
                stream << print_name(op->name);
            }
        };
        if (is_atomic_add) {
            string id_index = print_expr(op->index);
            string id_delta = print_expr(delta);
            stream << get_indent();
            // atomic_add(&x[i], delta);
            if (t.bits() == 32) {
                stream << "atomic_add(&";
            } else {
                stream << "atom_add(&";
            }

            print_store_var();
            stream << "[" << id_index << "]";
            stream << "," << id_delta << ");\n";
        } else {
            // CmpXchg loop
            // {
            //   union {unsigned int i; float f;} old_val;
            //   union {unsigned int i; float f;} new_val;
            //   do {
            //     old_val.f = x[id_index];
            //     new_val.f = ...
            //   } while(atomic_cmpxchg((volatile address_space unsigned int*)&x[id_index], old_val.i, new_val.i) != old_val.i);
            // }
            stream << get_indent() << "{\n";
            indent += 2;
            string id_index = print_expr(op->index);
            string int_type = t.bits() == 32 ? "int" : "long";
            if (t.is_float() || t.is_uint()) {
                int_type = "unsigned " + int_type;
            }
            if (t.is_float()) {
                stream << get_indent() << "union {" << int_type << " i; " << print_type(t) << " f;} old_val;\n";
                stream << get_indent() << "union {" << int_type << " i; " << print_type(t) << " f;} new_val;\n";
            } else {
                stream << get_indent() << int_type << " old_val;\n";
                stream << get_indent() << int_type << " new_val;\n";
            }
            stream << get_indent() << "do {\n";
            indent += 2;
            stream << get_indent();
            if (t.is_float()) {
                stream << "old_val.f = ";
            } else {
                stream << "old_val = ";
            }
            print_store_var();
            stream << "[" << id_index << "];\n";
            string id_value = print_expr(op->value);
            stream << get_indent();
            if (t.is_float()) {
                stream << "new_val.f = ";
            } else {
                stream << "new_val = ";
            }
            stream << id_value << ";\n";
            indent -= 2;
            string old_val = t.is_float() ? "old_val.i" : "old_val";
            string new_val = t.is_float() ? "new_val.i" : "new_val";
            stream << get_indent()
                   << "} while(atomic_cmpxchg((volatile "
                   << int_type << "*)&"
                   << print_name(op->name) << "[" << id_index << "], "
                   << old_val << ", " << new_val << ") != " << old_val << ");\n"
                   << get_indent() << "}\n";
            indent -= 2;
        }
        return;
    }

    string id_value = generate_intermediate_var(print_expr(op->value));
    Type t = op->value.type();

    // If we're writing a contiguous ramp, use vstore instead.
    Expr ramp_base = strided_ramp_base(op->index);
    if (ramp_base.defined()) {
        internal_assert(op->value.type().is_vector());
        string id_ramp_base = generate_intermediate_var(print_expr(ramp_base));

        if (!is_standard_opencl_type(t)) {
            // For compiler-generated vector types, write in the way like
            //   *((float6*) address) = value
            stream << get_indent() << "*(("
                   << print_type(t) << "*)(" << print_name(op->name)
                   << " + " << id_ramp_base << ")) = " << id_value << ";\n";
        } else {
            for (int i = 0; i < t.lanes(); i++)
                stream << get_indent() << print_name(op->name)
                       << "[" << id_ramp_base << " + " << i << "] = "
                       <<  id_value << "["<< i << "]"<< ";\n";
        }
    } else if (op->index.type().is_vector()) {
        // If index is a vector, scatter vector elements.
        internal_assert(t.is_vector());

        string id_index = generate_intermediate_var(print_expr(op->index));

        for (int i = 0; i < t.lanes(); ++i) {
            stream << get_indent() << "(("
                   << print_type(t.element_of()) << " *)"
                   << print_name(op->name)
                   << ")["
                   << id_index << "[" << i << "]] = "
                   << id_value << "[" << i << "];\n";
        }
    } else {
        bool type_cast_needed = !(allocations.contains(op->name) &&
                                  allocations.get(op->name).type == t);

        string id_index = generate_intermediate_var(print_expr(op->index));
        stream << get_indent();

        // Avoid type casting for global pointer
        if (pointer_args.count(op->name)) {
            stream << pointer_args[op->name];
        } else if (type_cast_needed) {
            stream << "(("
                   << print_type(t)
                   << " *)"
                   << print_name(op->name)
                   << ")";
        } else {
            stream << print_name(op->name);
        }
        stream << "[" << id_index << "] = "
               << id_value << ";\n";
    }
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::visit_binop(Type t, Expr a, Expr b, const char *op) {
    if (is_standard_opencl_type(t) && is_standard_opencl_type(a.type())) {
        CodeGen_Clear_C::visit_binop(t, a, b, op);
    } else {
        // Output something like bool16 x = {a.s0 op b.s0, a.s1 op b.s0, ...}
        internal_assert(t.is_vector() && a.type().is_vector() && t.lanes() == a.type().lanes());
        std::ostringstream oss;
        string sa = print_expr(a);
        string sb = print_expr(b);
        for (int i = 0; i < t.lanes(); i++) {
            oss << ((i == 0) ? "(" + print_type(t) + ") {" : ", ") << sa << ".s" << vector_index_to_string(i) << op
                << sb << ".s" << vector_index_to_string(i);
        }
        oss << "}";
        set_latest_expr(t, oss.str());
    }
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::visit(const EQ *op) {
    visit_binop(op->type, op->a, op->b, "==");
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::visit(const NE *op) {
    visit_binop(op->type, op->a, op->b, "!=");
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::visit(const LT *op) {
    visit_binop(op->type, op->a, op->b, "<");
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::visit(const LE *op) {
    visit_binop(op->type, op->a, op->b, "<=");
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::visit(const GT *op) {
    visit_binop(op->type, op->a, op->b, ">");
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::visit(const GE *op) {
    visit_binop(op->type, op->a, op->b, ">=");
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::visit(const Cast *op) {
    if (!target.has_feature(Target::CLHalf) &&
        ((op->type.is_float() && op->type.bits() < 32) ||
         (op->value.type().is_float() && op->value.type().bits() < 32))) {
        Expr equiv = lower_float16_cast(op);
        equiv.accept(this);
        return;
    }

    if (op->type.is_vector()) {
        string value = print_expr(op->value);
        if (op->type.is_bool()) {
            // We have standardized the IR so that op value must be a viriable.
            internal_assert(op->value.as<Variable>());
            std::ostringstream oss;
            for (int i = 0; i < op->type.lanes(); i++) {
                oss << (i == 0 ? "{" : ", ") << "(bool)" << value << ".s" << vector_index_to_string(i);
            }
            oss << "}";
            set_latest_expr(op->type, oss.str());
        } else {
            set_latest_expr(op->type, "convert_" + print_type(op->type) + "(" + print_expr(op->value) + ")");
        }
    } else {
        CodeGen_Clear_C::visit(op);
    }
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::visit(const Select *op) {
    // The branch(es) might contain actions with side effects like a channel read.
    // Thus we must guard the branch(es).
    // So first convert to if_then_else.
    user_assert(op->condition.type().is_scalar())
        << "The OpenCL does not support branch divergence. "
        << "Please do not perform vectorization if the value of a URE depends on the incoming data.\n";
    if (op->false_value.defined()) {
        string cond_id = print_expr(op->condition);
        string true_value = print_expr(op->true_value);
        string false_value = print_expr(op->false_value);

        char select_op[3];
        select_op[0] = '?';
        select_op[1] = ':';
        select_op[2] = '\0';

        string cond_id1 = (precedence_of_op(select_op) <= precedence_of_expr(op->condition)) ? "(" + cond_id + ")" : cond_id;
        string true_value1 = (precedence_of_op(select_op) <= precedence_of_expr(op->true_value)) ? "(" + true_value + ")" : true_value;
        string false_value1 = (precedence_of_op(select_op) <= precedence_of_expr(op->false_value)) ? "(" + false_value + ")" : false_value;
        set_latest_expr(op->type,  cond_id1 + " ? " + true_value1 + " : " + false_value1);
        return;
    }

    Expr c = Call::make(op->type, Call::if_then_else, {op->condition, op->true_value, op->false_value}, Call::PureIntrinsic);
    c.accept(this);


}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::visit(const IfThenElse *op) {
    string cond_id = print_expr(op->condition);
    stream << get_indent() << "if (" << cond_id << ") ";
    open_scope();
    op->then_case.accept(this);
    close_scope("if " + cond_id);

    if (op->else_case.defined()) {
        stream << get_indent() << "else ";
        open_scope();
        op->else_case.accept(this);
        close_scope("if " + cond_id + " else");
    }
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::visit(const Allocate *op) {
    user_assert(!op->new_expr.defined()) << "Allocate node inside OpenCL kernel has custom new expression.\n"
                                         << "(Memoization is not supported inside GPU kernels at present.)\n";

    map_verbose_to_succinct_globally(op->name, CodeGen_Clear_C::print_name(op->name));

    if (op->name == "__shared") {
        // Already handled
        op->body.accept(this);
    } else {
        // Check if the allocation is for global mutable pointer
        if (op->memory_type == MemoryType::CLPtr) {
            user_assert(op->extents.size() == 1);
            user_assert(op->extents[0].as<StringImm>());
            auto arg = op->extents[0].as<StringImm>()->value;
            pointer_args[op->name] = arg;
            debug(2) << "Found pointer arg " << op->name << "... \n";

            op->body.accept(this);
        } else {
            open_scope();

            debug(2) << "Allocate " << op->name << " on device\n";

            debug(3) << "Pushing allocation called " << op->name << " onto the symbol table\n";

            // Allocation is not a shared memory allocation, just make a local declaration.
            // It must have a constant size.
            int32_t size = op->constant_allocation_size();
            user_assert(size > 0)
                << "Allocation " << op->name << " has a dynamic size. "
                << "Only fixed-size allocations are supported on the gpu. "
                << "Try storing into shared memory instead.";

            stream << get_indent() << print_type(op->type) << ' '
                   << print_name(op->name) << "[" << size << "];\n";

            Allocation alloc;
            alloc.type = op->type;
            allocations.push(op->name, alloc);

            op->body.accept(this);

            // Should have been freed internally
            internal_assert(!allocations.contains(op->name));

            close_scope("alloc " + print_name(op->name));
        }
    }
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::visit(const Free *op) {
    if (op->name == "__shared") {
        return;
    } else {
        // Should have been freed internally
        internal_assert(allocations.contains(op->name));
        allocations.pop(op->name);
    }
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::visit(const AssertStmt *op) {
    user_warning << "Ignoring assertion inside OpenCL kernel: " << op->condition << "\n";
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::visit(const Shuffle *op) {
    int op_lanes = op->type.lanes();
    internal_assert(!op->vectors.empty());
    int arg_lanes = op->vectors[0].type().lanes();
    // TODO: [SIZE] the following logic for interleave is unnecessary, I think
    if (op->is_interleave()) {
        if (op->vectors.size() == 1) {
            // 1 argument, just do a simple assignment
            internal_assert(op_lanes == arg_lanes);
            set_latest_expr(op->type, print_expr(op->vectors[0]));
        } else if (op->vectors.size() == 2) {
            // 2 arguments, set the .even to the first arg and the
            // .odd to the second arg
            internal_assert(op->vectors[1].type().lanes() == arg_lanes);
            internal_assert(op_lanes / 2 == arg_lanes);
            string a1 = print_expr(op->vectors[0]);
            string a2 = print_expr(op->vectors[1]);
            latest_expr = unique_name('_');
            stream << get_indent() << print_type(op->type) << " " << latest_expr << ";\n";
            stream << get_indent() << latest_expr << ".even = " << a1 << ";\n";
            stream << get_indent() << latest_expr << ".odd = " << a2 << ";\n";
        } else {
            // 3+ arguments, interleave via a vector literal
            // selecting the appropriate elements of the vectors
            int dest_lanes = op->type.lanes();
            internal_assert(dest_lanes <= 16);
            int num_vectors = op->vectors.size();
            vector<string> arg_exprs(num_vectors);
            for (int i = 0; i < num_vectors; i++) {
                internal_assert(op->vectors[i].type().lanes() == arg_lanes);
                arg_exprs[i] = print_expr(op->vectors[i]);
            }
            internal_assert(num_vectors * arg_lanes >= dest_lanes);
            latest_expr = unique_name('_');
            stream << get_indent() << print_type(op->type) << " " << latest_expr;
            stream << " = (" << print_type(op->type) << ")(";
            for (int i = 0; i < dest_lanes; i++) {
                int arg = i % num_vectors;
                int arg_idx = i / num_vectors;
                internal_assert(arg_idx <= arg_lanes);
                stream << arg_exprs[arg] << "[" << arg_idx << "]";
                if (i != dest_lanes - 1) {
                    stream << ", ";
                }
            }
            stream << ");\n";
        }
    } else {
        // implement shuffle for other cases
        internal_assert(op->indices.size() == 1 || (int)op->indices.size() == op_lanes)
            << "Shuffle indices length not match expected lane size\n";
        internal_assert(op_lanes == 1 || op_lanes == 2 || op_lanes == 3 || op_lanes == 4
            || op_lanes == 8 || op_lanes == 16 ||
            // Otherwise, this is to merge several inputs into a non-standard vector type
            (op->is_concat() && !is_standard_opencl_type(op->type)))
            << "Unsupported vector length, only support"
            << " length in [1, 2, 3, 4, 8, 16] in OpenCL\n";
        int num_vectors = op->vectors.size();
        int total_lanes = 0;
        vector<std::pair<int, int>> index_table;
        vector<bool> is_vec;
        vector<string> arg_exprs(num_vectors);
        for (int i = 0; i < num_vectors; ++i) {
            arg_exprs[i] = print_expr(op->vectors[i]);
            int arg_lanes = op->vectors[i].type().lanes();
            internal_assert(arg_lanes == 1 || arg_lanes == 2 || arg_lanes == 3 || arg_lanes == 4
            || arg_lanes == 8 || arg_lanes == 16) << "Unsupported vector length, only support"
                                                << " length in [1, 2, 3, 4, 8, 16] in OpenCL\n";
            total_lanes += arg_lanes;
            is_vec.push_back(arg_lanes > 1);
            for (int j = 0; j < arg_lanes; ++j) {
                index_table.push_back(std::make_pair(i, j));
            }
        }
        internal_assert(total_lanes >= op_lanes) << "No enough source lanes for shuffle\n";
        // name for result
        latest_expr = unique_name('_');
        stream << get_indent() << print_type(op->type) << " " << latest_expr;
        stream << " = (" << print_type(op->type) << ")";
        if (!is_standard_opencl_type(op->type)) {
            stream << "{";
        } else {
            stream << "(";
        }

        for (int i = 0; i < op_lanes; ++i) {
            // which vector
            int p_arg = index_table[op->indices[i]].first;
            // which lane
            int p_arg_lane = index_table[op->indices[i]].second;
            if (is_vec[p_arg]) {
                stream << arg_exprs[p_arg] << "[" << p_arg_lane << "]";
            } else {
                stream << arg_exprs[p_arg];
            }
            if (i != op_lanes - 1) {
                stream << ", ";
            }
        }
        if (!is_standard_opencl_type(op->type)) {
            stream << "};\n";
        } else {
            stream << ");\n";
        }

        // internal_error << "Shuffle not implemented.\n";
    }
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::visit(const Max *op) {
    ostringstream rhs;
    rhs << "max(" << print_expr(op->a) << ", " << print_expr(op->b) << ")";
    set_latest_expr(op->type, rhs.str());
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::visit(const Min *op) {
    ostringstream rhs;
    rhs << "min(" << print_expr(op->a) << ", " << print_expr(op->b) << ")";
    set_latest_expr(op->type, rhs.str());
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::visit(const Atomic *op) {
    // Most GPUs require all the threads in a warp to perform the same operations,
    // which means our mutex will lead to deadlock.
    user_assert(op->mutex_name.empty())
        << "The atomic update requires a mutex lock, which is not supported in OpenCL.\n";

    // Issue atomic stores.
    ScopedValue<bool> old_emit_atomic_stores(emit_atomic_stores, true);
    IRVisitor::visit(op);
}

void CodeGen_Clear_OneAPI_Dev::add_kernel(Stmt s,
                                    const string &name,
                                    const vector<DeviceArgument> &args) {
    debug(2) << "CodeGen_Clear_OneAPI_Dev::compile " << name << "\n";

    // TODO: do we have to uniquify these names, or can we trust that they are safe?
    cur_kernel_name = name;
    clc.add_kernel(s, name, args);
}

namespace {
struct BufferSize {
    string name;
    size_t size;

    BufferSize()
        : size(0) {
    }
    BufferSize(string name, size_t size)
        : name(name), size(size) {
    }

    bool operator<(const BufferSize &r) const {
        return size < r.size;
    }
};
}  // namespace

// Check if this is a piece of code for an autorun kernel.
class IsAutorun : public IRVisitor {
    using IRVisitor::visit;
    void visit(const For *op) override {
        if (ends_with(op->name, ".autorun.run_on_device")) {
            is_autorun = true;
            return;
        }
        IRVisitor::visit(op);
    }
    public:
        bool is_autorun;
        IsAutorun() : is_autorun(false) {}
};

bool CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::succinct_name_is_unique(const string &verbose, const string &succinct) {
    for (auto &n : global_name_map) {
        if (n.second == succinct && n.first != verbose) {
            return false;
        }
    }
    for (auto &n : kernel_name_map) {
        if (n.second == succinct && n.first != verbose) {
            return false;
        }
    }
    return true;
}

string CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::print_name(const string &name) {
    auto it = global_name_map.find(name);
    if (it != global_name_map.end()) {
        return it->second;
    } else {
        auto it1 = kernel_name_map.find(name);
        if (it1 != kernel_name_map.end()) {
            return it1->second;
        }
        return CodeGen_Clear_C::print_name(name);
    }
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::map_verbose_to_succinct_globally(const string &verbose, const string &succinct) {
    // Make sure the succinct name corresponds to only one verbose name
    string succ = succinct;
    while (!succinct_name_is_unique(verbose, succ)) {
        succ += "_";
    }
    global_name_map[verbose] = succ;
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::map_verbose_to_succinct_locally(const string &verbose, const string &succinct) {
    // Make sure the succinct name corresponds to only one verbose name
    string succ = succinct;
    while (!succinct_name_is_unique(verbose, succ)) {
        succ += "_";
    }
    kernel_name_map[verbose] = succ;
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::add_kernel(Stmt s,
                                                      const string &name,
                                                      const vector<DeviceArgument> &args) {
    // No name seen for this kernel yet
    kernel_name_map.clear();

    debug(2) << "Adding OpenCL kernel " << name << "\n";

    // Figure out which arguments should be passed in __constant.
    // Such arguments should be:
    // - not written to,
    // - loads are block-uniform,
    // - constant size,
    // - and all allocations together should be less than the max constant
    //   buffer size given by CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE.
    // The last condition is handled via the preprocessor in the kernel
    // declaration.
    vector<BufferSize> constants;
    for (size_t i = 0; i < args.size(); i++) {
        if (args[i].is_buffer &&
            CodeGen_GPU_Dev::is_buffer_constant(s, args[i].name) &&
            args[i].size > 0) {
            constants.push_back(BufferSize(args[i].name, args[i].size));
        }
    }

    // Sort the constant candidates from smallest to largest. This will put
    // as many of the constant allocations in __constant as possible.
    // Ideally, we would prioritize constant buffers by how frequently they
    // are accessed.
    sort(constants.begin(), constants.end());

    // Compute the cumulative sum of the constants.
    for (size_t i = 1; i < constants.size(); i++) {
        constants[i].size += constants[i - 1].size;
    }

    stream << "__kernel void " << name << "(\n";
    for (size_t i = 0; i < args.size(); i++) {
        if (args[i].is_buffer) {
            // TODO: Update buffer attributes if written in kernel ip
            char *overlay_num = getenv("HL_OVERLAY_NUM");
            if (!args[i].write && overlay_num == NULL) stream << "const ";
            string printed_name = print_name(args[i].name);
            map_verbose_to_succinct_locally("inputs.args::" + args[i].name, printed_name);
            stream << print_type(args[i].type) << " *"
                   << "restrict "
                   << printed_name;
            Allocation alloc;
            alloc.type = args[i].type;
            allocations.push(args[i].name, alloc);
        } else {
            Type t = args[i].type;
            string name = args[i].name;
            // Bools are passed as a uint8.
            t = t.with_bits(t.bytes() * 8);
            // float16 are passed as uints
            if (t.is_float() && t.bits() < 32) {
                t = t.with_code(halide_type_uint);
                name += "_bits";
            }
            string printed_name = print_name(name);
            stream << " const "
                   << print_type(t)
                   << " "
                   << printed_name;
        }

        if (i < args.size() - 1) stream << ",\n";
    }
    if (!target.has_feature(Target::IntelFPGA)) {
        // TOFIX: should not we add shared only when there is shared memory passed in?
        stream << ",\n"
               << " __address_space___shared int16* __shared";
    }
    stream << ") ";

    open_scope();

    // Reinterpret half args passed as uint16 back to half
    for (size_t i = 0; i < args.size(); i++) {
        if (!args[i].is_buffer &&
            args[i].type.is_float() &&
            args[i].type.bits() < 32) {
            stream << " const " << print_type(args[i].type)
                   << " " << print_name(args[i].name)
                   << " = half_from_bits(" << print_name(args[i].name + "_bits") << ");\n";
        }
    }

    DeclareArrays da(this);
    s.accept(&da);
    stream << da.arrays.str();

    print(s);
    close_scope("kernel " + name);

    for (size_t i = 0; i < args.size(); i++) {
        // Remove buffer arguments from allocation scope
        if (args[i].is_buffer) {
            allocations.pop(args[i].name);
        }
    }

}

void CodeGen_Clear_OneAPI_Dev::init_module() {
    debug(2) << "OpenCL device codegen init_module\n";

    // wipe the internal kernel source
    src_stream.str("");
    src_stream.clear();

    const Target &target = clc.get_target();

    // Check whether it's compiled for ip kernel
    char *overlay_kenrel = getenv("HL_OVERLAY_KERNEL");
    if (overlay_kenrel != NULL) return;

    // This identifies the program as OpenCL C (as opposed to SPIR).
    src_stream << "/*OpenCL C " << target.to_string() << "*/\n";

    src_stream << "#pragma OPENCL FP_CONTRACT ON\n";

    // Write out the Halide math functions.
    src_stream << "#define float_from_bits(x) as_float(x)\n"
               << "inline float nan_f32() { return NAN; }\n"
               << "inline float neg_inf_f32() { return -INFINITY; }\n"
               << "inline float inf_f32() { return INFINITY; }\n"
               << "inline bool is_nan_f32(float x) {return isnan(x); }\n"
               << "inline bool is_inf_f32(float x) {return isinf(x); }\n"
               << "inline bool is_finite_f32(float x) {return isfinite(x); }\n"
               << "#define sqrt_f32 sqrt \n"
               << "#define sin_f32 sin \n"
               << "#define cos_f32 cos \n"
               << "#define exp_f32 exp \n"
               << "#define log_f32 log \n"
               << "#define abs_f32 fabs \n"
               << "#define floor_f32 floor \n"
               << "#define ceil_f32 ceil \n"
               << "#define round_f32 round \n"
               << "#define trunc_f32 trunc \n"
               << "#define pow_f32 pow\n"
               << "#define asin_f32 asin \n"
               << "#define acos_f32 acos \n"
               << "#define tan_f32 tan \n"
               << "#define atan_f32 atan \n"
               << "#define atan2_f32 atan2\n"
               << "#define sinh_f32 sinh \n"
               << "#define asinh_f32 asinh \n"
               << "#define cosh_f32 cosh \n"
               << "#define acosh_f32 acosh \n"
               << "#define tanh_f32 tanh \n"
               << "#define atanh_f32 atanh \n"
               << "#define fast_inverse_f32 native_recip \n"
               << "#define fast_inverse_sqrt_f32 native_rsqrt \n";

    // __shared always has address space __local.
    src_stream << "#define __address_space___shared __local\n";

    if (target.has_feature(Target::CLDoubles)) {
        src_stream << "#pragma OPENCL EXTENSION cl_khr_fp64 : enable\n"
                   << "inline bool is_nan_f64(double x) {return isnan(x); }\n"
                   << "inline bool is_inf_f64(double x) {return isinf(x); }\n"
                   << "inline bool is_finite_f64(double x) {return isfinite(x); }\n"
                   << "#define sqrt_f64 sqrt\n"
                   << "#define sin_f64 sin\n"
                   << "#define cos_f64 cos\n"
                   << "#define exp_f64 exp\n"
                   << "#define log_f64 log\n"
                   << "#define abs_f64 fabs\n"
                   << "#define floor_f64 floor\n"
                   << "#define ceil_f64 ceil\n"
                   << "#define round_f64 round\n"
                   << "#define trunc_f64 trunc\n"
                   << "#define pow_f64 pow\n"
                   << "#define asin_f64 asin\n"
                   << "#define acos_f64 acos\n"
                   << "#define tan_f64 tan\n"
                   << "#define atan_f64 atan\n"
                   << "#define atan2_f64 atan2\n"
                   << "#define sinh_f64 sinh\n"
                   << "#define asinh_f64 asinh\n"
                   << "#define cosh_f64 cosh\n"
                   << "#define acosh_f64 acosh\n"
                   << "#define tanh_f64 tanh\n"
                   << "#define atanh_f64 atanh\n";
    }

    if (target.has_feature(Target::CLHalf)) {
        src_stream << "#pragma OPENCL EXTENSION cl_khr_fp16 : enable\n"
                   << "inline half half_from_bits(unsigned short x) {return __builtin_astype(x, half);}\n"
                   << "inline half nan_f16() { return half_from_bits(32767); }\n"
                   << "inline half neg_inf_f16() { return half_from_bits(31744); }\n"
                   << "inline half inf_f16() { return half_from_bits(64512); }\n"
                   << "inline bool is_nan_f16(half x) {return isnan(x); }\n"
                   << "inline bool is_inf_f16(half x) {return isinf(x); }\n"
                   << "inline bool is_finite_f16(half x) {return isfinite(x); }\n"
                   << "#define sqrt_f16 sqrt\n"
                   << "#define sin_f16 sin\n"
                   << "#define cos_f16 cos\n"
                   << "#define exp_f16 exp\n"
                   << "#define log_f16 log\n"
                   << "#define abs_f16 fabs\n"
                   << "#define floor_f16 floor\n"
                   << "#define ceil_f16 ceil\n"
                   << "#define round_f16 round\n"
                   << "#define trunc_f16 trunc\n"
                   << "#define pow_f16 pow\n"
                   << "#define asin_f16 asin\n"
                   << "#define acos_f16 acos\n"
                   << "#define tan_f16 tan\n"
                   << "#define atan_f16 atan\n"
                   << "#define atan2_f16 atan2\n"
                   << "#define sinh_f16 sinh\n"
                   << "#define asinh_f16 asinh\n"
                   << "#define cosh_f16 cosh\n"
                   << "#define acosh_f16 acosh\n"
                   << "#define tanh_f16 tanh\n"
                   << "#define atanh_f16 atanh\n";
    }

    if (target.has_feature(Target::CLAtomics64)) {
        src_stream << "#pragma OPENCL EXTENSION cl_khr_int64_base_atomics : enable\n";
        src_stream << "#pragma OPENCL EXTENSION cl_khr_int64_extended_atomics : enable\n";
    }

    src_stream << '\n';

    clc.add_common_macros(src_stream);

    if (!target.has_feature(Target::IntelFPGA)) {
        // Add at least one kernel to avoid errors on some implementations for functions
        // without any GPU schedules.
        src_stream << "__kernel void _at_least_one_kernel(int x) { }\n";
    }

    if (target.has_feature(Target::IntelFPGA)) {
        //enable channels support
        src_stream << "#pragma OPENCL EXTENSION cl_intel_channels : enable\n";
    }

    char *kernel_num = getenv("HL_KERNEL_NUM");
    if (overlay_kenrel == NULL && kernel_num != NULL) {
        int ip_num = std::atoi(kernel_num);
        src_stream << "#include \"ihc_apint.h\"\n";
        char *space_dim = getenv("HL_SPACE_DIM");
        char *overlay_dtype = getenv("HL_OVERLAY_DTYPE");
        src_stream << "#define DTYPE  " << string(overlay_dtype)
            << "  // default data type\n";

        src_stream << "#define K      " << ip_num << "   // number of IPs\n";
        src_stream << "#define M      " << std::atoi(space_dim) << "   // number of iter space var\n";
        src_stream << R"(#define N      16  // number of constant parameters
#define O      5   // max number of out degree
#define SIZE   16  // max number of tasks in the graph

// Define task index
typedef struct index {
    int     task_id;
    int     space_id[M];
} index_t;

// argument to tasks
typedef struct arg {
    __global DTYPE*  args0;
    __global DTYPE*  args1;
    __global DTYPE*  args2;
    __global DTYPE*  args4;
    __global DTYPE*  args5;
    DTYPE            constants[N];
    int              num_of_args;
    index_t          finish;
} arg_t;

// Task and dependency
typedef struct task {
    int         queue;
    index_t     index;
    index_t     deps[O];
    arg_t       inputs;
    int         num_of_deps;
    bool        done;
    bool        last;
} task_t;

// Task graph
typedef struct graph {
    uint16_t     slots;        // valid bit used for allocation
    uint16_t     issue;        // valid bit used for task issuance
    task_t       tasks[SIZE];  // pending tasks
    uint16_t     deps[SIZE];
} graph_t;

// Command queue from app to scheduler
channel task_t qt __attribute__((depth(64)));
channel int    qf __attribute__((depth(64)));

)";
        // Print req and ack channels
        src_stream << "channel arg_t q[" << ip_num << "] __attribute__((depth(64)));\n";
        src_stream << "channel index_t q_ret[" << ip_num << "] __attribute__((depth(64)));\n";
        src_stream << R"(
// Map from task to array index in the graph
int map( graph_t* graph, index_t key) {
    for (int index = 0; index < SIZE; index++) {
      index_t k = graph->tasks[index].index;
      if (k.task_id == key.task_id) {
        bool match = true;
        for (int i = 0; i < M; i++) {
            if (k.space_id[i] != key.space_id[i]) {
                match = false;
            }
        }
        if (match) {
            return index;
        }
      }
    }
    return -1;
}

// Allocate a task in the graph
void allocate(graph_t* graph, task_t task) {
   int index = 0;
   while (graph->slots & (1 << index)) {
       index++;
   }
   graph->slots = graph->slots | (1 << index);
   graph->tasks[index] = task;

   // set up dependency vector
   // use bitmap to determine the parents op index in the graph
   for (int i = 0; i < task.num_of_deps; i++) {
     int dep_op_index = map(graph, task.deps[i]);
     // break if the op has no dependency
     if (dep_op_index == -1) break;
     if (!graph->tasks[dep_op_index].done) {
       graph->deps[index] |= (1 << dep_op_index);
     }
   }
}

// Find out tasks with all deps satisified
bool dispatch(graph_t* graph, task_t* task) {
    for (int i = 0; i < SIZE; i++) {
        // valid task slot with no dependency
        if ((graph->slots & (1 << i)) && graph->deps[i] == 0x0000) {
            if (!(graph->issue & (1 << i))) {
              graph->issue |= (1 << i);
              *task = graph->tasks[i];
              return true;
            }
        }
    }
    return false;
}

// Update the graph
void update(graph_t* graph, index_t key) {
    int index = map(graph, key);
    // invalidate the slot
    graph->slots &= ~(1 << index);
    graph->issue &= ~(1 << index);

    // set done flag
    graph->tasks[index].done = true;

    // check depending children tasks
    for (int i = 0; i < SIZE; i++) {
      graph->deps[i] &= ~(1 << index);
    }
}

// Free-running scheduler
__attribute__((max_global_work_dim(0)))
__attribute__((autorun))
__kernel void scheduler() {

    // create task graph
    graph_t graph;
    graph.slots = 0x0000;
    graph.issue = 0x0000;

    int task_count = 0;
    bool task_not_end = true;

    // perform scheduling
    while(1) {

        while (task_not_end) {
            if (graph.slots == 0xFFFF) break;   // stop allocating when graph is full
            task_t task = read_channel_intel(qt);   // read task generated by the application

            if (task.last) {
                // printf("Received the last task....\n");
                task_not_end = false;
            } else {
                // printf("Received a new task....\n");
                allocate(&graph, task);
                task_count += 1;
            }
        }

        mem_fence(CLK_CHANNEL_MEM_FENCE);
        while (1) {
            task_t task;
            bool new_task = dispatch(&graph, &task);    // dispatch the pending tasks
            if (!new_task) break;

            switch (task.queue) {
)";
        for (int t = 0; t < ip_num; t++) {
            src_stream << string(16, ' ') << "case " << t << ": {\n";
            src_stream << string(20, ' ') << "write_channel_intel(q["
                << t << "], task.inputs);\n";
            src_stream << string(20, ' ') << "break;\n";
            src_stream << string(16, ' ') << "}\n";
        }
        src_stream << R"(
            }
        }

        // update the graph with ack information
        mem_fence(CLK_CHANNEL_MEM_FENCE);
        for (int i = 0; i < K; i++) {
            bool ret_valid = true;
            index_t ret;
            switch (i) {
)";
        for (int t = 0; t < ip_num; t++) {
            src_stream << string(16, ' ') << "case " << t
                << ": {\n";
            src_stream << string(20, ' ') << "ret = read_channel_nb_intel(q_ret["<< t <<"], &ret_valid); \n";
            src_stream << string(20, ' ') << "while (ret_valid) {\n";
            src_stream << string(24, ' ') << "update(&graph, ret);\n"
                       << string(24, ' ') << "ret = read_channel_nb_intel(q_ret["<< t <<"], &ret_valid); \n";
            src_stream << string(20, ' ') << "}\n"
                       << string(20, ' ') << "break;\n";
            src_stream << string(16, ' ') << "}\n";
        }
        src_stream << R"(
            }
        }

        // send the ending signal
        mem_fence(CLK_CHANNEL_MEM_FENCE);
        if (!task_not_end && graph.slots == 0x0000) {
            write_channel_intel(qf, task_count);
        }
    }
}

)";
        // Include the kernel ip functions
        char *overlay_kenrel_files = getenv("HL_OVERLAY_FILES");
        user_assert(overlay_kenrel_files != NULL) << "HL_OVERLAY_FILES empty...\n";

        string text(overlay_kenrel_files);
        std::size_t start = 0, end = 0;
        while ((end = text.find(" ", start)) != string::npos) {
        src_stream << "#include \"" << text.substr(start, end - start) << ".cl\"\n";
        start = end + 1;
        }
        src_stream << "#include \"" << text.substr(start) << ".cl\"\n\n";

    }

    cur_kernel_name = "";
}

vector<char> CodeGen_Clear_OneAPI_Dev::compile_to_src() {
    // compile_to_src is invoked in CodeGen_GPU_Host, but we avoid going through that route.
    internal_assert(false) << "Unreachable";
    return {};
}

string CodeGen_Clear_OneAPI_Dev::get_current_kernel_name() {
    return cur_kernel_name;
}

void CodeGen_Clear_OneAPI_Dev::dump() {
    std::cerr << src_stream.str() << std::endl;
}

string CodeGen_Clear_OneAPI_Dev::print_gpu_name(const string &name) {
    return name;
}

string CodeGen_Clear_OneAPI_Dev::compile(const Module &input) {
    // Initialize the module
    init_module();

    std::ostringstream EmitOneAPIFunc_stream;
    EmitOneAPIFunc em_visitor(&clc, EmitOneAPIFunc_stream, clc.get_target());

    // The Halide runtime headers, etc. into a separate file to make the main file cleaner
    std::ofstream runtime_file("halide_runtime_etc.h");
    runtime_file << em_visitor.get_str() + "\n";
    runtime_file << "void halide_device_and_host_free_as_destructor(void *user_context, void *obj) {\n";
    runtime_file << "}\n";

    // Clean the stream, and include both Halide and sycl runtime headers into a single header.
    em_visitor.clean_stream();
    em_visitor.write_runtime_headers();

    internal_assert(input.buffers().size() <= 0) << "OneAPI has no implementation to compile buffers at this time.\n";
    for (const auto &f : input.functions()) {
        em_visitor.print_global_data_structures_before_kernel(&f.body);
        em_visitor.gather_shift_regs_allocates(&f.body);
        em_visitor.compile(f);
    }

    string str = em_visitor.get_str() + "\n";
    return str;
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::print_global_data_structures_before_kernel(const Stmt *op) {
    // Define the compiler-generated vector and struct types.
    DefineVectorStructTypes def(this);
    def.mutate(*op);
    stream << def.vectors + def.structs;

    // Declare the output channels of the kernel.
    DeclareChannels decl(this);
    op->accept(&decl);
    stream << decl.channels;
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::DeclareChannels::visit(const Realize *op) {
    if (ends_with(op->name, ".channel") || ends_with(op->name, ".channel.array")) {
        // Get the bounds in which all bounds are for the dimensions of the channel array, except the last one is for the min depth.
        Region bounds = op->bounds;
        string bounds_str{};
        string pipe_bounds{};
        string pipe_attributes{};
        for (size_t i = 0; i < bounds.size(); i++) {
            Range b = bounds[i];
            Expr extent = b.extent;
            const IntImm *e_extent = extent.as<IntImm>();
            internal_assert(e_extent != nullptr);
            if (i < bounds.size() - 1) {
                internal_assert(e_extent->value > 0);
                bounds_str += "[" + std::to_string(e_extent->value) + "]";
                pipe_bounds += std::to_string(e_extent->value) + ",";
            } else {
                pipe_attributes = std::to_string(e_extent->value);
            }
        }
        if (pipe_bounds.back() == ',') pipe_bounds.pop_back();
        internal_assert(op->types.size() == 1) << "In generating Intel OpenCL for FPGAs, a single type is expected for a channel.\n";
        string type = parent->print_type(op->types[0]);

        std::ostringstream oss;
        if (ends_with(op->name, ".channel")) {
            parent->map_verbose_to_succinct_globally(op->name, parent->print_name(op->name));
            oss << "using " << parent->print_name(op->name)
                << " = pipe_wrapper<class "
                << parent->print_name(op->name) << "_pipe, " << type << ", "
                << pipe_attributes << (pipe_bounds.empty() ? pipe_bounds : ", " + pipe_bounds) << ">;\n";

            channels += oss.str();
        } else {
            // For a channel array, we will declare a global channel and a local channel array. For example, for this IR:
            //        realize F.channel.array[0, 2], [0, 256] of type `float32x4'
            // We will generate a global channel:
            //        typedef struct { float4 s[2]; } F_channel_array_t;
            //        channel F_channel_array_t F_channel __attribute__((depth(256)));
            // And then in function F, generate a local channel array:
            //        F_channel_array_t F_channel_array;
            // Here we add the global channel. We will add the local channel array separately.
            string stripped = remove_postfix(op->name, ".array");
            string channel_name = parent->print_name(stripped);
            parent->map_verbose_to_succinct_globally(stripped, channel_name);

            string printed_name = parent->print_name(op->name);
            string type_name = printed_name + "_t";
            oss << "struct " << type_name << " { " << type << " s" << bounds_str << "; };\n";
            oss << "using " << channel_name << " = pipe_wrapper<class "
                << channel_name << "_pipe, " << type_name << ", " << pipe_attributes <<  ">;\n";
            channels += oss.str();
        }
    }
    IRVisitor::visit(op);
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::DeclareArrays::visit(const Call *op) {
    if (op->is_intrinsic(Call::write_array)) {
        string channel_array_name = op->args[0].as<StringImm>()->value;
        string printed_name = parent->print_name(channel_array_name);
        parent->map_verbose_to_succinct_locally(channel_array_name, printed_name);

        string type_name = printed_name + "_t";
        if (array_set.find(printed_name) == array_set.end()) {
            array_set.insert(printed_name);
            arrays << parent->get_indent() << type_name << " " << printed_name << ";\n";
        }

       parent-> kernel_name_map.erase(channel_array_name);
    }
    return IRVisitor::visit(op);
}

/*
Check if shift reg has irregular bounds.
e.g.    realize shift regs [k, J-k] [0, K] [0, T]
          ...
            unrolled k = 0 to K
              unrolled j = k to J
                ...
The corresponding systolic array of above case is triangular in shape.
*/
bool CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::is_irregular(Region &bounds) {
    bool irregular_bounds = false;
    for (int i = bounds.size()-1; i >= 0; i--) {
        Expr extent = bounds[i].extent;
        if (!is_const(extent)) {
            irregular_bounds = true;
            break;
        }
    }
    return irregular_bounds;
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::gather_shift_regs_allocates(const Stmt *op) {
    GatherShiftRegsAllocates gatherer(this, shift_regs_allocates, shift_regs_bounds, space_vars);
    op->accept(&gatherer);
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::GatherShiftRegsAllocates::print_irregular_bounds_allocates(string reg_name, string type, string name, Region space_bounds, Region time_bounds, int space_bound_level) {
    Expr min = space_bounds[space_bound_level].min;
    Expr extent = space_bounds[space_bound_level].extent;
    const IntImm *e_min = min.as<IntImm>();
    const IntImm *e_extent = extent.as<IntImm>();
    internal_assert(e_min != NULL);
    internal_assert(e_extent != NULL);
    internal_assert(e_extent->value > 0);
    if (space_bound_level > 0) {
        Expr space_var = space_vars.at(reg_name)[space_bound_level];
        for (int i = 0; i < e_extent->value; i++) {
            int iter = e_min->value + i;
            string new_name = name + "_" + std::to_string(iter);
            Region new_space_bounds(space_bounds);

            for (int j = 0; j < space_bound_level; j++) {
                debug(4) << space_bounds[j].min << " " << space_bounds[j].extent <<"\n";
                new_space_bounds[j].min = simplify(substitute(space_var, iter, space_bounds[j].min));
                new_space_bounds[j].extent = simplify(substitute(space_var, iter, space_bounds[j].extent));
                debug(4) << new_space_bounds[j].min << " " << new_space_bounds[j].extent <<"\n";
            }
            print_irregular_bounds_allocates(reg_name, type, new_name, new_space_bounds, time_bounds, space_bound_level - 1);
        }
    } else {
        string bounds_str = "" ;
        // allocate time bounds as usual
        for (size_t i = 0; i < time_bounds.size(); i++) {
            Range b = time_bounds[i];
            Expr extent = b.extent;
            const IntImm *e_extent = extent.as<IntImm>();
            internal_assert(e_extent != NULL);
            internal_assert(e_extent->value > 0);
            bounds_str = bounds_str + "[" + std::to_string(e_extent->value) + "]";
        }

        for (int i = 0; i < e_extent->value; i++) {
            string new_name = name + "_" + std::to_string(e_min->value + i);
            parent->map_verbose_to_succinct_globally(new_name, parent->print_name(new_name)); // Record as a global variable name
            debug(3) << "allocate shift regs " << type << " " << new_name << bounds_str << ";\n";
            ostringstream rhs;
            rhs << type << " " << parent->print_name(new_name) << bounds_str << ";\n";
            shift_regs_allocates[reg_name].push_back(rhs.str());
        }

    }
}

// gather loop info from the annotation
void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::GatherShiftRegsAllocates::visit(const Call *op) {
    if (op->is_intrinsic(Call::annotate) && op->args[0].as<StringImm>()->value == "Bounds") {
        string reg_name = op->args[1].as<StringImm>()->value;
        vector<Expr> vars(op->args.begin() + 2, op->args.end());
        space_vars[reg_name] = vars;
    } else {
        IRVisitor::visit(op);
    }
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::GatherShiftRegsAllocates::visit(const Realize *op) {
    if (ends_with(op->name, ".channel") || ends_with(op->name, ".channel.array")) {
    } else if (ends_with(op->name, ".shreg")) {
        ostringstream rhs;
        Region bounds = op->bounds;
        string bounds_str = "" ;
        string type = parent->print_type(op->types[0]);

        if (parent->is_irregular(bounds)) {
            debug(3) << op->name << " has irregular bounds. \n";
            /*
            For irregular case, we physically unroll the space dimension of shift registers.
            e.g.
            realize shreg[k, 3-k][0, 3][0, 100] of type float32
            [k, 3-k] and [0, 3] are space dimensions.
            The generated code should be like this:
            float shreg_0_0[100]; float shreg_0_1[100]; float shreg_0_2[100];
                                  float shreg_1_1[100]; float shreg_1_2[100];
                                                        float shreg_2_2[100];
            */

            string reg_name = extract_first_token(op->name);

            internal_assert(space_vars.find(reg_name) != space_vars.end());
            size_t space_var_num = space_vars[reg_name].size();

            // for (auto var : space_vars[reg_name])
            //     debug(2) << "space var " << var << "\n";

            // Divide reg bounds into space bounds and time bounds.
            // We only unroll the space part.
            Region space_bounds, time_bounds;
            for (size_t i = 0; i < bounds.size(); i++) {
                if (i < space_var_num)
                    space_bounds.push_back(bounds[i]);
                else
                    time_bounds.push_back(bounds[i]);
            }

            // recursively add index as suffix of reg name
            print_irregular_bounds_allocates(reg_name, type, parent->print_name(op->name), space_bounds, time_bounds, space_bounds.size() - 1);

            // debug(3) << shift_regs_allocates[reg_name];

        } else {
            parent->map_verbose_to_succinct_globally(op->name, parent->print_name(op->name)); // Record as a global variable name
            for (int i = bounds.size()-1; i >= 0; i--) {
                Range b = bounds[i];
                Expr extent = b.extent;
                const IntImm *e_extent = extent.as<IntImm>();
                internal_assert(e_extent != NULL);
                internal_assert(e_extent->value > 0);
                bounds_str = "[" + std::to_string(e_extent->value) + "]" + bounds_str;
            }
            rhs << type << " " << parent->print_name(op->name) << bounds_str << ";\n";
            vector<string> names = split_string(op->name, ".");
            shift_regs_allocates[names[0]].push_back(rhs.str());
        }
        if (!parent->is_standard_opencl_type(op->types[0]) && op->types[0].is_vector()) {
            // Remember the bounds of the shift registers.
            shift_regs_bounds[op->name] = bounds.size();
        }
    } else if (ends_with(op->name, ".temp")) {
    } else if (ends_with(op->name, ".ibuffer")) {
    } else if (ends_with(op->name, ".break")) {
    } else {
        // Not really shift registers. But we can allocate them as shift regs as well.
        ostringstream rhs;
        Region bounds = op->bounds;
        string bounds_str = "" ;
        for (size_t i = 0; i < bounds.size(); i++) {
            Range b = bounds[i];
            Expr extent = b.extent;
            const IntImm *e_extent = extent.as<IntImm>();
            internal_assert(e_extent != NULL);
            internal_assert(e_extent->value > 0);
            bounds_str += "[" + std::to_string(e_extent->value) + "]";
        }
        string type = parent->print_type(op->types[0]);
        rhs << type << " " << parent->print_name(op->name) << bounds_str << ";\n";
        vector<string> names = split_string(op->name, ".");
        shift_regs_allocates[names[0]].push_back(rhs.str());
        if (!parent->is_standard_opencl_type(op->types[0]) && op->types[0].is_vector()) {
            // Remember the bounds of the shift registers.
            shift_regs_bounds[op->name] = bounds.size();
        }
    }
    IRVisitor::visit(op);
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::visit(const Realize *op) {
    if (ends_with(op->name, ".channel") || ends_with(op->name, ".channel.array")) {
        // We have already declared the channel before the kernel with print_global_data_structures_before_kernel().
        // Just skip it and get into the body.
        print_stmt(op->body);
    } else if (ends_with(op->name, ".shreg")) {
        // We have already gathered shift regs allocates with gather_shift_regs_allocates().
        // Just skip it and get into the body.
        print_stmt(op->body);
    } else if (ends_with(op->name, ".temp")) {
        string stripped = remove_postfix(op->name, ".temp");
        map_verbose_to_succinct_locally(op->name, CodeGen_Clear_C::print_name(stripped));

        string string_bound = "" ;
        vector<string> access_exprs;
        for (Range b : op->bounds) {
            access_exprs.push_back(print_expr(b.extent));
        }
        for (string ae : access_exprs) {
            string_bound += "[" + ae + "]";
        }
        for (size_t i = 0; i<op->types.size(); i++) {
            // do_indent();
            string name = op->name;
            stream << get_indent() << print_type(op->types[i]) << " " << print_name(name)
                << string_bound << ";\n";
        }
        print_stmt(op->body);

        // The Realize scope ends and its name is dead
        kernel_name_map.erase(op->name);
    } else if (ends_with(op->name, ".ibuffer")) {
        string string_bound = "" ;
        vector<string> access_exprs;
        for (Range b : op->bounds) {
            access_exprs.push_back(print_expr(b.extent));
        }
        for (string ae : access_exprs) {
            string_bound += "[" + ae + "]";
        }
        bool multi_values = op->types.size() > 1;
        for (size_t i = 0; i < op->types.size(); i++) {
            string stripped = remove_postfix(op->name, ".ibuffer");
            stripped = extract_after_tokens(stripped, 1); // Remove the function prefix
            if (multi_values) {
                stripped += '.' + std::to_string(i);
            }
            string buffer_name = op->name + '.' + std::to_string(i);
            map_verbose_to_succinct_locally(buffer_name, print_name(stripped));
            stream << get_indent() << "// OpenCL's __attribute__((memory, numbanks("
                   << access_exprs.back()
                   << "), singlepump, numwriteports(1), numreadports(1)))"
                   << print_name(buffer_name) << string_bound << "\n";
            stream << get_indent() << "[[intel::fpga_memory(), "
                   << "intel::numbanks(" << access_exprs.back() << "), "
                   << "intel::singlepump, intel::simple_dual_port]]\n"
                   << get_indent() << print_type(op->types[i])  << " "
                   << print_name(buffer_name) << string_bound << ";\n";
        }
        print_stmt(op->body);

        // The Realize scope ends and its name is dead
        for (size_t i = 0; i < op->types.size(); i++) {
            string buffer_name = op->name + '.' + std::to_string(i);
            kernel_name_map.erase(buffer_name);
        }
    } else if (ends_with(op->name, ".break")) {
        print_stmt(op->body);
    } else {
        // We have treated this case as shift regs allocates with gather_shift_regs_allocates().
        // Just skip it and get into the body.
        print_stmt(op->body);
    }
}

void CodeGen_Clear_OneAPI_Dev::CodeGen_Clear_OneAPI_C::visit(const Provide *op) {
    if (ends_with(op->name, ".ibuffer")) {
        internal_assert(op->values.size() == 1);
        string id_value = print_expr(op->values[0]);
        vector<string> access_exprs;
        for (size_t i = 0; i < op->args.size(); i++) {
            access_exprs.push_back(print_expr(op->args[i]));
        }
        string buffer_name = op->name + '.' + std::to_string(0);
        stream << get_indent() << print_name(buffer_name);
        for (size_t i = 0; i < op->args.size(); i++) {
            stream << "[";
            stream << access_exprs[i];
            stream << "]";
        }
        stream << " = " << id_value << ";\n";
    } else if (ends_with(op->name, ".temp")) {
        internal_assert(op->values.size() == 1);
        string id_value = print_expr(op->values[0]);
        string name = op->name;
        vector<string> access_exprs;
        for (size_t i = 0; i < op->args.size(); i++) {
            access_exprs.push_back(print_expr(op->args[i]));
        }
        stream << get_indent() << print_name(name);
        // do_indent();
        for (size_t i = 0; i < op->args.size(); i++) {
            stream << "[";
            stream << access_exprs[i];
            stream << "]";
        }
        stream << " = " << id_value ;
        stream<< ";\n";
    }else if (ends_with(op->name, ".break")) {
        stream<<"break;\n";
    }
    else {
        CodeGen_Clear_C::visit(op);
    }
}

void CodeGen_Clear_OneAPI_Dev::EmitOneAPIFunc::ExternCallFuncs::check_valid(const Call *op, unsigned int max_args, unsigned int min_args) {
    internal_assert(op);
    internal_assert(p);
    internal_assert(op->args.size() <= max_args && op->args.size() >= min_args);
}

string CodeGen_Clear_OneAPI_Dev::EmitOneAPIFunc::ExternCallFuncs::halide_device_malloc(const Call *op) {
    check_valid(op, 2, 1);
    std::ostringstream rhs;
    vector<Expr> args = op->args;
    string buff = p->print_expr(args[0]);

    rhs << "if (!" << buff << "->device) { // device malloc\n";
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

string CodeGen_Clear_OneAPI_Dev::EmitOneAPIFunc::ExternCallFuncs::halide_host_malloc(const Call *op) {
    check_valid(op, 2, 1);
    std::ostringstream rhs;
    vector<Expr> args = op->args;
    string buffer_name = p->print_expr(args[0]);
    rhs << "{ // host malloc\n";
    rhs << p->get_indent() << "  std::cout << \"//\\t host malloc "<< buffer_name << "\\n\";\n";
    rhs << p->get_indent() << "  assert(" << buffer_name << "->size_in_bytes() != 0);\n";
    rhs << p->get_indent() << "  " << buffer_name << "->host = (uint8_t*)std::malloc(" << buffer_name << "->size_in_bytes());\n";
    rhs << p->get_indent() << "  assert(" << buffer_name << "->host != NULL);\n";
    rhs << p->get_indent() << "}";
    return rhs.str();
}

string CodeGen_Clear_OneAPI_Dev::EmitOneAPIFunc::ExternCallFuncs::halide_copy_to_device(const Call *op) {
    check_valid(op, 2, 1);
    std::ostringstream rhs;
    rhs << halide_opencl_buffer_copy(op, false);
    return rhs.str();
}

string CodeGen_Clear_OneAPI_Dev::EmitOneAPIFunc::ExternCallFuncs::halide_copy_to_host(const Call *op) {
    check_valid(op, 2, 1);
    std::ostringstream rhs;
    rhs << halide_opencl_buffer_copy(op, true);
    return rhs.str();
}

string CodeGen_Clear_OneAPI_Dev::EmitOneAPIFunc::ExternCallFuncs::create_memcpy(string buffer_name, bool to_host) {
    std::ostringstream rhs;
    string dst;
    string src;
    if (to_host) {
        dst = buffer_name + "->host";
        src = "(((device_handle*)" + buffer_name + "->device)->mem)";
    } else {
        dst = "(((device_handle*)" + buffer_name + "->device)->mem)";
        src = buffer_name + "->host";
    }
    rhs << "q_device.submit([&](handler& h){ h.memcpy("
        << "(void *)" << dst << ", " // dst
        << "(void *)" << src << ", " // src
        << buffer_name << "->size_in_bytes()); }).wait()"; // size
    return rhs.str();
}

string CodeGen_Clear_OneAPI_Dev::EmitOneAPIFunc::ExternCallFuncs::halide_opencl_buffer_copy(const Call* op, bool to_host) {
    // Implemented based on AOT-OpenCL-Runtime.cpp
    check_valid(op, 2, 1);
    std::ostringstream rhs;
    vector<Expr> args = op->args;
    string buffer_name = p->print_expr(args[0]);

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
    rhs << p->get_indent() << "  } else {\n";
    rhs << p->get_indent() << "    std::cout << \"//\t memcpy "<< buffer_name << " Do nothing.\\n\";\n";
    rhs << p->get_indent() << "  }\n";
    rhs << p->get_indent() << "}";
    return rhs.str();
}

string CodeGen_Clear_OneAPI_Dev::EmitOneAPIFunc::ExternCallFuncs::halide_device_and_host_free(const Call *op) {
    check_valid(op, 2, 1);
    std::ostringstream rhs;
    std::vector<Expr> args = op->args;
    std::string buffer_name = p->print_expr(args[0]);
    rhs << halide_device_host_nop_free(buffer_name);
    return rhs.str();
}

string CodeGen_Clear_OneAPI_Dev::EmitOneAPIFunc::ExternCallFuncs::halide_device_and_host_malloc(const Call *op) {
    check_valid(op, 2, 1);
    std::ostringstream rhs;
    // Device Malloc
    rhs << halide_device_malloc(op);
    rhs << ";\n";
    // Host Malloc
    rhs << p->get_indent() << halide_host_malloc(op);
    return rhs.str();
}

string CodeGen_Clear_OneAPI_Dev::EmitOneAPIFunc::ExternCallFuncs::halide_opencl_wait_for_kernels_finish(const Call *op) {
    check_valid(op, 0, 0);
    std::ostringstream rhs;
    // Wait for all kernels to finish
    rhs << "oneapi_kernel_events.back().wait();";
    return rhs.str();
}


string CodeGen_Clear_OneAPI_Dev::EmitOneAPIFunc::ExternCallFuncs::halide_device_host_nop_free(string buffer_name) {
    internal_assert(p);
    std::ostringstream rhs;

    // Free device
    rhs << "if (" << buffer_name << "->device) { // device free\n";
    rhs << p->get_indent() << "  sycl::free(((device_handle*)" << buffer_name << "->device)->mem , q_device);\n";
    rhs << p->get_indent() << "  assert(((device_handle *)" << buffer_name << "->device)->offset == 0);\n";
    rhs << p->get_indent() << "  std::free((device_handle *)" << buffer_name << "->device);\n";
    rhs << p->get_indent() << "  "<< buffer_name <<"->set_device_dirty(false);\n";
    rhs << p->get_indent() << "}\n";

    // Free host
    rhs << p->get_indent() << "if (" << buffer_name << "->host) { // host free\n";
    rhs << p->get_indent() << "  std::free((void*)"<< buffer_name <<"->host);\n";
    rhs << p->get_indent() << "  "<< buffer_name <<"->host = NULL;\n";
    rhs << p->get_indent() << "  "<< buffer_name <<"->set_host_dirty(false);\n";
    rhs << p->get_indent() << "}";
    return rhs.str();
}

void CodeGen_Clear_OneAPI_Dev::EmitOneAPIFunc::create_assertion(const string &id_cond, const string &id_msg) {
    // (NOTE) Implementation mimic CodeGen_C::create_assertion()
    if (target.has_feature(Target::NoAsserts)) return;

    stream << get_indent() << "if (!" << id_cond << ")\n";
    open_scope();
    stream << get_indent() << "std::cout << \"Condition '" <<  id_cond << "' failed "
           << "with error id_msg: " << id_msg
           << "\\n\";\n";
    stream << get_indent() << "assert(false);\n";
    close_scope("");
}

void CodeGen_Clear_OneAPI_Dev::EmitOneAPIFunc::clean_stream() {
    stream_ptr->str("");
    stream_ptr->clear();
}

void CodeGen_Clear_OneAPI_Dev::EmitOneAPIFunc::write_runtime_headers() {
    stream << 
R"(#include "halide_runtime_etc.h"
#if __has_include(<sycl/sycl.hpp>)
#include <sycl/sycl.hpp>
#else
#include <CL/sycl.hpp>
#endif
#include <sycl/ext/intel/fpga_extensions.hpp>
using namespace sycl;
struct device_handle {
    // Important: order these to avoid any padding between fields;
    // some Win32 compiler optimizer configurations can inconsistently
    // insert padding otherwise.
    uint64_t offset;
    void* mem;
};

template <typename name, typename data_type, int32_t min_capacity, int32_t... dims>
struct pipe_wrapper {
    template <int32_t...> struct unique_id;
    template <int32_t... idxs>
    static data_type read() {
        static_assert(((idxs >= 0) && ...), "Negative index");
        static_assert(((idxs < dims) && ...), "Index out of bounds");
        return sycl::ext::intel::pipe<unique_id<idxs...>, data_type, min_capacity>::read();
    }
    template <int32_t... idxs>
    static void write(const data_type &t) {
        static_assert(((idxs >= 0) && ...), "Negative index");
        static_assert(((idxs < dims) && ...), "Index out of bounds");
        sycl::ext::intel::pipe<unique_id<idxs...>, data_type, min_capacity>::write(t);
    }
};

void exception_handler(sycl::exception_list exceptions) {
    for (std::exception_ptr const &e : exceptions) {
        try {
            std::rethrow_exception(e);
        } catch (sycl::exception const &e) {
            std::cout << "Caught asynchronous SYCL exception:\n"
                      << e.what() << std::endl;
        }
    }
}

// The SYCL 1.2.1 device_selector class is deprecated in SYCL 2020.
// Use the callable selector object instead.
#if SYCL_LANGUAGE_VERSION >= 202001
using device_selector_t = int(*)(const sycl::device&);
#else
using device_selector_t = const sycl::device_selector &;
#endif

)";
}

string CodeGen_Clear_OneAPI_Dev::EmitOneAPIFunc::get_str() {
    std::set<string>::iterator it = UnImplementedExternFuncs.begin();
    while (it != UnImplementedExternFuncs.end()) {
        // Output all the unimplemented external functions that have been called but not caught
        stream << get_indent() << "// " << (*it) << " to-be-implemented\n";
        it++;
    }

    string str = stream_ptr->str();
    return str;
};

// EmitOneAPIFunc
void CodeGen_Clear_OneAPI_Dev::EmitOneAPIFunc::compile(const LoweredFunc &f) {

    // (NOTE) Mimicing exection from CodeGen_C.cpp's
    // "void CodeGen_C::compile(const LoweredFunc &f)"

    // Linkage does not matter as we are making a stand alone file
    const std::vector<LoweredArgument> &args = f.args;

    have_user_context = false;
    for (size_t i = 0; i < args.size(); i++) {
        // TODO: check that its type is void *?
        have_user_context |= (args[i].name == "__user_context");
    }

    // OneAPI is compiled wiht DPC++ so we should always be using C++ NameMangling
    NameMangling name_mangling = f.name_mangling;
    internal_assert(target.has_feature(Target::CPlusPlusMangling));
    if (name_mangling == NameMangling::Default) {
        name_mangling = NameMangling::CPlusPlus;
    }

    set_name_mangling_mode(name_mangling);

    std::vector<std::string> namespaces;
    std::string simple_name = extract_namespaces(f.name, namespaces);

    // Print out namespace begining
    if (!namespaces.empty()) {
        for (const auto &ns : namespaces) {
            stream << "namespace " << ns << " {\n";
        }
        stream << "\n";
    }

    // Emit the function prototype
    if (f.linkage == LinkageType::Internal) {
        // If the function isn't public, mark it static.
        stream << "static ";
    }

    // (TODO) Check that HALIDE_FUNCTION_ATTRS is necessary or not
    // stream << "HALIDE_FUNCTION_ATTRS\n";
    stream << "auto " << simple_name << "(device_selector_t device_selector_v";
    if (args.size() > 0) stream << ", ";
    for (size_t i = 0; i < args.size(); i++) {
        if (args[i].is_buffer()) {
            stream << "struct halide_buffer_t *"
                   << print_name(args[i].name)
                   << "_buffer";
        } else {
            stream << print_type(args[i].type, AppendSpace)
                   << print_name(args[i].name);
        }

        if (i < args.size() - 1) stream << ", ";
    }


    // Begin of function implementation
    stream << ") {\n";
    indent += 2;

    // Initalize elements for kernel such as sycl event's vector
    stream << get_indent() << "std::vector<sycl::event> oneapi_kernel_events{};\n";
    stream << get_indent() << "std::vector<size_t> kernels_used_to_measure_time{};\n";
    stream << get_indent() << "std::cout << \"// creating device queues\\n\";\n";
    stream << get_indent() << "sycl::queue q_host(sycl::cpu_selector_v, exception_handler, sycl::property::queue::enable_profiling());\n";
    stream << get_indent() << "sycl::queue q_device(device_selector_v, exception_handler, sycl::property::queue::enable_profiling());\n";
    stream << get_indent() << "std::cout << \"// Host: \" << q_host.get_device().get_info<sycl::info::device::name>() << \"\\n\";\n";
    stream << get_indent() << "std::cout << \"// Device: \" << q_device.get_device().get_info<sycl::info::device::name>() << \"\\n\";\n";
    stream << get_indent() << "sycl::device dev = q_device.get_device();\n";


    // Emit a local user_context we can pass in all cases, either
    // aliasing __user_context or nullptr.
    stream << get_indent() << "void * const _ucon = "
            << (have_user_context ? "const_cast<void *>(__user_context)" : "nullptr")
            << ";\n";

    // Emit the body
    print(f.body);

    // Make sure all kernels are finished
    stream << get_indent() << "oneapi_kernel_events.back().wait();\n";


    // Return execution time of the kernels.
    stream << get_indent() << "std::cout << \"// return the kernel execution time in nanoseconds\\n\";\n";
    stream << get_indent() << "auto k_earliest_start_time = std::numeric_limits<\n"
           << get_indent() << "  typename sycl::info::event_profiling::command_start::return_type>::max();\n"
           << get_indent() << "auto k_latest_end_time = std::numeric_limits<\n"
           << get_indent() << "  typename sycl::info::event_profiling::command_end::return_type>::min();\n"
           << get_indent() << "for (auto i : kernels_used_to_measure_time) {\n"
           << get_indent() << "  auto tmp_start = oneapi_kernel_events[i].get_profiling_info<sycl::info::event_profiling::command_start>();\n"
           << get_indent() << "  auto tmp_end = oneapi_kernel_events[i].get_profiling_info<sycl::info::event_profiling::command_end>();\n"
           << get_indent() << "  if (tmp_start < k_earliest_start_time) {\n"
           << get_indent() << "    k_earliest_start_time = tmp_start;\n"
           << get_indent() << "  }\n"
           << get_indent() << "  if (tmp_end > k_latest_end_time) {\n"
           << get_indent() << "    k_latest_end_time = tmp_end;\n"
           << get_indent() << "  }\n"
           << get_indent() << "}\n"
           << get_indent() << "// Get time in ns\n"
           << get_indent() << "return kernels_used_to_measure_time.empty() ? decltype(k_latest_end_time){} : k_latest_end_time - k_earliest_start_time;\n";



    indent -= 2;
    stream << "}\n";
    // Ending of function implementation

    // (TODO) Check that HALIDE_FUNCTION_ATTRS is necessary or not
    // if (is_header_or_extern_decl() && f.linkage == LinkageType::ExternalPlusMetadata) {
    //     // Emit the argv version
    //     stream << "\nHALIDE_FUNCTION_ATTRS\nint " << simple_name << "_argv(void **args);\n";
    //     // And also the metadata.
    //     stream << "\nHALIDE_FUNCTION_ATTRS\nconst struct halide_filter_metadata_t *" << simple_name << "_metadata();\n";
    // }

    if (!namespaces.empty()) {
        stream << "\n";
        for (size_t i = namespaces.size(); i > 0; i--) {
            stream << "}  // namespace " << namespaces[i - 1] << "\n";
        }
        stream << "\n";
    }

}


// EmitOneAPIFunc
std::string CodeGen_Clear_OneAPI_Dev::EmitOneAPIFunc::create_kernel_name(const For *op) {
    // Remove already useless info from the loop name, so as to get a cleaner kernel name.
    std::string loop_name = op->name;
    std::string func_name = extract_first_token(loop_name);
    std::string kernel_name;
    {
        using namespace llvm;
        kernel_name = unique_name("kernel_" + func_name);
    }

    // If the kernel writes to memory, append "_WAIT_FINISH" so that the OpenCL runtime knows to wait for this
    // kernel to finish.
    // KernelStoresToMemory checker;
    // op->body.accept(&checker);
    // if (checker.stores_to_memory) {
    //     // TOFIX: overlay does not work well with this change of name
    //     // kernel_name += "_WAIT_FINISH";
    // }

    for (size_t i = 0; i < kernel_name.size(); i++) {
        if (!isalnum(kernel_name[i])) {
            kernel_name[i] = '_';
        }
    }
    return kernel_name;
}


void CodeGen_Clear_OneAPI_Dev::EmitOneAPIFunc::create_kernel_wrapper(const std::string &name, std::string q_device_name, const std::vector<DeviceArgument> &args, bool begining, bool is_run_on_device) {
    // (TODO), will need a function wrapper if user wants to ever
    // use the compile_to_devsrc to generate the kernels alone


    if (begining) {
        stream << get_indent() << "// " << name << "\n";
        stream << get_indent() << "std::cout << \"// kernel " << name << "\\n\";\n";

        // convert any pointers we need to get the device memory allocated
        for (size_t i = 0; i < args.size(); i++) {
            auto &arg = args[i];
            if (arg.is_buffer) {
                if (allocations.contains(arg.name) && is_run_on_device) {
                    stream << get_indent() << print_name(arg.name) << " = (" << print_type(arg.type) << "*)(((device_handle*) " << print_name(arg.name + ".buffer") << "->device)->mem);\n";
                } else if (!allocations.contains(arg.name) && is_run_on_device) {
                    stream << get_indent() << print_type(arg.type) << " *" << print_name(arg.name) << " = (" << print_type(arg.type) << "*)(((device_handle*) " << print_name(arg.name + ".buffer") << "->device)->mem);\n";
                } else if (allocations.contains(arg.name) && !is_run_on_device) {
                    stream << get_indent() << print_name(arg.name) << " = (" << print_type(arg.type) << "*)(" << print_name(arg.name + ".buffer") << "->host);\n";
                } else if (!allocations.contains(arg.name) && !is_run_on_device) {
                    stream << get_indent() << print_type(arg.type) << " *" << print_name(arg.name) << " = (" << print_type(arg.type) << "*)(" << print_name(arg.name + ".buffer") << "->host);\n";
                } else {
                    internal_assert(false) << "OneAPI cannot determin mapping of `" << print_type(arg.type) << "` to `" << print_name(arg.name + ".buffer") << "\n";
                }
            }

        }

        if (is_run_on_device) {
            stream << get_indent() << "oneapi_kernel_events.push_back(" << q_device_name << ".submit([&](sycl::handler &h){\n";
            indent += 2;
            stream << get_indent() << "h.single_task<class " + name + "_class>([=](){\n";
            indent += 2;
        } else {
            open_scope();
        }

    } else {
        if (is_run_on_device) {
            indent -= 2;
            stream << get_indent() << "}); //  h.single_task " << name + "_class\n";
            indent -= 2;
            stream << get_indent() << "})); // "<< q_device_name << ".submit\n";
        } else {
            close_scope("// kernel_" + name);
        }
    }
}


// Check if this is a piece of code for an run_on_device kernel
class IsRunOnDevice : public IRVisitor {
    using IRVisitor::visit;
    void visit(const For *op) override {
        if (ends_with(op->name, ".run_on_device")) {
            is_run_on_device = true;
            return;
        }
        IRVisitor::visit(op);
    }
    public:
        bool is_run_on_device;
        IsRunOnDevice() : is_run_on_device(false) {}
};

void CodeGen_Clear_OneAPI_Dev::EmitOneAPIFunc::add_kernel(Stmt s,
                                                      const string &name,
                                                      const vector<DeviceArgument> &args) {


    currently_inside_kernel = true;

    // OneAPI combines host and device code.
    // Check if we need to wrapp the Stmt in a q.submit of which type
    // one implementaiton for host, one for device

    // (TODO) Remove once safe
    // Store the kernel information for a kernel wrapped function in compile_to_oneapi
    // Only add device kernels to kernel_args
    kernel_args.push_back(std::tuple<Stmt, std::string, std::vector<DeviceArgument>>(s, name, args));

    IsRunOnDevice device_run_checker;
    s.accept(&device_run_checker);

    IsAutorun autorun_checker{};
    s.accept(&autorun_checker);

    if (device_run_checker.is_run_on_device && !autorun_checker.is_autorun) {
        stream << get_indent() << "kernels_used_to_measure_time.push_back(oneapi_kernel_events.size());\n";
    }

    std::string queue_name = device_run_checker.is_run_on_device ? "q_device"  : "q_host";
    debug(2) << "Adding OneAPI kernel " << name << "\n";

    // create kernel wrapper top half
    create_kernel_wrapper(name, queue_name, args, true, device_run_checker.is_run_on_device);

    // Output q.submit kernel code
    DeclareArrays da(this);
    s.accept(&da);
    stream << da.arrays.str();
    print(s);

    // create kernel wrapper btm half
    create_kernel_wrapper(name, queue_name, args, false, device_run_checker.is_run_on_device);


    currently_inside_kernel = false;
    return;

    { // Previous
        if (!device_run_checker.is_run_on_device) {
            debug(2) << "Adding OneAPI Host kernel " << name << "\n";

            // create kernel wrapper top half
            create_kernel_wrapper(name, "q_host", args, true, device_run_checker.is_run_on_device);

            // Output q.submit kernel code
            DeclareArrays da(this);
            s.accept(&da);
            stream << da.arrays.str();
            print(s);

            // create kernel wrapper btm half
            create_kernel_wrapper(name, "q_host", args, false, device_run_checker.is_run_on_device);

        }
        // Emmit Device Code
        else {
            debug(2) << "Adding OneAPI Device kernel " << name << "\n";

            // create kernel wrapper top half
            create_kernel_wrapper(name, "q_device", args, true, device_run_checker.is_run_on_device);

            // Output q.submit kernel code
            DeclareArrays da(this);
            s.accept(&da);
            stream << da.arrays.str();
            print(s);

            // create kernel wrapper btm half
            create_kernel_wrapper(name, "q_device", args, false, device_run_checker.is_run_on_device);

        }
    }


}



// EmitOneAPIFunc
void CodeGen_Clear_OneAPI_Dev::EmitOneAPIFunc::visit(const For *loop) {
    debug(3) << "//!!! EmitOneAPIFunc::visit(const For *loop) FOR LOOP NAME << " << loop->name << "\n";


    // The currently_inside_kernel is set to true/false inside the
    // ::add_kerel() function. This prevennts the CodeGen from adding recursively calling it forever
    if (currently_inside_kernel) {
        debug(3) << "//!!! EmitOneAPIFunc::visit(const For *loop) FOUND << " << loop->name << "\n";
        // CodeGen_OneAPI_C::visit(loop); // Coppied // Call parent implementaiton of For* loop to print
        {
                if (is_gpu_var(loop->name)) {
                    internal_assert((loop->for_type == ForType::GPUBlock) ||
                                    (loop->for_type == ForType::GPUThread))
                        << "kernel loop must be either gpu block or gpu thread\n";
                    internal_assert(is_zero(loop->min));

                    stream << get_indent() << print_type(Int(32)) << " " << print_name(loop->name)
                        << " = " << simt_intrinsic(loop->name) << ";\n";

                    loop->body.accept(this);

                } else if (ends_with(loop->name, ".run_on_device")) {
                    // The loop just tells the compiler to generate an OpenCL kernel for
                    // the loop body.
                    std::string name = extract_first_token(loop->name);
                    if (shift_regs_allocates.find(name) != shift_regs_allocates.end()) {
                        for (size_t i = 0; i < shift_regs_allocates[name].size(); i++) {
                            stream << get_indent() << shift_regs_allocates[name][i];
                        }
                    }
                    name += "_temp";
                    if (shift_regs_allocates.find(name) != shift_regs_allocates.end()) {
                        for (size_t i = 0; i < shift_regs_allocates[name].size(); i++) {
                            stream << get_indent() << shift_regs_allocates[name][i];
                        }
                    }
                    loop->body.accept(this);
                } else if (ends_with(loop->name, ".infinite")) {
                    stream << get_indent() << "while(1)\n";
                    open_scope();
                    loop->body.accept(this);
                    close_scope("while "+print_name(loop->name));
                } else if (ends_with(loop->name, "remove")) {
                    loop->body.accept(this);
                } else if (loop->for_type == ForType::DelayUnroll) {
                    Expr extent = simplify(loop->extent);
                    Stmt body = loop->body;
                    const IntImm *e = extent.as<IntImm>();
                    user_assert(e)
                            << "Can only unroll for loops over a constant extent.\n"
                            << "Loop over " << loop->name << " has extent " << extent << ".\n";
                    if (e->value == 1) {
                        user_warning << "Warning: Unrolling a for loop of extent 1: " << loop->name << "\n";
                    }

                    Stmt iters;
                    for (int i = 0; i < e->value; i++) {
                        Stmt iter = simplify(substitute(loop->name, loop->min + i, body));
                        iter.accept(this);
                    }
                } else if (loop->for_type == ForType::PragmaUnrolled) {
                    stream << get_indent() << "#pragma unroll\n";
                    CodeGen_Clear_C::visit(loop);
                } else {
                    /*
                    If not explicitly define a env variable(DELAYUNROLL or PRAGMAUNROLL), the unrolling strategy will be automatically
                    decided by the compiler.
                    To physically unroll a loop, we require that:
                        1. there is a conditional execution of channel read/write inside the current loop, e.g.
                            unrolled k = 0 to K
                                if (cond)
                                    read/write channel
                        2. there is a irregular loop inside the current loop and the irregular bound depends on current loop var, e.g.
                            unrolled k = 0 to K
                                unrolled j = k to J
                        3. there is a shift register whose bounds depends on current loop var, e.g.,
                            float _Z_shreg_0, _Z_shreg_1, _Z_shreg_2, _Z_shreg_3;
                            unrolled j = 0 to J
                                access _Z_shreg_j   // j needs to be replaced with 0, 1, 2, 3

                    For other cases, we simply insert the #pragma unroll directive before a loop. The offline compiler attempts to fully
                    unroll the loop.
                    */
                    user_assert(loop->for_type != ForType::Parallel) << "Cannot use parallel loops inside OpenCL kernel\n";
                    if (loop->for_type == ForType::Unrolled) {
                        CheckConditionalChannelAccess checker(this, loop->name);
                        loop->body.accept(&checker);

                        // Check the condition 1 and 2
                        bool needs_unrolling = checker.conditional_access || checker.irregular_loop_dep;
                        // Check the condition 3
                        for (auto &kv : space_vars) {
                            for (auto &v : kv.second) {
                                if (v.as<Variable>()->name == loop->name)
                                    needs_unrolling |= true;
                            }
                        }
                        if (needs_unrolling) {
                            Expr extent = simplify(loop->extent);
                            Stmt body = loop->body;
                            const IntImm *e = extent.as<IntImm>();
                            user_assert(e)
                                    << "Can only unroll for loops over a constant extent.\n"
                                    << "Loop over " << loop->name << " has extent " << extent << ".\n";
                            if (e->value == 1) {
                                user_warning << "Warning: Unrolling a for loop of extent 1: " << loop->name << "\n";
                            }

                            Stmt iters;
                            for (int i = 0; i < e->value; i++) {
                                Stmt iter = simplify(substitute(loop->name, loop->min + i, body));
                                iter.accept(this);
                            }
                        } else {
                            stream << get_indent() << "#pragma unroll\n";
                            CodeGen_Clear_C::visit(loop);
                        }
                    } else {
                        CodeGen_Clear_C::visit(loop);
                    }
                }
        }
        return;
    }




    debug(3) << "//!!! EmitOneAPIFunc::visit(const For *loop) FIRST TIME << " << loop->name << "\n";


    // (NOTE) implementation derived from CodeGen_GPU_Host<CodeGen_CPU>::visit(const For *loop)
    // std::string kernel_name = create_kernel_name(loop);
    // (TODO) Need to confirm this with the create_kernel_wrapper with the proper arguments needed
    std::string kernel_name;
    vector<DeviceArgument> closure_args;
    {
        using namespace llvm;
        kernel_name = create_kernel_name(loop);

        // std::string kernel_name = "Kernel_" + loop->name;
        // compute a closure over the state passed into the kernel
        HostClosure c(loop->body, loop->name);
        // Determine the arguments that must be passed into the halide function
        closure_args = c.arguments();
        // Sort the args by the size of the underlying type. This is
        // helpful for avoiding struct-packing ambiguities in metal,
        // which passes the scalar args as a struct.
        std::sort(closure_args.begin(), closure_args.end(),
                    [](const DeviceArgument &a, const DeviceArgument &b) {
                        if (a.is_buffer == b.is_buffer) {
                            return a.type.bits() > b.type.bits();
                        } else {
                            return a.is_buffer < b.is_buffer;
                        }
                    });


        // (TODO) Verify that not setting the closure arg size will not effect generated code
        // for (size_t i = 0; i < closure_args.size(); i++) {
        //     if (closure_args[i].is_buffer && allocations.contains(closure_args[i].name)) {
        //         closure_args[i].size = allocations.get(closure_args[i].name).constant_bytes;
        //     }
        // }
    }

    add_kernel(loop, kernel_name, closure_args);
}

// EmitOneAPIFunc
void CodeGen_Clear_OneAPI_Dev::EmitOneAPIFunc::visit(const Allocate *op) {
    // (TODO) Need error testing when we implement compile_to_devsrc()
    // user_assert(!op->new_expr.defined()) << "Allocate node inside OpenCL kernel has custom new expression.\n"
    //                                      << "(Memoization is not supported inside GPU kernels at present.)\n";


    // New C Implementaiton 1.0
    {
        open_scope();

        std::string op_name = print_name(op->name);
        std::string op_type = print_type(op->type, AppendSpace);

        // For sizes less than 8k, do a stack allocation
        bool on_stack = false;
        int32_t constant_size;
        std::string size_id;
        Type size_id_type;
        std::string free_function;


        if (op->new_expr.defined()) {
            Allocation alloc;
            alloc.type = op->type;
            allocations.push(op->name, alloc);
            heap_allocations.push(op->name);
            string value = print_expr(op->new_expr);
            stream << get_indent() << op_type << "*" << op_name << " = (" << op_type << "*)(" << value << ");\n";
        } else {
            constant_size = op->constant_allocation_size();
            if (constant_size > 0) {
                int64_t stack_bytes = constant_size * op->type.bytes();

                if (stack_bytes > ((int64_t(1) << 31) - 1)) {
                    user_error << "Total size for allocation "
                            << op->name << " is constant but exceeds 2^31 - 1.\n";
                } else {
                    size_id_type = Int(32);
                    size_id = print_expr(make_const(size_id_type, constant_size));

                    if (op->memory_type == MemoryType::Stack ||
                        (op->memory_type == MemoryType::Auto &&
                        can_allocation_fit_on_stack(stack_bytes))) {
                        on_stack = true;
                    }
                }
            } else {
                // Check that the allocation is not scalar (if it were scalar
                // it would have constant size).
                internal_assert(!op->extents.empty());

                size_id = set_latest_expr(Int(64), print_expr(op->extents[0]));
                size_id_type = Int(64);

                for (size_t i = 1; i < op->extents.size(); i++) {
                    // Make the code a little less cluttered for two-dimensional case
                    string new_size_id_rhs;
                    string next_extent = print_expr(op->extents[i]);
                    if (i > 1) {
                        new_size_id_rhs = "(" + size_id + " > ((int64_t(1) << 31) - 1)) ? " + size_id + " : (" + size_id + " * " + next_extent + ")";
                    } else {
                        new_size_id_rhs = size_id + " * " + next_extent;
                    }
                    size_id = set_latest_expr(Int(64), new_size_id_rhs);
                }
                stream << get_indent() << "if (("
                    << size_id << " > ((int64_t(1) << 31) - 1)) || (("
                    << size_id << " * sizeof("
                    << op_type << ")) > ((int64_t(1) << 31) - 1)))\n";
                open_scope();

                // (TODO) Have some sort of check for the buffer allocaiton here directly in C
                user_warning << "OneAPI currently does not validate for "
                             << "32-bit signed overflow computing size of allocation " << op->name << "\n";
                stream << get_indent() << "// OneAPI currently does not validate for "
                             << "32-bit signed overflow computing size of allocation " << op->name << "\n";

                // Below is the original implementaiton from CodeGen_C

                // stream << get_indent();
                // TODO: call halide_error_buffer_allocation_too_large() here instead
                // TODO: call create_assertion() so that NoAssertions works
                // stream << "halide_error(_ucon, "
                //     << "\"32-bit signed overflow computing size of allocation " << op->name << "\\n\");\n";
                // stream << get_indent() << "return -1;\n";

                close_scope("overflow test " + op->name);
            }

            // Check the condition to see if this allocation should actually be created.
            // If the allocation is on the stack, the only condition we can respect is
            // unconditional false (otherwise a non-constant-sized array declaration
            // will be generated).
            if (!on_stack || is_zero(op->condition)) {
                Expr conditional_size = Select::make(op->condition,
                                                    Variable::make(size_id_type, size_id),
                                                    make_const(size_id_type, 0));
                conditional_size = simplify(conditional_size);
                size_id = set_latest_expr(Int(64), print_expr(conditional_size));
            }

            Allocation alloc;
            alloc.type = op->type;
            allocations.push(op->name, alloc);

            stream << get_indent() << op_type;

            if (on_stack) {
                if (ends_with(op->name, ".temp")) {
                    stream << op_name << ";\n";
                } else {
                    stream << op_name
                        << "[" << size_id << "];\n";
                }
            } else {
                stream << "*"
                    << op_name
                    << " = ("
                    << op_type
                    << " *)std::malloc(sizeof("
                    << op_type
                    << ")*" << size_id << ");\n";
                heap_allocations.push(op->name);
            }
        }

        if (!on_stack) {
            // create_assertion(op_name, "assert(false) /* replaced halide_error_out_of_memory */");
            EmitOneAPIFunc::create_assertion(op_name, "None");
            free_function = op->free_function.empty() ? "std::free" : op->free_function; // Type of extended op to use for freeing
        }

        op->body.accept(this);


        if (allocations.contains(op->name)) {
            if (heap_allocations.contains(op->name)) {
                // Heap allocations may be from an expression or a malloc all
                // the map is only implemented for Calls at the moment
                // plan to eventually go to a switch statment function
                if (op->new_expr.defined()) {
                    stream << get_indent() << print_name(op->name)  << " = NULL;\n";
                } else if (free_function.find("std::free") != std::string::npos) {
                    stream << get_indent() << "if (" << print_name(op->name)  << ") { std::free(" << print_name(op->name) << "); };\n";
                } else if (free_function.find("halide_device_host_nop_free") != std::string::npos) {
                    stream << get_indent() << ext_funcs.halide_device_host_nop_free(print_name(op->name));
                } else {
                    // internal_assert(false);
                    user_warning << "OneAPI: Free " << op->name << " with " << free_function << " to-be-replaced";
                    UnImplementedExternFuncs.insert(free_function);
                }
                heap_allocations.pop(op->name);
            }
            allocations.pop(op->name);
        }

        close_scope("alloc " + print_name(op->name));
    }
    return;


    // Semi-original CodeGen_OneAPI_C implementation
    {
        if (op->name == "__shared") {
            // Already handled
            debug(2) << "Allocate " << op->name << " already handled\n";
            stream << get_indent() << "// Allocate " << op->name << " already handled\n";
            op->body.accept(this);
        } else {
            // Check if the allocation is for global mutable pointer
            // if (op->memory_type == MemoryType::CLPtr) {
            //     user_assert(op->extents.size() == 1);
            //     user_assert(op->extents[0].as<StringImm>());
            //     auto arg = op->extents[0].as<StringImm>()->value;
            //     // parent->pointer_args[op->name] = arg;
            //     pointer_args[op->name] = arg;
            //     debug(2) << "Found pointer arg " << op->name << "... \n";
            //     op->body.accept(this);
            //     return;
            // }

            open_scope();

            // debug(2) << "Allocate " << op->name << " on device\n";
            debug(2) << "Allocate " << op->name << " on device w/ memory_type: " << op->memory_type << "\n";

            debug(3) << "Pushing allocation called " << op->name << " onto the symbol table\n";

            // Allocation is not a shared memory allocation, just make a local declaration.
            // It must have a constant size.
            int32_t size = op->constant_allocation_size();
            // user_assert(size > 0)
            //     << "OneAPI at this time has not implemented the case of allocation of size 0 for: "
            //     << op->name << "\n";

            if (size == 0) {
                user_warning << "OneAPI at this time has not implemented the case of allocation of size 0 for: " << op->name << "\n";
            }

            if (size == 1) {
                stream << get_indent() << print_type(op->type) << ' '
                    << print_name(op->name) << ";\n";
            } else {
                stream << get_indent() << print_type(op->type) << ' '
                    << print_name(op->name) << "[" << size << "];\n";
            }


            Allocation alloc;
            alloc.type = op->type;
            allocations.push(op->name, alloc);

            op->body.accept(this);

            // Should have been freed internally
            // (TODO) Need to correct this for OneAPI implemenntation as it currently is not freed
            // internal_assert(!allocations.contains(op->name));

            // parent->close_scope("alloc " + parent->print_name(op->name)); // OneAPI does need to add scopes
            close_scope("alloc " + print_name(op->name)); // OneAPI does need to add scopes
        }
    }
    return;

    // Using the CodeGen_C::visit(const Allocate *op) implementation, copied here
    {
        open_scope();

        string op_name = print_name(op->name);
        string op_type = print_type(op->type, AppendSpace);

        // For sizes less than 8k, do a stack allocation
        bool on_stack = false;
        int32_t constant_size;
        string size_id;
        Type size_id_type;

        if (op->new_expr.defined()) {
            Allocation alloc;
            alloc.type = op->type;
            allocations.push(op->name, alloc);
            heap_allocations.push(op->name);
            string value = print_expr(op->new_expr);
            stream << get_indent() << op_type << "*" << op_name << " = (" << op_type << "*)(" << value << ");\n";
        } else {
            constant_size = op->constant_allocation_size();
            if (constant_size > 0) {
                int64_t stack_bytes = constant_size * op->type.bytes();

                if (stack_bytes > ((int64_t(1) << 31) - 1)) {
                    user_error << "Total size for allocation "
                            << op->name << " is constant but exceeds 2^31 - 1.\n";
                } else {
                    size_id_type = Int(32);
                    size_id = print_expr(make_const(size_id_type, constant_size));

                    if (op->memory_type == MemoryType::Stack ||
                        (op->memory_type == MemoryType::Auto &&
                        can_allocation_fit_on_stack(stack_bytes))) {
                        on_stack = true;
                    }
                }
            } else {
                // Check that the allocation is not scalar (if it were scalar
                // it would have constant size).
                internal_assert(!op->extents.empty());

                size_id = set_latest_expr(Int(64), print_expr(op->extents[0]));
                size_id_type = Int(64);

                for (size_t i = 1; i < op->extents.size(); i++) {
                    // Make the code a little less cluttered for two-dimensional case
                    string new_size_id_rhs;
                    string next_extent = print_expr(op->extents[i]);
                    if (i > 1) {
                        new_size_id_rhs = "(" + size_id + " > ((int64_t(1) << 31) - 1)) ? " + size_id + " : (" + size_id + " * " + next_extent + ")";
                    } else {
                        new_size_id_rhs = size_id + " * " + next_extent;
                    }
                    size_id = set_latest_expr(Int(64), new_size_id_rhs);
                }
                stream << get_indent() << "if (("
                    << size_id << " > ((int64_t(1) << 31) - 1)) || (("
                    << size_id << " * sizeof("
                    << op_type << ")) > ((int64_t(1) << 31) - 1)))\n";
                open_scope();
                stream << get_indent();
                // TODO: call halide_error_buffer_allocation_too_large() here instead
                // TODO: call create_assertion() so that NoAssertions works
                stream << "halide_error(_ucon, "
                    << "\"32-bit signed overflow computing size of allocation " << op->name << "\\n\");\n";
                stream << get_indent() << "return -1;\n";
                close_scope("overflow test " + op->name);
            }

            // Check the condition to see if this allocation should actually be created.
            // If the allocation is on the stack, the only condition we can respect is
            // unconditional false (otherwise a non-constant-sized array declaration
            // will be generated).
            if (!on_stack || is_zero(op->condition)) {
                Expr conditional_size = Select::make(op->condition,
                                                    Variable::make(size_id_type, size_id),
                                                    make_const(size_id_type, 0));
                conditional_size = simplify(conditional_size);
                size_id = set_latest_expr(Int(64), print_expr(conditional_size));
            }

            Allocation alloc;
            alloc.type = op->type;
            allocations.push(op->name, alloc);

            stream << get_indent() << op_type;

            if (on_stack) {
                if (is_host_interface() && ends_with(op->name, ".temp")) {
                    stream << op_name << ";\n";
                } else {
                    stream << op_name
                        << "[" << size_id << "];\n";
                }
            } else {
                stream << "*"
                    << op_name
                    << " = ("
                    << op_type
                    << " *)halide_malloc(_ucon, sizeof("
                    << op_type
                    << ")*" << size_id << ");\n";
                heap_allocations.push(op->name);
            }
        }

        if (!on_stack) {
            stream << "assert(false); // replaced halide_error_out_of_memory\n";

            stream << get_indent();
            string free_function = op->free_function.empty() ? "halide_free" : op->free_function;
            stream << "HalideFreeHelper " << op_name << "_free(_ucon, "
                << op_name << ", " << free_function << ");\n";
        }
        op->body.accept(this);

        if (allocations.contains(op->name)) {
            if (heap_allocations.contains(op->name)) {
                stream << get_indent() << print_name(op->name) << "_free.free();\n";
                heap_allocations.pop(op->name);
            }
            allocations.pop(op->name);
        }

        close_scope("alloc " + print_name(op->name));
    }
    return;

}

void CodeGen_Clear_OneAPI_Dev::EmitOneAPIFunc::visit(const Free *op) {
    // Should be freed
    internal_assert(allocations.contains(op->name));
    if (heap_allocations.contains(op->name)) {
        heap_allocations.pop(op->name);
    }
    allocations.pop(op->name);
}


void CodeGen_Clear_OneAPI_Dev::EmitOneAPIFunc::visit(const AssertStmt *op) {
    // Currently not implementing any assertions
    // this will reduce the number of Extern Funcs that need
    // to be replaced
    // FIXME: Some necessary functions may be executed in AssertStmt::condition.
    // assert(extern_funcs() == 0, extern_funcs())
    // So we can't simply ignore them.
    const auto *eq = op->condition.as<EQ>();
    if (eq != nullptr) {
        const auto *call_1 = eq->a.as<Call>();
        if (call_1 != nullptr) {
            const auto *call_2 = op->message.as<Call>();
            if (call_2 != nullptr && call_1->name == call_2->name) {
                auto iter = print_extern_call_map.find(call_1->name);
                if (iter != print_extern_call_map.end()) {
                    stream << get_indent() << (ext_funcs.*(iter->second))(call_1) << "\n";
                }
            }
        }
    }
    return;

    // Currently, we are not mplementaiting
    // assertions inside a kernel
    if (!currently_inside_kernel) {
        if (target.has_feature(Target::NoAsserts)) return;


        std::string id_cond = print_expr(op->condition);



        stream << get_indent() << "if (!" << id_cond << ") ";
        open_scope();
        stream << get_indent() << "std::cout << \"Condition '"
               <<  id_cond << "' failed ";
        if (op->message.defined()) {
            std::string id_msg = print_expr(op->message);
            stream << "with error id_msg: " << id_msg;
        }
        stream << "\\n\";\n";
        stream << get_indent() << "assert(false);\n";
        close_scope("");
    } else {
        debug(2) << "Ignoring assertion inside OneAPI kernel: " << op->condition << "\n";
    }
}

void CodeGen_Clear_OneAPI_Dev::EmitOneAPIFunc::visit(const Evaluate *op) {
    if (is_const(op->value)) return;
    print_expr(op->value);
    const auto *call = op->value.as<Call>();
    if (call != nullptr && call->call_type == Call::CallType::Extern && call->type == Int(32))
        stream << get_indent() << latest_expr << ";\n";
}

// EmitOneAPIFunc
std::string CodeGen_Clear_OneAPI_Dev::EmitOneAPIFunc::print_extern_call(const Call *op) {
    // Previous i.e. with directly calling _halide_buffer funcs
    {
        ostringstream rhs;
        vector<string> args(op->args.size());
        for (size_t i = 0; i < op->args.size(); i++) {
            args[i] = print_expr(op->args[i]);
        }

        auto search = print_extern_call_map.find(op->name);

        if (search != print_extern_call_map.end()) {
            // (TODO) Have the functions do a simple check instead of always marking them as `... = 0;`
            // i.e. `... = exec() ? 0 /*success*/ : -1 /*errror*/;` or whatever's appropriate to return
            // This will remove the need to check for user context and simplify this better
            if (function_takes_user_context(op->name)) {
                // functions that take user context return 0 on success
                // simple place 0 as the value and run the actual function in the next line
                rhs << "0; // " << op->name << "(" << with_commas(args) << ") replaced with line(s) below \n";
                rhs << get_indent() << (ext_funcs.*(search->second))(op);
            } else {
                rhs << (ext_funcs.*(search->second))(op) << " /* " << op->name << "(" << with_commas(args) << ") replaced */";
            }
        } else {
            if (op->name.find("_halide_buffer") != std::string::npos && !function_takes_user_context(op->name)) {
                rhs << op->name << "(" << with_commas(args) << ")";
            } else {
                // (TODO) check with (target.has_feature(Target::NoAsserts)) as some call external functions
                rhs << "0; //!!! print_extern_call(): " << op->name << "(" << with_commas(args) << ") to-be-replaced";
                user_warning << "OneAPI: " << op->name << "(" << with_commas(args) << ") to-be-replaced";
                UnImplementedExternFuncs.insert(op->name);
            }
        }
        return rhs.str();
    }
}

// EmitOneAPIFunc
std::string CodeGen_Clear_OneAPI_Dev::EmitOneAPIFunc::print_reinterpret(Type type, Expr e) {
    ostringstream oss;
    if (type.is_handle() || e.type().is_handle()) {
        // Use a c-style cast if either src or dest is a handle --
        // note that although Halide declares a "Handle" to always be 64 bits,
        // the source "handle" might actually be a 32-bit pointer (from
        // a function parameter), so calling reinterpret<> (which just memcpy's)
        // would be garbage-producing.
        oss << "(" << print_type(type) << ")";
    } else {
        oss << "reinterpret<" << print_type(type) << ">";
    }
    oss << "(" << print_expr(e) << ")";
    return oss.str();
}

std::string CodeGen_Clear_OneAPI_Dev::EmitOneAPIFunc::print_type(Type type, AppendSpaceIfNeeded space) {
    if (!currently_inside_kernel) // Modifed Type.cpp implementation to fix int32x4_t
    {
        ostringstream oss;
        bool include_space = space == AppendSpace;
        bool needs_space = true;
        bool c_plus_plus = true;
        if (type.is_bfloat()) {
            oss << "bfloat" << type.bits();
        } else if (type.is_float()) {
            if (type.bits() == 32) {
                oss << "float";
            } else if (type.bits() == 64) {
                oss << "double";
            } else {
                oss << "float" << type.bits();
            }
            if (type.is_vector()) {
                oss << type.lanes();
            }
        } else if (type.is_handle()) {
            needs_space = false;
            // If there is no type info or is generating C (not C++) and
            // the type is a class or in an inner scope, just use void *.
            if (type.handle_type == NULL ||
                (!c_plus_plus &&
                (!type.handle_type->namespaces.empty() ||
                !type.handle_type->enclosing_types.empty() ||
                type.handle_type->inner_name.cpp_type_type == halide_cplusplus_type_name::Class))) {
                oss << "void *";
            } else {
                if (type.handle_type->inner_name.cpp_type_type ==
                    halide_cplusplus_type_name::Struct) {
                    oss << "struct ";
                }
                if (!type.handle_type->namespaces.empty() ||
                    !type.handle_type->enclosing_types.empty()) {
                    oss << "::";
                    for (size_t i = 0; i < type.handle_type->namespaces.size(); i++) {
                        oss << type.handle_type->namespaces[i] << "::";
                    }
                    for (size_t i = 0; i < type.handle_type->enclosing_types.size(); i++) {
                        oss << type.handle_type->enclosing_types[i].name << "::";
                    }
                }
                oss << type.handle_type->inner_name.name;
                if (type.handle_type->reference_type == halide_handle_cplusplus_type::LValueReference) {
                    oss << " &";
                } else if (type.handle_type->reference_type == halide_handle_cplusplus_type::LValueReference) {
                    oss << " &&";
                }
                for (auto modifier : type.handle_type->cpp_type_modifiers) {
                    if (modifier & halide_handle_cplusplus_type::Const) {
                        oss << " const";
                    }
                    if (modifier & halide_handle_cplusplus_type::Volatile) {
                        oss << " volatile";
                    }
                    if (modifier & halide_handle_cplusplus_type::Restrict) {
                        oss << " restrict";
                    }
                    if (modifier & halide_handle_cplusplus_type::Pointer) {
                        oss << " *";
                    }
                }
            }
        } else {
            // This ends up using different type names than OpenCL does
            // for the integer vector types. E.g. uint16x8_t rather than
            // OpenCL's short8. Should be fine as CodeGen_C introduces
            // typedefs for them and codegen always goes through this
            // routine or its override in CodeGen_OpenCL to make the
            // names. This may be the better bet as the typedefs are less
            // likely to collide with built-in types (e.g. the OpenCL
            // ones for a C compiler that decides to compile OpenCL).
            // This code also supports arbitrary vector sizes where the
            // OpenCL ones must be one of 2, 3, 4, 8, 16, which is too
            // restrictive for already existing architectures.
            switch (type.bits()) {
            case 1:
                // bool vectors are always emitted as uint8 in the C++ backend
                if (type.is_vector()) {
                    oss << "uint8x" << type.lanes() << "_t";
                } else {
                    oss << "bool";
                }
                break;
            case 8:
            case 16:
            case 32:
            case 64:
                if (type.is_uint()) {
                    oss << 'u';
                }
                oss << "int";
                if (type.is_vector()) {
                    oss << type.lanes();
                } else {
                    oss << type.bits() << "_t";
                }
                break;
            default:
                user_error << "Can't represent an integer with this many bits in C: " << type << "\n";
            }
        }
        if (include_space && needs_space) {
            oss << " ";
        }
        return oss.str();
    } else { // From original OpenCl's CodeGen_C
        ostringstream oss;
        if (type.is_generated_struct()) {
            int type_id = type.bits();
            const std::pair<std::string, std::vector<Type>> &entry = GeneratedStructType::structs[type_id];
            string struct_name = entry.first;
            oss << struct_name;
        } else if (!is_standard_opencl_type(type)) {
            Type basic_type = type.with_lanes(1);
            string type_name = print_type(basic_type);
            oss << type_name;
        } else if (type.is_float()) {
            if (type.bits() == 16) {
                user_assert(target.has_feature(Target::CLHalf))
                    << "OpenCL kernel uses half type, but CLHalf target flag not enabled\n";
                oss << "half";
            } else if (type.bits() == 32) {
                oss << "float";
            } else if (type.bits() == 64) {
                oss << "double";
            } else {
                user_error << "Can't represent a float with this many bits in OpenCL C: " << type << "\n";
            }

        } else {
            if (type.is_uint() && type.bits() > 1) oss << 'u';
            switch (type.bits()) {
            case 1:
                internal_assert(type.lanes() == 1) << "Encountered vector of bool\n";
                oss << "bool";
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
                oss << "long";
                break;
            default:
                user_error << "Can't represent an integer with this many bits in OpenCL C: " << type << "\n";
            }
        }
        if (type.lanes() != 1) {
            switch (type.lanes()) {
            case 2:
            case 3:
            case 4:
            case 8:
            case 16:
                oss << type.lanes();
                break;
            default:
                if (GeneratedStructType::nonstandard_vector_type_exists(type)) {
                    oss << type.lanes();
                    break;
                } else {
                    user_error << "Unsupported vector width in OpenCL C: " << type << "\n";
                }
            }
        }
        if (space == AppendSpace) {
            oss << ' ';
        }
        return oss.str();
    }
}


}  // namespace Internal
}  // namespace Halide
