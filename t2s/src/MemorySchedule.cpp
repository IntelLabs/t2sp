/*******************************************************************************
* Copyright 2021 Intel Corporation
*
* Licensed under the BSD-2-Clause Plus Patent License (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* https://opensource.org/licenses/BSDplusPatent
*
* Unless required by applicable law or agreed to in writing,
* software distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions
* and limitations under the License.
*
*
* SPDX-License-Identifier: BSD-2-Clause-Patent
*******************************************************************************/
#include <list>
#include "../../Halide/src/CSE.h"
#include "../../Halide/src/IR.h"
#include "../../Halide/src/IREquality.h"
#include "../../Halide/src/IRMutator.h"
#include "../../Halide/src/IROperator.h"
#include "../../Halide/src/Simplify.h"
#include "../../Halide/src/Substitute.h"
#include "./MemorySchedule.h"
#include "./Utilities.h"

using std::string;
using std::vector;
using std::map;
using std::set;
using std::list;
using std::pair;
using std::tuple;

namespace Halide {
namespace Internal {

namespace {

enum InstType {
    UNDEFINED,
    MediaBlock,
    Scatter
};

// Collect information about loop nests and image parameters
vector<string> loop_vars;                   // loop variables (from inner to outer)
vector<Range>  loop_bounds;                 // the bounds of loop variables
map<string, vector<Expr>>  im_args;         // the arguments used to access an image parameter
map<string, vector<Range>> im_bounds;       // the bounds of arguments
map<string, Parameter> image_param;         // original image parameters

// Collect information about loop-invariant code motion
struct FuncInfo {
    bool is_output;                         // true if it is an output func
    Type func_type;                         // function's element type
    Expr if_cond;                           // the guarding condition
    string sink_loop;                       // the destination loop level of code motion
};
map<string, FuncInfo> func_info;
vector<size_t> space_loops;

struct GPUBufInfo {
    Region allocation;                      // bounds of the allocated buffer
    Expr access_idx;                        // the index to access the allocated buffer
    FetchParams fp;
    struct COPY {
        Range src;                          // the range of the source
        Range dest;                         // the range of the destination
    } copy_inst;                            // insert an instruction to reuse overlapped region
    struct LOAD {
        InstType type;                      // the load instruction type
        Expr elem;                          // elements of the load instruction (scatter or a region)
        size_t rw_len;                      // the length of data to be read/write
    } init_inst, iter_inst;                 // insert an instruction to load data
    struct LOOP {
        Region bounds;                      // the loops enclosing load instruction
        Expr addr;                          // the base address determined by above loops
        Expr offs;                          // the base address determined by enclosing loops
        size_t rem_sz;                      // the size of data not covered by loops (remaining size)
    } init_loop, iter_loop;                 // insert a loop to load data several times
};
map<string, GPUBufInfo> gpu_bufs;

struct {
    StoreParams sp;
    struct {
        InstType type;
        Expr elem;
        Expr offs;
        Expr addr;
    } st_inst;
    struct {
        Range loop;
        size_t rem_sz;
    } outer;
} store_func;

inline int int_val(Expr e) {
    auto v = simplify(e).as<IntImm>();
    internal_assert(v);
    return v->value;
}
inline Expr int_exp(int v) {
    return IntImm::make(Int(32), v);
}
inline Expr make_var(string s, int lanes = 1) {
    return Variable::make(Int(32).with_lanes(lanes), s);
}

inline string loop_name(string name, string suffix, int idx) {
    return name + "." + suffix + ".t" + std::to_string(idx);
}
inline string output_outer_loop(const string &out) {
    return loop_name(out, "store", 0);
}
inline string output_inner_loop(const string &out) {
    return loop_name(out, "store", 1);
}

inline string elem_name(const string &buf, const string &type) {
    return "val." +buf +"." +type +".elem";
}
inline string addr_name(const string &buf, const string &type,
                        const string &loc = "", const int idx = 0) {
    string tmp = "var." +buf +"." +type;
    if (loc != "") {
        tmp += "." +loc;
    }
    return tmp +".addr." +std::to_string(idx);
}
inline string buf_name(string name = "") {
    return name + "_buf";
}

int space_loop_extents() {
    int sz = 1;
    for (auto idx : space_loops) {
        sz *= int_val(loop_bounds[idx].extent);
    }
    return sz;
}

} // namespace

class FuncInfoCollector : public IRVisitor
{
    const map<string, Function> &env;
    Expr cond = UIntImm::make(UInt(1), 1);          // used to pass value acorss multiple calls
    Expr nested_cond = UIntImm::make(UInt(1), 1);   // used for code motion
    bool nested_select = true;

    Range infer_bounds_from_expr(Expr e) {
        map<string, Expr> fix_mins;
        map<string, Expr> fix_exts;
        for (size_t i = 0; i < loop_vars.size(); i++) {
            string name = loop_vars[i];
            fix_mins[name] = loop_bounds[i].min;
            fix_exts[name] = loop_bounds[i].extent;
        }
        Expr min = simplify(substitute(fix_mins, e));
        Expr extent = simplify(substitute(fix_exts, e));
        return Range(min, extent);
    }

    Expr get_equation_rhs(vector<Expr> &conjuctions, string var) {
        for (auto &c : conjuctions) {
            auto eq = c.as<EQ>();
            if (eq) {
                auto lhs = eq->a.as<Variable>();
                internal_assert(lhs);
                if (lhs->name == var) {
                    return eq->b;
                }
            }
        }
        return StringImm::make("invalid");
    }

public:
    using IRVisitor::visit;
    FuncInfoCollector(const map<string, Function>& _env)
        : env(_env) {}

    void visit(const IfThenElse *op) override {
        // Output condition is usually not nested
        nested_cond = op->condition;
        internal_assert(!op->else_case.defined());
        IRVisitor::visit(op);
    }

    void visit(const Select *op) override {
        Expr tmp_cond = cond;
        bool tmp_nested_select = nested_select;
        op->condition.accept(this);

        if (is_const(op->true_value) && tmp_nested_select) {
            // We are finding const value to initialize an inner-product operation
            nested_cond = simplify(tmp_cond && op->condition);
        }
        cond = tmp_cond && op->condition;
        nested_select = tmp_nested_select && op->true_value.as<Select>();
        op->true_value.accept(this);

        if (is_const(op->false_value) && tmp_nested_select) {
            // We are finding const value to initialize an inner-product operation
            nested_cond = simplify(tmp_cond && !op->condition);
        }
        cond = tmp_cond && !op->condition;
        nested_select = tmp_nested_select && op->false_value.as<Select>();
        op->false_value.accept(this);
    }

    void visit(const Realize *op) override {
        Function func;
        internal_assert(function_is_in_environment(op->name, env, func));
        if (func.definition().schedule().has_stt()) {
            internal_assert(space_loops.empty());
            const auto &stt_param = func.definition().schedule().transform_params()[0];
            for (size_t i = 0; i < stt_param.num_space_vars; i++) {
                if (stt_param.sch_vector_specified) {
                    // In this case, space loops must be at the innermost level.
                    space_loops.push_back(i);
                } else {
                    auto pos = std::find_if(loop_vars.begin(), loop_vars.begin() + stt_param.num_space_vars,
                                            [&](string &s){ return extract_last_token(s) == stt_param.src_vars[i]; });
                    space_loops.push_back(pos - loop_vars.begin());
                }
            }
        }
        IRVisitor::visit(op);
    }

    void visit(const Call* op) override {
        if (op->call_type == Call::CallType::Image) {
            string name = op->name + "_im";
            // Find new inputs and save their bounds and call arguments
            if (op->param.defined() && im_args.find(name) == im_args.end()) {
                image_param[name] = op->param;
                for (size_t i = 0; i < op->args.size(); i++) {
                    Range bound;
                    if (!op->param.buffer().defined()) {
                        // Replacing loop vars with their extents to find the bounds
                        bound = infer_bounds_from_expr(op->args[i]);
                    } else {
                        Expr min = op->param.buffer().min(i);
                        Expr extent = op->param.buffer().extent(i);
                        bound = Range(min, extent);
                    }
                    im_args[name].push_back(op->args[i]);
                    im_bounds[name].push_back(bound);
                }
            }
        }
        return IRVisitor::visit(op);
    }

    void visit(const Provide *op) override {
        Function func;
        internal_assert(function_is_in_environment(op->name, env, func));
        if (!func.definition().schedule().is_merged() && !func.has_merged_defs()) {
            IRVisitor::visit(op);
            return;
        }
        for (size_t i = 0; i < op->values.size(); i++) {
            op->values[i].accept(this);
        }
        vector<Expr> conjuction = break_logic_into_conjunction(nested_cond);
        FuncInfo tmp_info;
        tmp_info.is_output = func.definition().schedule().is_output();
        tmp_info.if_cond   = nested_cond;
        tmp_info.func_type = func.output_types()[0];
        tmp_info.sink_loop = "";
        
        for (size_t i = 0; i < loop_vars.size(); i++) {
            bool is_space = find(space_loops.begin(), space_loops.end(), i) != space_loops.end();
            if (is_space) {
                // We allocate registers for space loops, so hoisting code above them is safe
                continue;
            }
            // Find a condition corresponding to the current loop
            auto rhs = get_equation_rhs(conjuction, loop_vars[i]);
            Expr border_val = tmp_info.is_output ? loop_bounds[i].extent-1 : 0;
            bool is_border = equal(rhs, simplify(border_val));
            if (!is_border) {
                // We cannot further hoist code without border condition
                break;
            }
            internal_assert(tmp_info.if_cond.defined());
            tmp_info.sink_loop = loop_vars[i];
            tmp_info.if_cond = simplify(substitute(loop_vars[i], border_val, tmp_info.if_cond));
        }
        func_info[op->name] = tmp_info;
    }
};

class GPUStoreBuilder
{
    const map<string, Function> &env;
    vector<Expr> arg_bounds;
    string output_func;

    void update_shape_args() {
        auto &shape_args = store_func.sp.shape_args;
        map<string, Expr> real_loops;
        map<string, Expr> fix_bounds;

        for (size_t i = 0; i < loop_vars.size(); i++) {
            string var = extract_last_token(loop_vars[i]);
            // if (ends_with(var, "__block_id_x") || ends_with(var, "__thread_id_x")
            //  || ends_with(var, "__block_id_y") || ends_with(var, "__thread_id_y")) {
            //     size_t pos = loop_vars[i].size()-var.size()-1;
            //     var = extract_last_token(loop_vars[i].substr(0, pos));
            // }
            real_loops.insert({ var, make_var(loop_vars[i]) });
            fix_bounds.insert({ loop_vars[i], loop_bounds[i].extent-1 });
        }
        size_t sz = shape_args.size();
        arg_bounds.resize(sz);
        for (size_t i = 0; i < sz; i++) {
            shape_args[i] = simplify(substitute(real_loops, shape_args[i]));
            arg_bounds[i] = simplify(substitute(fix_bounds, shape_args[i])+1);
        }
    }

    Expr get_flatten_expr() {
        auto &shape_args = store_func.sp.shape_args;
        map<string, Expr> fix_loops;
        for (size_t i = 0; i < loop_vars.size(); i++)
            fix_loops.insert({ loop_vars[i], loop_bounds[i].extent-1});

        Expr flat = 0;
        Expr stri = 1;
        for (size_t i = 0; i < shape_args.size(); i++) {
            Expr bound = substitute(fix_loops, shape_args[i])+1;
            flat += shape_args[i] *stri;
            stri *= bound;
        }
        return simplify(flat);
    }

    // replace space loops with a fused var
    Expr fuse_space_loops(Expr flatten) {
        const auto &rw_len = store_func.sp.rw_len;
        internal_assert(rw_len);
        map<string, Expr> rev_exprs;
        vector<Expr> space_loop_bounds;
        Expr fused;

        string outer_name = output_outer_loop(output_func);
        string inner_name = output_inner_loop(output_func);
        Expr inner_var = make_var(inner_name);
        Expr outer_var = make_var(outer_name);
        Expr var = outer_var;

        for (auto idx : space_loops) {
            Expr ext = loop_bounds[idx].extent;
            Expr rev = simplify(var % ext);
            rev_exprs.insert({ loop_vars[idx], rev });
            var = var / ext;
        }
        fused = substitute(rev_exprs, flatten);

        Expr new_var = inner_var + int_exp(rw_len) * outer_var;
        fused = simplify(substitute(outer_name, new_var, fused));

        return fused;
    }

    auto get_outer_loop(void)
    -> decltype(store_func.outer) {
        const auto &rw_len = store_func.sp.rw_len;
        internal_assert(rw_len);

        Expr sz = space_loop_extents();
        Expr min = 0;
        Expr ext = simplify(sz /int_exp(rw_len));
        size_t rem_sz = int_val(simplify(sz - int_exp(rw_len) * ext));
        if (rem_sz > 0)
            ext = simplify(ext + 1);

        return { Range(min, ext), rem_sz };
    }

    auto get_scatter_store_inst(Expr fused)
    -> decltype(store_func.st_inst) {
        const auto &rw_len = store_func.sp.rw_len;
        internal_assert(rw_len);

        map<string, Expr> fix_loops;
        vector<Expr> idxes;
        string outer_name = output_outer_loop(output_func);
        string inner_name = output_inner_loop(output_func);
        Expr inner_var = make_var(inner_name);
        Expr outer_var = make_var(outer_name);

        fix_loops.insert({ outer_name, 0 });
        for (size_t i = 0; i < loop_vars.size(); i++)
            fix_loops.insert({ loop_vars[i], 0 });
        Expr inner = simplify(substitute(fix_loops, fused));

        for (size_t i = 0; i < rw_len; i++) {
            Expr idx = simplify(substitute(inner_name, int_exp(i), inner));
            internal_assert(is_const(idx));
            idxes.push_back(idx);
        }
        Expr elem = Shuffle::make_concat(idxes);

        Expr offs = simplify(substitute(inner_name, 0, fused));
        Expr addr = simplify(substitute(outer_name, 0, offs));
        for (size_t i = 0; i < loop_vars.size(); i++)
            offs = simplify(substitute(loop_vars[i], 0, offs));

        return { InstType::Scatter, elem, offs, addr };
    }

    auto get_2d_store_inst(Expr fused_arg0, Expr fused_arg1)
    -> decltype(store_func.st_inst) {
        const auto &rw_len = store_func.sp.rw_len;
        string outer_name = output_outer_loop(output_func);
        string inner_name = output_inner_loop(output_func);

        int ext_0 = int_val(loop_bounds[0].extent);
        int ext_1 = rw_len / ext_0;
        Expr addr_0 = substitute(inner_name, 0, fused_arg0);
        Expr addr_1 = substitute(inner_name, 0, fused_arg1);
        addr_0 = simplify(substitute(outer_name, 0, addr_0));
        addr_1 = simplify(substitute(outer_name, 0, addr_1));

        Expr offs = Shuffle::make_concat({ 0, 0 });
        Expr addr = Shuffle::make_concat({ addr_0, addr_1 });
        Expr elem = Shuffle::make_concat({ ext_1, ext_0 });
        return { InstType::MediaBlock, elem, offs, addr };
    }

public:
    GPUStoreBuilder(decltype(env) _env)
        : env(_env) { }

    void get_store_func() {
        for (auto &kv : func_info) {
            if (kv.second.is_output)
                output_func = kv.first;
        }
        if (output_func.empty())
            return;

        Function func;
        internal_assert(function_is_in_environment(output_func, env, func));
        store_func.sp = func.definition().schedule().store_params();
        const auto &shape_args = store_func.sp.shape_args;
        if (shape_args.empty())
            return;

        update_shape_args();
        if (shape_args.size() == 2) {
            store_func.sp.rw_len = space_loop_extents();
            Expr fused_arg0    = fuse_space_loops(shape_args[0]);
            Expr fused_arg1    = fuse_space_loops(shape_args[1]);
            store_func.st_inst = get_2d_store_inst(fused_arg0, fused_arg1);
            store_func.outer   = get_outer_loop();
            return;
        }
        Expr flatten       = get_flatten_expr();
        Expr fused_expr    = fuse_space_loops(flatten);
        store_func.st_inst = get_scatter_store_inst(fused_expr);
        store_func.outer   = get_outer_loop();
    }
};

class GPUBufferBuilder
{
    struct LastArgument {
        size_t arg_idx;
        size_t num_iter;
    };
    bool coalesce_vectorized_elements = false;
    const map<string, Function> &env;

    // For stencil, the overlapped region partitions loops into two sets: init_loop and iter_loop.
    // The data corresponding to init_loop set is loaded at the beginning.
    // The data corresponding to iter_loop set is loaded at each iteration.
    void get_loop_bounds(string name, Region &init_bounds, Region &iter_bounds) {
        const auto &store_at   = gpu_bufs[name].fp.store_at;
        const auto &reuse_args = gpu_bufs[name].fp.reuse_args;
        internal_assert(reuse_args.empty() || reuse_args.size() == loop_bounds.size());
        internal_assert(!store_at.empty());

        size_t sz = loop_vars.size();
        for (size_t i = 0; i < sz; i++) {
            if (extract_last_token(loop_vars[i]) == store_at
                && (reuse_args.empty() || !is_one(reuse_args[i]))) {
                iter_bounds = loop_bounds;
                return;
            }
        }

        size_t cnt = 0;
        init_bounds.resize(sz, Range(0, 0));
        iter_bounds.resize(sz, Range(0, 0));
        for (size_t i = 0; i < loop_vars.size(); i++) {
            if (extract_last_token(loop_vars[i]) == store_at) {
                break;
            }
            Expr min = loop_bounds[i].min;
            Expr ext = loop_bounds[i].extent;
            Expr diff = reuse_args[i];
            if (is_negative_const(diff)) {
                Expr point = simplify(ext + diff);
                init_bounds[i] = Range(min, point);
                iter_bounds[i] = Range(point, ext);
                cnt++;
            } else if (is_positive_const(diff)) {
                Expr point = simplify(min + diff);
                init_bounds[i] = Range(point, ext);
                iter_bounds[i] = Range(min, point);
                cnt++;
            } else {
                init_bounds[i] = Range(min, ext);
                iter_bounds[i] = Range(min, ext);
            }
        }
        user_assert(cnt < 2) << "Complex reuse pattern is not supported\n";
    }

    // The base address of load instruction is determined by enclosing loops.
    // For example, given an input A(kk+KK*k, ii+II*i) and loops kk, ii, k, i.
    // If insert a buffer below loop k, the base address is A(KK*k, II*i),
    // which is calculated by replacing all inner loops (kk, ii) with 0
    Expr get_load_addr(string name, const Region &loop_bounds, bool is_init = false) {
        const auto &store_at  = gpu_bufs[name].fp.store_at;
        const auto &inst_type = gpu_bufs[name].iter_inst.type;
        internal_assert(!store_at.empty() && inst_type != UNDEFINED);

        if (loop_bounds.size() == 0) {
            return 0;
        }
        map<string, Expr> fix_loops;
        size_t i = 0;
        for (; i < loop_vars.size(); i++) {
            string var = loop_vars[i];
            if (extract_last_token(var) == store_at)
                break;
            fix_loops[var] = loop_bounds[i].min;
        }
        if (is_init) {
            // The initial load is inserted above the store_at loop
            fix_loops[loop_vars[i]] = loop_bounds[i].min;
        }
        vector<Expr> new_args;
        Expr stri = 1;
        Expr addr = 0;
        for (size_t i = 0; i < im_args[name].size(); i++) {
            Expr new_arg = substitute(fix_loops, im_args[name][i]);
            if (inst_type == InstType::MediaBlock) {
                new_args.push_back(simplify(new_arg));
            } else {
                addr += stri * new_arg;
                stri *= im_bounds[name][i].extent;
            }
        }
        if (inst_type == InstType::MediaBlock) {
            // Two-dimensional address is packed as a vector
            addr = Shuffle::make_concat(new_args);
        }
        return simplify(addr);
    }

    // The allocation size of a buffer is determined by inner loops.
    // For example, given an input A(kk+KK*k, ii+II*i) and loops kk, ii, k, i.
    // If insert a buffer below loop k, the allocation size is A(KK, 0),
    // which is calculated by replacing all outer loops (k, i) with 0
    // and all inner loops (kk, ii) with their extents.
    Region get_alloc_size(string name, const Region &loop_bounds) {
        const auto &store_at = gpu_bufs[name].fp.store_at;
        internal_assert(!store_at.empty());

        if (loop_bounds.size() == 0) {
            return Region();
        }
        map<string, Expr> fix_mins;
        map<string, Expr> fix_exts;
        bool flag = true;
        for (size_t i = 0; i < loop_vars.size(); i++) {
            string var = loop_vars[i];
            if (extract_last_token(var) == store_at)
                flag = false;
            fix_mins[var] = flag ? loop_bounds[i].min : 0;
            fix_exts[var] = flag ? loop_bounds[i].extent-1 : 0;
        }

        Region region;
        for (size_t i = 0; i < im_args[name].size(); i++) {
            Expr min = simplify(substitute(fix_mins, im_args[name][i]));
            Expr ext = simplify(substitute(fix_exts, im_args[name][i])+1);
            region.push_back(Range(min, ext));
        }
        return region;
    }

    // Expand scatter vector step by step until rw_len elements are collected.
    // For example, given an input A(kk+KK*k, ii+II*i) and loops kk, ii, k, i,
    // the region for loops kk, ii is A(0..KK, 0..II), then expand elements like:
    // [0, 1, ..., KK] (when ii == 0)
    // [0, 1, ..., KK, 0+1*KK, 1+1*KK, ...] (when ii == 1)
    // [0, 1, ..., KK, 0+1*KK, 1+1*KK, ..., 0+2*KK, 1+2*KK, ...] (when ii == 2)
    void expand_elem(const string &name, const Region &region,
                     vector<Expr> &idx, LastArgument &last_arg) {
        size_t rw_len = gpu_bufs[name].fp.rw_len;
        internal_assert(rw_len > 0);

        size_t stri = 1;
        for (size_t i = 0; i < im_bounds[name].size(); i++) {
            size_t cur_sz = idx.size();
            size_t min = int_val(region[i].min);
            size_t upp = int_val(region[i].extent) +min;
            for (size_t j = min+1; j < upp; j++) {
                for (size_t k = 0; k < cur_sz; k++) {
                    idx.push_back(simplify( idx[k] + int_exp(j*stri) ));
                }
                user_assert(idx.size() <= rw_len)
                    << "Cannot enable scatter read for " << name << "\n";
                if (idx.size() == rw_len) {
                    last_arg = { i, j-min+1 };
                    return;
                }
            }
            stri *= int_val(im_bounds[name][i].extent);
        }
    }

    auto get_scatter_elem(string name, Region &region, LastArgument &last_arg)
    -> GPUBufInfo::LOAD {
        size_t rw_len = gpu_bufs[name].fp.rw_len;
        internal_assert(rw_len > 0);

        vector<Expr> idx;
        Expr init = 0, stri = 1;
        for (size_t i = 0; i < im_args[name].size(); i++) {
            init += im_bounds[name][i].min * stri;
            stri *= im_bounds[name][i].extent;
        }
        idx.push_back(simplify(init));

        // TOFIX: vectorized loop is not required to be innermost
        size_t vec_ext = int_val(loop_bounds[0].extent);
        if (rw_len != vec_ext) {
            // not coalescing elements for the innermost loop in this case
            expand_elem(name, region, idx, last_arg);
        } else {
            coalesce_vectorized_elements = true;
            map<string, Expr> fix_mins;
            map<string, Expr> fix_exts;
            for (auto &name : loop_vars) {
                fix_mins[name] = 0;
                fix_exts[name] = 0;
            }
            fix_mins[loop_vars[0]] = loop_bounds[0].min;
            fix_exts[loop_vars[0]] = loop_bounds[0].extent-1;

            Region inner;
            Expr stri = 1, elem = 0;
            for (size_t i = 0; i < im_args[name].size(); i++) {
                Expr min = simplify(substitute(fix_mins, im_args[name][i]));
                Expr ext = simplify(substitute(fix_exts, im_args[name][i])+1);
                inner.push_back(Range(min, ext));
                user_assert(is_zero(min) || equal(min, region[i].min))
                    << "The coalesced loop must be align with other loops\n";
            }
            expand_elem(name, inner, idx, last_arg);
            for (size_t i = 0; i < im_args[name].size(); i++) {
                user_assert(is_one(inner[i].extent) || i == last_arg.arg_idx
                            || equal(inner[i].extent, region[i].extent))
                    << "The coalesced loop must be align with other loops\n";
            }
        }
        Expr concat_idx = Shuffle::make_concat(idx);
        return { InstType::Scatter, concat_idx, rw_len };
    }

    auto get_load_elem(string name, Region &region, LastArgument &last_arg)
    -> GPUBufInfo::LOAD {
        if (region.size() == 0) {
            return { UNDEFINED };
        }
        if (region.size() == 2) {
            int ext_0 = region[0].extent.as<IntImm>()->value;
            int ext_1 = region[1].extent.as<IntImm>()->value;
            size_t rw_len = ext_0 * ext_1;
            Expr elem = Shuffle::make_concat({ ext_1, ext_0 });

            internal_assert(is_zero(region[0].min));
            region[0].extent  = 0;
            last_arg.arg_idx  = 1;
            last_arg.num_iter = ext_1;
            return { InstType::MediaBlock, elem, rw_len };
        }
        return get_scatter_elem(name, region, last_arg);
    }

    auto get_load_loops(string name, Region &bounds, Region &region,
                        LastArgument &last_arg, bool is_init = false)
    -> GPUBufInfo::LOOP {
        const auto &info = gpu_bufs[name];
        const auto &inst = is_init ?info.init_inst :info.iter_inst;
        size_t idx  = last_arg.arg_idx;
        size_t iter = last_arg.num_iter;
        if (region.size() == 0) {
            return { Region() };
        }
        internal_assert(inst.rw_len > 0);

        int extent = int_val(region[idx].extent);
        int times = extent / iter;
        size_t rem_sz = (extent - iter*times) * (inst.rw_len/iter);
        if (rem_sz > 0) {
            times += 1;
        }
        Expr addr = get_load_addr(name, bounds, is_init);
        Region loops = { Range(0, times) };

        string suffix = is_init ?"init" :"iter";
        Expr stri = 1;
        Expr offs = 0;
        for (size_t i = 0; i < region.size(); i++) {
            if (i == last_arg.arg_idx) {
                offs += make_var(loop_name(name, suffix, 0)) *int_exp(iter) *stri;
            } else if (is_positive_const(region[i].extent)) {
                offs += make_var(loop_name(name, suffix, loops.size())) *stri;
                loops.push_back(Range(0, region[i].extent));
            }
            stri *= im_bounds[name][i].extent;
        }
        offs = simplify(offs);

        return { loops, addr, offs, rem_sz };
    }

    // Given a conv workload that slides a 3x3 filter over a 2D plane with a stride 1,
    // two rows are loaded initially and one row is loaded at each vertical slide.
    // At the end of a slide, the second/third row can be reused as the first/second row,
    // which saves memory bandwidth to load two rows. We name it copy instruction, like:
    // I(0..1, ...) = I(1..2, ...)
    auto get_copy_range(string name, const Region &init)
    -> GPUBufInfo::COPY {
        if (init.size() == 0) {
            return { Range() };
        }
        const auto &alloc = gpu_bufs[name].allocation;
        internal_assert(!alloc.empty() && init.size() == alloc.size());

        int dest_min = 0, src_min = 0;
        int extent = 1, stride = 1;
        for (size_t i = 0; i < init.size(); i++) {
            size_t init_min = int_val(init[i].min);
            size_t init_ext = int_val(init[i].extent);
            size_t buff_min = int_val(alloc[i].min);
            size_t buff_ext = int_val(alloc[i].extent);

            if (init_ext==0 || init_ext==buff_ext) {
                src_min += init_min * stride;
            } else if (init_min == buff_min) {
                src_min += (buff_min +buff_ext -init_ext) * stride;
            } else if (init_min+init_ext == buff_min+buff_ext) {
                src_min += buff_min * stride;
            }
            dest_min += init_min *stride;
            extent *= (init_ext==0 ?buff_ext :init_ext);
            stride *= buff_ext;
        }

        return { Range(src_min, extent), Range(dest_min, extent) };
    }

    // The index to allocated buffer is determined by inner loops.
    // For example, given an input A(kk+KK*k, ii+II*i) and loops kk, ii, k, i.
    // If insert a buffer below loop k, the index is (kk, ii),
    // which is calculated by replacing all outer loops (k, i) with 0
    Expr get_access_idx(string name) {
        const auto &store_at = gpu_bufs[name].fp.store_at;
        const auto &alloc = gpu_bufs[name].allocation;
        internal_assert(!alloc.empty() && !store_at.empty());

        map<string, Expr> fix_loops;
        for (int i = loop_vars.size()-1; i >= 0; i--) {
            auto var = loop_vars[i];
            fix_loops[var] = 0;
            if (extract_last_token(var) == store_at) {
                break;
            }
        }
        Expr addr = 0;
        Expr stri = 1;
        if (coalesce_vectorized_elements) {
            // TOFIX: vectorized loop is not required to be innermost
            string vec_loop = loop_vars[0];
            fix_loops[vec_loop] = 0;
            addr = make_var(vec_loop);
            stri = loop_bounds[0].extent;
        }
        for (size_t i = 0; i < im_args[name].size(); i++) {
            Expr arg = simplify(substitute(fix_loops, im_args[name][i]));
            if (is_zero(arg)) continue;
            addr += arg * stri;
            stri *= alloc[i].extent;
        }
        return simplify(addr);
    }

public:
    GPUBufferBuilder(const map<string, Function>& _env)
        : env(_env) { }

    void get_buf_info(void) {
        for (auto &pair : env) {
            const string &name = pair.first;
            const Function &func = pair.second;
            if (func.definition().schedule().has_fetch()) {
                GPUBufInfo &info = gpu_bufs[name];

                Region init_bounds, iter_bounds;
                Region alloc_iter, alloc_init;
                LastArgument last_iter, last_init;
                info.fp         = func.definition().schedule().fetch_params();
                get_loop_bounds(name, init_bounds, iter_bounds);
                info.allocation = get_alloc_size(name, loop_bounds);
                alloc_iter      = get_alloc_size(name, iter_bounds);
                alloc_init      = get_alloc_size(name, init_bounds);
                info.iter_inst  = get_load_elem(name, alloc_iter, last_iter);
                info.init_inst  = get_load_elem(name, alloc_init, last_init);
                info.iter_loop  = get_load_loops(name, iter_bounds, alloc_iter, last_iter);
                info.init_loop  = get_load_loops(name, init_bounds, alloc_init, last_init, true);
                info.copy_inst  = get_copy_range(name, alloc_init);
                info.access_idx = get_access_idx(name);
            }
        }
    }
};

class GPUStoreInserter : public IRMutator
{
    const Load  *ori_load;
    const Store *ori_store;
    bool in_output = false;
    bool in_thread = false;
    string output_func;

    Stmt make_store(string name, Expr ld_idx, size_t len) {
        const auto &st_name = ori_store->name;
        const auto &ld_name = ori_load->name;
        const auto &sp = store_func.sp;
        const auto &st = store_func.st_inst;
        Type ld_type = func_info[output_func].func_type.with_lanes(len);

        if (st.type == InstType::MediaBlock) {
            vector<Expr> call_args;
            const auto &elems = st.elem.as<Shuffle>()->vectors;
            const auto &offs  = st.offs.as<Shuffle>()->vectors;

            Expr var_ld = Variable::make(Handle(), ld_name);
            Expr var_st = Variable::make(Handle(), st_name);
            call_args.push_back(var_st);
            for (size_t i = 0; i < offs.size(); i++) {
                Expr var_addr = make_var(addr_name(name, "store", "", i));
                call_args.push_back(var_addr +offs[i]);
            }
            call_args.push_back(var_ld);
            call_args.push_back(ld_idx);
            for (size_t i = 0; i < elems.size(); i++)
                call_args.push_back(elems[i]);

            Expr call = Call::make(ld_type, Call::cm_store_2d, call_args, Call::Intrinsic);
            return Evaluate::make(call);
        }

        Expr load = Load::make(ld_type, ld_name, ld_idx,
                               ori_load->image, ori_load->param, const_true(len), ori_load->alignment);
        Expr var_addr = make_var(addr_name(name, "store"));
        Expr var_elem = make_var(elem_name(name, "store"), sp.rw_len);
        if (len < sp.rw_len)
            var_elem = Shuffle::make_slice(var_elem, 0, 1, len);
        return Store::make(name, load, var_addr +var_elem +st.offs,
                           ori_store->param, const_true(len), ori_store->alignment);
    }
public:
    using IRMutator::visit;
    const map<string, Function> &env;

    GPUStoreInserter(decltype(env) _env)
        : env(_env) {
        for (auto &kv : func_info) {
            Function func;
            internal_assert(function_is_in_environment(kv.first, env, func));
            if (kv.second.is_output && func.definition().schedule().has_store())
                output_func = kv.first;
        }
    }

    Stmt visit(const For *op) override {
        if (output_func.empty())
            return IRMutator::visit(op);

        const auto &output = func_info[output_func];
        const auto &sp = store_func.sp;
        const auto &st = store_func.st_inst;
        const auto &outer = store_func.outer;

        Stmt body;
        body = mutate(op->body);
        // innermost thread loop
        in_thread = in_thread==false && op->for_type==ForType::GPUThread ?true :false;

        if (in_thread && st.type==InstType::Scatter) {
            string let_elem_name = elem_name(output_func, "store");
            body = LetStmt::make(let_elem_name, st.elem, body);
        }
        if (op->name == output.sink_loop) {
            internal_assert(ori_load && ori_store);
            const auto &loop = outer.loop;
            size_t rw_len = sp.rw_len;
            size_t rem_sz = outer.rem_sz;

            auto loop_name = output_outer_loop(output_func);
            Expr loop_var  = make_var(loop_name);
            Expr load_base = loop_var * int_exp(rw_len);
            Expr load_idx  = Ramp::make(load_base, 1, rw_len);
            Stmt store = make_store(output_func, load_idx, rw_len);
            if (rem_sz > 0) {
                Expr rem_load_idx = Ramp::make(load_base, 1, rem_sz);
                Stmt rem_store = make_store(output_func, rem_load_idx, rem_sz);
                Expr rem_cond = simplify(loop_var == loop.extent-1);
                store = IfThenElse::make(rem_cond, rem_store, store);
            }

            Stmt for_node = For::make(loop_name, loop.min, loop.extent,
                                      ForType::Unrolled, op->device_api, store);
            if (st.type == InstType::Scatter)
                for_node = LetStmt::make(addr_name(output_func, "store"), st.addr, for_node);
            else {
                auto &addrs = st.addr.as<Shuffle>()->vectors;
                for (size_t i = 0; i < addrs.size(); i++) {
                    string let_addr_name = addr_name(output_func, "store", "", i);
                    for_node = LetStmt::make(let_addr_name, addrs[i], for_node);
                }
            }
            if (!is_one(output.if_cond))
                for_node = IfThenElse::make(output.if_cond, for_node);
            body = For::make(op->name, op->min, op->extent, op->for_type, op->device_api, body);
            body = Block::make(body, for_node);
            return body;
        }
        return For::make(op->name, op->min, op->extent, op->for_type, op->device_api, body);
    }

    Stmt visit(const Store *op) override {
        if (op->name == output_func) {
            in_output = true;
            ori_load  = op->value.as<Load>();
            ori_store = op;
            return Stmt();
        }
        return IRMutator::visit(op);
    }

    Stmt visit(const IfThenElse *op) override {
        in_output = false;
        Stmt then_case = mutate(op->then_case);
        Stmt else_case = mutate(op->else_case);
        if (!in_output)
            return IfThenElse::make(op->condition, then_case, else_case);
        return then_case;
    }
};

class GPUBufferInserter : public IRMutator
{
public:
    GPUBufferInserter(const map<string, Function>& _env)
        : env(_env) {}

private:
    bool in_kernel = false;
    const map<string, Function> &env;
    vector<string> alloc_buf;
    using IRMutator::visit;

    Stmt make_load_inst(string name, bool is_init, size_t len,
                        Expr load_offs, Expr store_idx) {
        // Eliminate _im suffix
        auto pos = name.find("_im");
        internal_assert(pos != name.npos);
        string image = name.substr(0, pos);

        const auto &info  = gpu_bufs[name];
        const auto &param = image_param[name];
        const auto &ld_inst = is_init ? info.init_inst : info.iter_inst;
        string suffix = is_init ? "init" : "iter";

        // Transform the IR like this:
        // for {
        //   X_buf(store_idx) = X(var_addr + var_elem + load_offs)
        // }
        if (ld_inst.type == InstType::Scatter) {
            Expr var_addr = make_var(addr_name(name, "load", suffix));
            Expr var_elem = make_var(elem_name(name, "load"), ld_inst.rw_len);
            if (len < ld_inst.rw_len) {
                var_elem = Shuffle::make_slice(var_elem, 0, 1, len);
            }
            Type load_t = param.type().with_lanes(len);
            Expr load = Load::make(load_t, image, var_addr + var_elem + load_offs,
                                   Buffer<>(), param, const_true(len), ModulusRemainder());
            return Store::make(buf_name(name), load, store_idx,
                               Parameter(), const_true(len), ModulusRemainder());
        }
        // Load value from X at (var_addr_0, var_addr_1) to X_buf with size (N, M)
        // Transform the IR like this:
        // for {
        //   cm_load_2d(X, var_addr_0, var_addr_1, X_buf, store_idx, N, M);
        // }
        if (ld_inst.type == InstType::MediaBlock) {
            const auto &elems = ld_inst.elem.as<Shuffle>()->vectors;
            internal_assert(elems.size() == 2);

            Expr image_var = Variable::make(Handle(), image);
            Expr buf_var = Variable::make(Handle(), buf_name(name));
            vector<Expr> call_args;
            call_args.push_back(image_var);
            for (int i = 0; i < param.dimensions(); i++) {
                Expr var_addr = make_var(addr_name(name, "load", suffix, i));
                call_args.push_back(simplify(var_addr));
            }
            call_args.push_back(buf_var);
            call_args.push_back(store_idx);
            for (size_t i = 0; i < elems.size(); i++) {
                call_args.push_back(elems[i]);
            }
            Type load_t = param.type().with_lanes(len);
            Expr call = Call::make(load_t, Call::cm_load_2d, call_args, Call::Intrinsic);
            return Evaluate::make(call);
        }

        return Stmt();
    }

    Stmt build_enclosing_loops(string name, bool is_init, const For *op) {
        const auto &info = gpu_bufs[name];
        const auto &loop = is_init ?info.init_loop :info.iter_loop;
        const auto &inst = is_init ?info.init_inst :info.iter_inst;
        string suffix = is_init ?"init" :"iter";

        // Expr buf_base = is_init ?info.copy_inst.dest.min :info.copy_inst.dest.extent;
        Expr buf_offs = 0;
        Expr buf_stri = int_exp(inst.rw_len);
        for (size_t i = 0; i < loop.bounds.size(); i++) {
            buf_offs += make_var(loop_name(name, suffix, i)) *buf_stri;
            buf_stri *= loop.bounds[i].extent;
        }
        // buf_base = simplify(buf_offs + buf_base);
        Expr store_idx = Ramp::make(buf_offs, 1, inst.rw_len);

        // Building the loop body
        Stmt load_inst = make_load_inst(name, is_init, inst.rw_len, loop.offs, store_idx);
        // if (rem_sz > 0) {
        //     Expr rem_store_idx = Ramp::make(buf_base, 1, rem_sz);
        //     Stmt rem_store = make_store(name, is_init, rem_sz, outer.offs, rem_store_idx);
        //     Expr var_loop = make_var(loop_name(name, suffix, 0));
        //     Expr rem_cond = simplify(var_loop == loops[0].extent-1);
        //     store = IfThenElse::make(rem_cond, rem_store, store);
        // }

        // Insert enclosing loops
        Stmt for_node = load_inst;
        for (size_t i = 0; i < loop.bounds.size(); i++) {
            Expr min = loop.bounds[i].min;
            Expr extent = loop.bounds[i].extent;
            for_node = For::make(loop_name(name, suffix, i), min, extent,
                                 ForType::Unrolled, op->device_api, for_node);
        }

        // Insert let statements for address
        string let_addr_name = addr_name(name, "load", suffix);
        if (inst.type == InstType::Scatter) {
            for_node =  LetStmt::make(let_addr_name, loop.addr, for_node);
        }
        if (inst.type == InstType::MediaBlock) {
            auto addrs = loop.addr.as<Shuffle>()->vectors;
            for (size_t i = 0; i < addrs.size(); i++) {
                let_addr_name = addr_name(name, "load", suffix, i);
                for_node = LetStmt::make(let_addr_name, addrs[i], for_node);
            }
        }
        return for_node;
    }

    Stmt visit(const For *op) override {
        Stmt body = mutate(op->body);
        // True at the beginning of a kernel (the first GPU loop)
        in_kernel = (in_kernel==false && op->for_type==ForType::GPUThread) ? true : false;

        for (auto func : gpu_bufs) {
            string name = func.first;
            const auto &info  = func.second;
            // const auto &param = image_param[name];
            const auto &ld_inst = info.iter_inst;
            // const auto &cp_inst = info.copy_inst;

            if (extract_token(op->name, 2) == info.fp.store_at) {
                // Build the loops bottom-up and insert copy instruction
                // size_t sz = val(cp_inst.dest.extent);
                // if (sz > 0) {
                //     Expr dest_idx = Ramp::make(cp_inst.dest.min, 1, sz);
                //     Expr src_idx  = Ramp::make(cp_inst.src.min, 1, sz);
                //     Type type = param.type().with_lanes(sz);
                //     Expr load = Load::make(type, buf_name(name), src_idx,
                //                            Buffer<>(), Parameter(), const_true(sz), ModulusRemainder());
                //     Stmt store = Store::make(buf_name(name), load, dest_idx,
                //                              Parameter(), const_true(sz), ModulusRemainder());
                //     body = Block::make(body, store);
                // }
                // Insert loops enclosing the load instructions
                Stmt iter_for = build_enclosing_loops(name, false, op);
                body = Block::make(iter_for, body);
                // if (sz > 0) {
                //     Stmt init_for = build_fetch_loops(name, true, op->device_api);
                //     out_body = out_body.defined() ?Block::make(out_body, init_for) :init_for;
                // }
                // Buffer allocation is inserted at the beginning of a kernel
                alloc_buf.push_back(name);
            }
            if (in_kernel && ld_inst.type==InstType::Scatter) {
                // The scatter element is inserted at the beginning of a kernel.
                // The initial instruction inserted above the loop should have the same elements
                string let_elem_name = elem_name(name, "load");
                body = LetStmt::make(let_elem_name, ld_inst.elem, body);
            }
        }
        if (in_kernel) {
            // The allocated register buffers are inserted at the beginning of a kernel
            for (auto name : alloc_buf) {
                const auto &info = gpu_bufs[name];
                vector<Expr> buf_size;
                for (size_t i = 0; i < info.allocation.size(); i++) {
                    buf_size.push_back(info.allocation[i].extent);
                }
                body = Allocate::make(buf_name(name), image_param[name].type(),
                                    info.fp.store_in, buf_size, const_true(), body);
            }
        }
        body = For::make(op->name, op->min, op->extent, op->for_type, op->device_api, body);
        // if (out_body.defined()) {
        //     body = Block::make(out_body, body);
        // }
        return body;
    }

    Expr visit(const Load *op) override {
        string name = op->name + "_im";
        // Replace the access to inputs with the access to allocated buffer
        for (auto pair : gpu_bufs) {
            if (name == pair.first) {
                Expr access_idx = gpu_bufs[name].access_idx;
                return Load::make(op->type, buf_name(name), access_idx,
                                  op->image, op->param, op->predicate, op->alignment);
            }
        }
        return IRMutator::visit(op);
    }
};

class AutoUnroll : public IRMutator
{
    const map<string, Function> &env;
    set<string> used_vars;
    bool in_space_loop = false;
    bool in_buffer = false;

public:
    using IRMutator::visit;

    AutoUnroll(decltype(env) _env)
        : env(_env) { }

    Expr visit(const Variable *op) override {
        if (in_buffer)
            used_vars.insert(op->name);
         return op;
    }

    Expr visit(const Load *op) override {
        if (in_space_loop && ends_with(op->name, buf_name())) {
            in_buffer = true;
            mutate(op->index);
            in_buffer = false;
        }
        return IRMutator::visit(op);
    }

    Stmt visit(const Store *op) override {
        if (in_space_loop && func_info.find(op->name) != func_info.end()) {
            if (!func_info[op->name].is_output) {
                in_buffer = true;
                mutate(op->index);
                in_buffer = false;
            }
        }
        return IRMutator::visit(op);
    }

    Stmt visit(const For *op) override {
        if (!space_loops.empty() && op->name == loop_vars[0])
            in_space_loop = true;
        Stmt body = mutate(op->body);
        in_space_loop = false;

        if (used_vars.find(op->name) != used_vars.end()
            && op->for_type == ForType::Serial) {
            return For::make(op->name, op->min, op->extent,
                             ForType::PragmaUnrolled, op->device_api, body);
        }
        return For::make(op->name, op->min, op->extent,
                         op->for_type, op->device_api, body);
    }
};

class GPUInnerProductMatcher : public IRMutator
{
    const map<string, Function> &env;
    struct URE {
        const Store *ori_store;
        Expr init_val;
    };
    map<string, URE> ures;
    bool in_kernel = false;
    bool in_body = false;
    string last_visited_ure;

public:
    using IRMutator::visit;
    GPUInnerProductMatcher(decltype(env) _env)
        : env(_env) {}

    Expr visit(const Select *op) override {
        if (!last_visited_ure.empty()) {
            if (is_const(op->true_value)) {
                ures[last_visited_ure].init_val = op->true_value;
                return mutate(op->false_value);
            }
            if (op->false_value.defined() && is_const(op->false_value)) {
                ures[last_visited_ure].init_val = op->false_value;
                return mutate(op->true_value);
            }
        }
        return IRMutator::visit(op);
    }

    Stmt visit(const Store *op) override {
        auto it = func_info.find(op->name);
        if (it != func_info.end() && !it->second.is_output) {
            last_visited_ure = op->name;
            ures[last_visited_ure] = { op, 0 };
            // Transform the IR like this:
            // Before: Z(j, i) = select(k == 0, 0, Z(j, i)) + A(k, i) * B(j, k)
            // After: Z(j, i) = Z(j, i) + A(k, i) * B(j, k)
            Expr value = mutate(op->value);
            last_visited_ure = "";
            return Store::make(op->name, value, op->index,
                               op->param, op->predicate, op->alignment);
        }
        return IRMutator::visit(op);
    }

    Stmt visit(const For *op) override {
        if (op->name == loop_vars[0]) {
            in_body = true;
        }
        Stmt body = mutate(op->body);
        if (op->name == loop_vars[0]) {
            in_body = false;
        }
        in_kernel = (in_kernel==false && op->for_type==ForType::GPUThread) ?true :false;

        for (auto &kv : ures) {
            const string &name = kv.first;
            URE &ure = kv.second;
            internal_assert(func_info.find(name) != func_info.end());
            // Transform the IR like this:
            // if (...) {
            //   X[ramp(0, 1, size)] = ure.init_val
            // }
            // for (sink_loop, 0, N)
            if (op->name == func_info[name].sink_loop) {
                size_t size = space_loop_extents();
                Expr idx = Ramp::make(0, 1, size);
                Expr val = Broadcast::make(ure.init_val, size);
                Stmt init_ure = Store::make(name, val, idx, ure.ori_store->param,
                                            const_true(size), ModulusRemainder());
                init_ure = IfThenElse::make(func_info[name].if_cond, init_ure);
                Stmt for_node = For::make(op->name, op->min, op->extent,
                                          op->for_type, op->device_api, body);
                return Block::make(init_ure, for_node);
            }
        }
        return For::make(op->name, op->min, op->extent, op->for_type, op->device_api, body);
    }
};

void adapt_loop_info(const Adaptor &stt) {
    // The loop_vars and loop_bounds used in this stage have minor differences
    // from these defined in stt, so we need to re-build loop information.
    for (size_t i = 0; i < stt.loop_vars.size(); i++) {
        string name = stt.loop_vars[i].as<Variable>()->name;
        Expr min = stt.loop_mins[i];
        Expr ext = stt.loop_extents[i];
        loop_vars.push_back(name);
        loop_bounds.push_back(Range(min, ext));
    }
}

class LoopInfoCollector : public IRVisitor
{
public:
    map<string, Expr> new_loop_vars;
    using IRVisitor::visit;

    LoopInfoCollector() {
        for (auto &v : loop_vars) {
            new_loop_vars[v] = 0;
        }
    }

    // After transformation, some variables are decorated with special suffix like
    // ".thread_id_x" for GPU loops, so we need to update their names
    void visit(const For* op) override {
        op->body.accept(this);
        for (auto &v : loop_vars) {
            if (extract_token(op->name, 2) == extract_token(v, 2)) {
                new_loop_vars[v] = Variable::make(Int(32), op->name);
            }
        }
    }
};

void update_loop_vars(Stmt s) {
    LoopInfoCollector lc;
    s.accept(&lc);

    for (auto &b : gpu_bufs) {
        auto &info = b.second;
        info.access_idx = substitute(lc.new_loop_vars, info.access_idx);
        info.iter_loop.addr = substitute(lc.new_loop_vars, info.iter_loop.addr);
        info.init_loop.addr = substitute(lc.new_loop_vars, info.init_loop.addr);
    }
    store_func.st_inst.addr = substitute(lc.new_loop_vars, store_func.st_inst.addr);
}

Stmt do_prepare_memory_schedule(Stmt s, const map<string, Function> &env, const Adaptor &stt) {
    // The loop information is collected before performing space-time transform
    if (!stt.loop_vars.empty()) {
        FuncInfoCollector func_infoc(env);
        adapt_loop_info(stt);
        s.accept(&func_infoc);

        GPUBufferBuilder gb_builder(env);
        GPUStoreBuilder gs_builder(env);
        gb_builder.get_buf_info();
        gs_builder.get_store_func();
    }
    return s;
}

Stmt do_memory_schedule(Stmt s, const map<string, Function> &env) {
    // The loop information is collected before performing space-time transform
    if (!loop_vars.empty()) {
        GPUBufferInserter gb_inserter(env);
        GPUStoreInserter gs_inserter(env);
        AutoUnroll auto_unroll(env);
        GPUInnerProductMatcher gpu_ipm(env);

        update_loop_vars(s);
        s = gb_inserter.mutate(s);
        s = gs_inserter.mutate(s);
        s = auto_unroll.mutate(s);
        s = gpu_ipm.mutate(s);
    }
    return s;
}

}  // namespace Internal
}  // namespace Halide
