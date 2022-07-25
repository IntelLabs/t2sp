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
#include <vector>
#include <algorithm>

#include "../../Halide/src/CSE.h"
#include "../../Halide/src/Func.h"
#include "../../Halide/src/Function.h"
#include "../../Halide/src/IR.h"
#include "../../Halide/src/Simplify_Internal.h"
#include "../../Halide/src/Simplify.h"
#include "../../Halide/src/Substitute.h"
#include "../../Halide/src/IRMutator.h"
#include "./DebugPrint.h"
#include "./NoIfSimplify.h"
#include "./SpaceTimeTransform.h"
#include "./Math.h"
#include "./Utilities.h"

#include "MemorySchedule.h"

namespace Halide {

using namespace Internal;

using std::vector;
using std::map;

// A helper function. If sch_vector_specified is true, the coefficients are the scheduling vector specified by the programmer.
// Otherwise, the cofficients are made up by the compiler.
void space_time_transform_helper(Func &f, const vector<Var> &vars, const vector<int> &coefficients,
                                 SpaceTimeTransform check, bool sch_vector_specified) {
    const auto &func_dims = f.function().definition().schedule().dims();
    // Check if the number of vars exceeds the number of loops variables
    user_assert(vars.size() <= func_dims.size()) << "Too many loops (" << to_string<Var>(vars)
            << ") asked for than what are available in the func.\n";
    // Check if the number of coefficients is the same as the number of variables
    user_assert(vars.size() == coefficients.size()) << "Number of space variables (" << vars.size()
        << ") should be the same as number of coefficients (" << coefficients.size() << ").";

    string time_var;
    for (auto &d : func_dims) {
        auto p = std::find_if(vars.begin(), vars.end(), [&](const Var &v){ return v.name() == d.var; });
        if (p == vars.end()) {
            time_var = d.var;
            break;
        }
    }
    vector<Var> src_vars(vars);
    src_vars.push_back(Var(time_var));
    // Since it projects to n-1 dimension, we get n-1 space loops plus 1 time loop
    vector<Var> dst_vars(vars);
    string t_name = (sch_vector_specified ? "t." : "") + time_var;
    dst_vars.push_back(Var(t_name));

    // Generate allocation matrix
    size_t size = vars.size();
    Matrix::matrix_t alloc(size+1, vector<int>(size+1));
    for (size_t i = 0; i < size; i++) {
        for (size_t j = 0; j < size+1; j++)
            alloc[i][j] = (i == j ? 1 : 0);
    }
    for (size_t j = 0; j < size; j++) {
        alloc[size][j] = coefficients[j];
    }
    alloc[size][size] = 1;

    f.space_time_transform(src_vars, dst_vars, alloc, {}, check);
}

/* Front-end interface */

Func &Func::space_time_transform(const vector<Var> &vars, SpaceTimeTransform check) {
    vector<int> coefficients(vars.size(), 0);
    space_time_transform_helper(*this, vars, coefficients, check, false);
    SpaceTimeTransformParams &params = func.definition().schedule().transform_params()[0];
    params.sch_vector_specified = false;

    // During apply_space_time_transform(), for unscheduled STT, we will simply generate the loop nests the same way
    // when there is no STT specified at all. So make space loops unrolled, and make every loop as a dimension in
    // shift registers. We will minimize the shift registers after replace_references_with_shift_registers() in Lower.cpp.
    // We cannot easily do it during apply_space_time_transform() because we may need to linearize several groups of
    // loops separately, and that makes the shift register indices as linear expressions of loop vars, not necessarily
    // within the bounds of the loops, and that may break bound inference later. So we wil delay the minimization of
    // shift registers after bound inference is no longer needed.
    for (auto v: vars) {
        unroll(v);
    }
    return *this;
}

Func &Func::space_time_transform(const vector<Var> &vars, const vector<int> &coefficients, SpaceTimeTransform check) {
    space_time_transform_helper(*this, vars, coefficients, check, true);
    return *this;
}

class ReverseDefChecker : public IRVisitor {
  public:
    ReverseDefChecker(const vector<std::string> &vec)
        : visited(vec.begin(), vec.end()) {}

    void add_visited_var(const std::string &v) {
        visited.insert(v);
    }

  private:
    using IRVisitor::visit;
    std::set<std::string> visited;

    void visit(const Variable* op) override {
        user_assert(visited.count(op->name))
            << "Your expression refer to a undefined variable " << op->name
            << ". Please make sure the reverse definitions are in correct order.\n";
        IRVisitor::visit(op);
    }
};

Func& Func::space_time_transform(const vector<Var>& src_vars,
                                 const vector<Var>& dst_vars,
                                 const vector<int> &coefficients,
                                 SpaceTimeTransform check) {
    vector<vector<int>> new_coefficients;
    size_t src_size = src_vars.size();
    size_t dst_size = dst_vars.size();
    user_assert(coefficients.size() == src_size * dst_size);
    for (size_t i = 0; i < dst_size; i++) {
        new_coefficients.push_back(vector<int>(coefficients.begin()+i*src_size,
                                   coefficients.begin()+(i+1)*src_size));
    }
    return space_time_transform(src_vars, dst_vars, new_coefficients, {}, check);
}

Func& Func::space_time_transform(const vector<Var>& src_vars,
                                 const vector<Var>& dst_vars,
                                 const vector<int> &coefficients,
                                 const vector<Expr> &reverse,
                                 SpaceTimeTransform check) {
    vector<vector<int>> new_coefficients;
    vector<std::pair<Expr, Expr>> new_reverse;
    size_t src_size = src_vars.size();
    size_t dst_size = dst_vars.size();
    user_assert(coefficients.size() == src_size * dst_size);
    user_assert(reverse.size() == src_size * 2);

    for (size_t i = 0; i < dst_size; i++) {
        new_coefficients.push_back(vector<int>(coefficients.begin()+i*src_size,
                                   coefficients.begin()+(i+1)*src_size));
    }
    for (size_t i = 0; i < src_size; i++) {
        new_reverse.push_back(std::make_pair(reverse[i*2], reverse[i*2+1]));
    }
    return space_time_transform(src_vars, dst_vars, new_coefficients, new_reverse, check);
}

Func& Func::space_time_transform(const vector<Var>& src_vars,
                                 const vector<Var>& dst_vars,
                                 const vector<vector<int>> &coefficients,
                                 const vector<std::pair<Expr, Expr>> &reverse,
                                 SpaceTimeTransform check) {
    // Add the transformation information to the function
    auto& param_vector = func.definition().schedule().transform_params();
    user_assert(param_vector.size() == 0)
        << "Do not support invoking stt multiple times, "
        << "please consider merging multiple projections into a single one.\n";

    size_t src_size = src_vars.size();
    size_t dst_size = dst_vars.size();
    user_assert(dst_size <= src_size);

    // Check if the number of vars exceeds the number of loops variables
    user_assert(src_size <= this->args().size())
        << "Number of space variables (" << src_size
        << ") exceeds the number of loop variables (" << this->args().size()<< ").";

    vector<std::string> src_names(src_size);
    vector<std::string> dst_names(dst_size);
    for (size_t i = 0; i < src_size; i++) {
        src_names[i] = src_vars[i].name();
    }
    for (size_t i = 0; i < dst_size; i++) {
        dst_names[i] = dst_vars[i].name();
    }

    // Check if the number of coefficients is the same as the number of variables
    user_assert(dst_size == coefficients.size()
                && src_size == coefficients[0].size())
        << "Any of " << dst_size << " destination variables"
        << " should have " << src_size << " coefficients";

    vector<vector<int> > proj_matrix(coefficients.begin(),
                                     coefficients.begin() + dst_size - 1);
    vector<int> sch_vector = coefficients[dst_size-1];
    map<string, Expr> new_reverse;

    if (reverse.size() == 0) {
        // The source variables form a identity matrix I
        // The allocation matrix P*I = A (the destination variables)
        // So, a matrix B satisfying B*A = I is what we want. It's P^-1!
        Matrix::matrix_t inv(dst_size, vector<int>(src_size));
        user_assert(Matrix::get_inverse(coefficients, inv))
            << "Cannot compute the reverse transformation for your allocation matrix.\n"
               "Please consider manually specifying it with the following format\n"
            << "{ {source varible 1, definition},\n"
               "  {source varible 2, definition}, ...}\n";

        // Build the corresponding expression
        for (size_t i = 0; i < src_size; i++) {
            Expr def = IntImm::make(Int(32), 0);
            for (size_t j = 0; j < dst_size; j++) {
                if (inv[i][j] != 0)
                    def += IntImm::make(Int(32), inv[i][j]) * dst_vars[j];
            }
            new_reverse.insert({ src_vars[i].name(), simplify(def) });
            debug(3) << "Var " << src_vars[i].name()
                     << " = "  << def << "\n";
        }
    } else {
        user_assert(reverse.size() == src_size)
            << "Every source variable requires a reverse definition using destination variable";
        ReverseDefChecker rev_chk(dst_names);
        for (size_t i = 0; i < reverse.size(); i++) {
            const Variable *src_var = reverse[i].first.as<Variable>();
            bool is_found = false;
            for (const Var& v : src_vars) {
                if (src_var && v.name() == src_var->name)
                    is_found = true;
            }
            user_assert(is_found)
                << "The reverse definitions should have the following format:\n"
                << "{ {source varible 1, definition},\n"
                   "  {source varible 2, definition}, ...}\n";

            // Check if the variable used in the expr has been defined previously
            reverse[i].second.accept(&rev_chk);
            rev_chk.add_visited_var(src_var->name);
            // Eliminate reference to previously defined variables
            Expr def = substitute(new_reverse, reverse[i].second);
            new_reverse.insert({ src_var->name, def});
            debug(3) << "Var " << src_var->name
                     << " = "  << reverse[i].second << "\n";
        }
    }

    SpaceTimeTransformParams params;
    params.num_space_vars   = src_size - 1;
    params.sch_vector       = sch_vector;
    params.src_vars         = src_names;
    params.dst_vars         = dst_names;
    params.proj_matrix      = proj_matrix;
    params.reverse          = new_reverse;
    params.check_time       = check;
    params.sch_vector_specified = true;
    param_vector.push_back(params);

    return *this;
}

/* Back-end lowering pass */

namespace Internal {
namespace {

class SelectToIfConverter : public IRMutator {
  public:
    SelectToIfConverter() {}

  private:
    using IRMutator::visit;

    Stmt visit(const Provide* op) override {
        if (op->values.size() == 1) {
            if (const Select* sel = op->values[0].as<Select>()) {
                if (!sel->false_value.defined()) {
                    Stmt body = Provide::make(op->name, {sel->true_value}, op->args);
                    return IfThenElse::make(sel->condition, body);
                }
            }
        }
        return IRMutator::visit(op);
    }
};

class SpaceTimeTransformer : public IRMutator {
    using IRMutator::visit;
  public:
    SpaceTimeTransformer(std::map<std::string, Function>& _env)
        : env(_env), target(get_host_target()) {}

    SpaceTimeTransformer(std::map<std::string, Function>& _env,
                         const Target& _t)
        : env(_env), target(_t) {}

    std::map<std::string, RegBound > reg_size_map;
    std::map<std::string, std::vector<Expr>> reg_annotation_map;
    size_t for_level;
    vector<Expr> loop_vars;
    vector<Expr> loop_mins;
    vector<Expr> loop_extents;

  private:
    std::map<std::string, Function> &env;
    const Target &target;
    // Space-time transformation related params
    bool in_scheduled_stt;
    bool need_rewrite;
    std::string vectorized_loop_name;
    vector<SpaceTimeTransformParams> param_vector;
    size_t num_args;
    size_t num_new_args;
    size_t num_time_vars;
    size_t num_space_vars;
    size_t num_other_vars;
    size_t num_new_space_vars;
    // The current for loop level, inner-most is 0
    // loop_mins[0] is the loop min of the inner-most loop
    std::map<std::string, Expr> global_min;
    std::map<std::string, Expr> global_max;
    size_t new_for_level;
    vector<Expr> new_loop_vars;
    vector<Expr> new_loop_mins;
    vector<Expr> new_loop_extents; // loop extents after transformation
    vector<int> loop_var_pos;
    vector<int> src_var_pos;
    // A list of variables that will be realized
    std::map<std::string, std::vector<Type>> realize_types;
    // A list of variables that are used
    std::map<std::string, bool> used_vars;
    // Check whether in isolated kernels
    bool in_input_or_output{true};

    // Helper functions
    bool recalculate_func_referrence_args(std::string func_name, vector<Expr>& args) {
        // special case when every loops are space loops
        // if (num_space_vars == num_args) return;
        // reorder the args if necessary
        vector<Expr> new_args = args;
        for (size_t i = 0; i < num_args; i++) {
            new_args[i] = args[loop_var_pos[i]];
        }
        // first, handle the time loops that are not transformed
        SpaceTimeTransformParams &param = param_vector[0];
        size_t size = param.sch_vector.size();
        args.resize(num_args, 0);
        debug(3) << func_name << "(";

        // modify the index for space loops
        size_t t = num_new_space_vars;
        for (size_t i = 0; i < t; i++) {
            Expr space_expr = IntImm::make(Int(32), 0);
            for (size_t j = 0; j < size; j++) {
                size_t k = src_var_pos[j];
                space_expr += (loop_vars[k] - new_args[k]) * param.proj_matrix[i][j];
            }
            space_expr = new_loop_vars[i] - space_expr;
            args[i] = simplify(space_expr);
            debug(3) << args[i] << ", ";
        }

        // modify the index for time loops
        Expr time_expr = IntImm::make(Int(32), 0);
        for (size_t j = 0; j < size; j++) {
            size_t k = src_var_pos[j];
            Expr diff = simplify(loop_vars[k] - new_args[k]);
            if (can_prove(diff >= loop_extents[k])) {
                // The dependence distance on a dimension must be smaller than its extent.
                // For example, the dependence jj+JJ-1, j-1 where J = 1 is illegal,
                // and thus we skip to calculate distance in such cases
                time_expr = 0;
                break;
            }
            time_expr += diff * param.sch_vector[j];
            debug(3) << "time expr: " << time_expr;
            time_expr = simplify(time_expr);
        }
        args[t] = is_zero(time_expr) ? 0 : simplify(time_expr -1);
        debug(3) << args[t] << ", ";

        for (size_t i = t+1; i < num_args; i++) {
            args[i] = 0;
            debug(3) << args[i] << ", ";
        }
        debug(3) << ")\n";

        // update the size of the shift register
        Expr min_expr = Select::make(args[t] < reg_size_map[func_name].mins[t],
                                args[t], reg_size_map[func_name].mins[t]);
        Expr max_expr = Select::make(args[t] > reg_size_map[func_name].maxs[t],
                                args[t], reg_size_map[func_name].maxs[t]);
        // min_expr = Min::make(substitute(global_max, min_expr), substitute(global_min, min_expr));
        // ext_expr = Max::make(substitute(global_max, ext_expr), substitute(global_min, ext_expr));
        reg_size_map[func_name].mins[t] = simplify(min_expr);
        reg_size_map[func_name].maxs[t] = simplify(max_expr);
        debug(3) << "the time size: ("
                 << reg_size_map[func_name].mins[t] << ".."
                 << reg_size_map[func_name].maxs[t] << ")\n";

        return is_zero(time_expr);
    }

    // Helper Classes
    class LoopInfoCollector : public IRVisitor {
      public:
        LoopInfoCollector(size_t for_level, vector<Expr>& loop_vars, vector<Expr>& loop_mins,
                vector<Expr>& loop_extents, std::map<std::string, Expr>& global_min,
                std::map<std::string, Expr>& global_max)
                : for_level(for_level), loop_vars(loop_vars),
                loop_mins(loop_mins), loop_extents(loop_extents),
                global_min(global_min), global_max(global_max) {}

      private:
        using IRVisitor::visit;
        size_t for_level;
        vector<Expr>& loop_vars;
        vector<Expr>& loop_mins;
        vector<Expr>& loop_extents;

        vector<std::string> current_loops;
        std::map<std::string, Expr>& global_min;
        std::map<std::string, Expr>& global_max;

        void visit(const For* op) override {
            for_level = for_level - 1;
            loop_vars[for_level] = Variable::make(Int(32), op->name);
            Expr extent = op->extent;
            Expr loop_min = op->min;
            loop_mins[for_level] = loop_min;
            loop_extents[for_level] = extent;

            if (extent.as<IntImm>() == nullptr && !global_max.empty()) {
                Expr max_substitute = substitute(global_max, extent);
                Expr min_substitute = substitute(global_min, extent);
                extent = Max::make(max_substitute, min_substitute);
                max_substitute = substitute(global_max, loop_min);
                min_substitute = substitute(global_min, loop_min);
                loop_min = Min::make(max_substitute, min_substitute);
            }
            global_max[op->name] = simplify(loop_min+extent-1);
            global_min[op->name] = simplify(loop_min);

            current_loops.push_back(op->name);
            op->body.accept(this);
            current_loops.pop_back();
            for_level = for_level + 1;
        }

        void visit(const Let *op) override {
            Expr max_substitute = substitute(global_max, op->value);
            Expr min_substitute = substitute(global_min, op->value);
            Expr max_value = Max::make(max_substitute, min_substitute);
            Expr min_value = Min::make(max_substitute, min_substitute);
            global_max[op->name] = simplify(max_value);
            global_min[op->name] = simplify(min_value);
            // May have scope problem
            op->body.accept(this);
        }

        void visit(const LetStmt *op) override {
            Expr max_substitute = substitute(global_max, op->value);
            Expr min_substitute = substitute(global_min, op->value);
            Expr max_value = Max::make(max_substitute, min_substitute);
            Expr min_value = Min::make(max_substitute, min_substitute);
            global_max[op->name] = simplify(max_value);
            global_min[op->name] = simplify(min_value);
            op->body.accept(this);

            for_level = for_level + 1;
        }
    };

    Stmt visit(const Realize* op) override {
        realize_types[op->name] = op->types;
        Stmt body = mutate(op->body);
        Region bounds = op->bounds;

        if (reg_size_map.find(op->name) != reg_size_map.end()) {
            bounds.clear();
            auto &mins = reg_size_map[op->name].mins;
            auto &maxs = reg_size_map[op->name].maxs;
            for (size_t i = 0; i < mins.size(); i++)
                bounds.emplace_back(simplify(mins[i]), simplify(maxs[i]+1));
        }
        body = Realize::make(op->name, op->types, op->memory_type, bounds, op->condition, body);
        // annotation to realizing shift registers
        if (reg_annotation_map.find(op->name) !=  reg_annotation_map.end()) {
            std::vector<Expr> args = {StringImm::make("Bounds"), StringImm::make(op->name)};
            args.insert(args.end(), reg_annotation_map[op->name].begin(), reg_annotation_map[op->name].end());
            Expr annotation = Call::make(Int(32), Call::annotate, args, Call::Intrinsic);
            body = Block::make(Evaluate::make(annotation), body);
        }
        return body;
    }

    Stmt visit(const ProducerConsumer* op) override {
        Function func;
        need_rewrite = false;
        if (op->is_producer // we only care about producer
            && function_is_in_environment(op->name, env, func)
            && func.definition().schedule().transform_params().size() > 0) {
            // collect the information for later processing
            param_vector = func.definition().schedule().transform_params();
            in_scheduled_stt = param_vector[0].sch_vector_specified;
            need_rewrite = true;
            if (!in_scheduled_stt && target.has_feature(Target::IntelFPGA)) {
                // offload to minimize_shift_reg phase on FPGAs
                need_rewrite = false;
            }
            // calculate the number of space loops and new time loops
            num_args                = func.args().size();
            num_time_vars           = 1;
            num_space_vars          = param_vector[0].num_space_vars;
            num_other_vars          = num_args - num_space_vars - num_time_vars;
            num_new_space_vars      = param_vector[0].dst_vars.size() - num_time_vars;
            num_new_args            = param_vector[0].dst_vars.size() + num_other_vars;
            internal_assert(num_space_vars > 0);
            // initialize useful variables
            for_level               = num_args;
            new_for_level           = num_new_args;
            vectorized_loop_name    = "";
            loop_vars.resize(for_level);
            loop_mins.resize(for_level);
            loop_extents.resize(for_level);
            new_loop_vars.resize(new_for_level);
            new_loop_mins.resize(new_for_level);
            new_loop_extents.resize(new_for_level);
            loop_var_pos.resize(for_level, -1);
            LoopInfoCollector visitor(num_args, loop_vars, loop_mins,
                                      loop_extents, global_min, global_max);
            op->body.accept(&visitor);
            // Is the space vars' mins or extents not constant? Used only for unscheduled stt.
            bool space_is_dynamic = false;
            if (!need_rewrite) {
                for (size_t i = 0; i < num_space_vars; i++) {
                    if (!is_const(loop_mins[i]) || !is_const(loop_extents[i])) {
                        space_is_dynamic = true;
                        break;
                    }
                }
            }
            in_input_or_output = func.definition().schedule().is_output() || func.definition().schedule().is_input();
            for (auto &kv : realize_types) {
                Function merged_func;
                const string merged_func_name = kv.first;
                if (function_is_in_environment(merged_func_name, env, merged_func)
                    && (merged_func.definition().schedule().is_merged() || merged_func.has_merged_defs())
                    && !merged_func.definition().schedule().is_output()
                    && !merged_func.definition().schedule().is_input()
                    && reg_size_map.find(merged_func_name) == reg_size_map.end()) {
                    if (need_rewrite) {
                        vector<Expr> reg_size;
                        reg_size.resize(num_args, 0);
                        reg_size_map[merged_func_name].mins = reg_size;
                        reg_size_map[merged_func_name].maxs = reg_size;
                    } else {
                        // Let the register bounds the same as the corresponding loop bounds (In unscheduled stt,
                        // at this moment, we do not change loops at all). We will minimize the bounds in a later phase.
                        for (size_t j = 0; j < merged_func.args().size(); j++) {
                            for (size_t k = 0; k < num_args; k++) {
                                const Variable *loop_var = loop_vars[k].as<Variable>();
                                if (extract_last_token(loop_var->name) == merged_func.args()[j]) {
                                    Expr min_expr = Min::make(substitute(global_max, loop_mins[k]), substitute(global_min, loop_mins[k]));
                                    Expr ext_expr = Max::make(substitute(global_max, loop_extents[k]), substitute(global_min, loop_extents[k]));
                                    reg_size_map[merged_func_name].mins.push_back(min_expr);
                                    reg_size_map[merged_func_name].maxs.push_back(simplify(ext_expr - 1));
                                    break;
                                }
                            }
                        }
                        if (!in_input_or_output) {
                            if (space_is_dynamic) {
                                std::vector<Expr> reg_vars(loop_vars.begin(), loop_vars.begin() + num_space_vars);
                                reg_annotation_map[merged_func_name] = reg_vars;
                            }
                        }
                    }
                    merged_func.has_shift_reg(true);
                }
            }
            auto &src_vars = param_vector[0].src_vars;
            src_var_pos.resize(src_vars.size(), -1);
            for (size_t i = 0; i < src_vars.size(); i++) {
                for (size_t j = 0; j < loop_vars.size(); j++) {
                    auto p = loop_vars[j].as<Variable>();
                    internal_assert(p);
                    if (extract_last_token(p->name) == src_vars[i]) {
                        src_var_pos[i] = j;
                        break;
                    }
                }
            }
            used_vars.clear();
        }
        return IRMutator::visit(op);
    }

    class EliminateTemp : public IRMutator {
        string func;
        bool eliminate_temp = true;
        vector<string> &temp_regs;
        const SpaceTimeTransformer &stt;
    public:
        using IRMutator::visit;
        EliminateTemp(string &_f, vector<string> &_t, const SpaceTimeTransformer &_s)
            : func(_f), temp_regs(_t), stt(_s) {}

        // The lhs refers to the new value and the rhs refers to the last value:
        // W_temp(i, 0) = select(i == 0, w(j), W(i, 0))
        // If W(i, 0) is not needed anymore, we can simply overwrite it as: W(i, 0) = select(...)
        // Otherwise, like W_temp(i, 0) = select(i == 0, w(j), W(i-1, 0))
        // W(i, 0) is needed by W(i+1, 0). Overwriting is wrong if loop i is sequentially executed.
        // In general, skip overwriting if a positive distance on space loops exists.
        Stmt visit(const Provide *op) override {
            if (op->name == func + "_temp") {
                // Step 1: determine to overwrite or not
                eliminate_temp = true;
                for (size_t i = 0; i < op->values.size(); i++) {
                    mutate(op->values[i]);
                }
                if (eliminate_temp) {
                    // Step 2: rewrite call nodes
                    vector<Expr> values(op->values.size());
                    temp_regs.erase(find(temp_regs.begin(), temp_regs.end(), func));
                    for (size_t i = 0; i < op->values.size(); i++) {
                        values[i] = mutate(op->values[i]);
                    }
                    // Step 3: rewrite the current node
                    string name = op->name.substr(0, op->name.size()-5);
                    vector<Expr> ori_args(op->args.begin(), op->args.end());
                    ori_args.resize(stt.num_args, 0);
                    return Provide::make(name, values, ori_args);
                }
            }
            return IRMutator::visit(op);
        }

        Expr visit(const Call *op) override {
            if (op->name == func + "_temp") {
                // If no left-hand side contains this variable, eliminate it
                auto pos = find(temp_regs.begin(), temp_regs.end(), func);
                if (pos == temp_regs.end()) {
                    vector<Expr> ori_args(op->args.begin(), op->args.end());
                    ori_args.resize(stt.num_args, 0);
                    return Call::make(op->type, func, ori_args, op->call_type);
                }
            }
            if (op->name == func) {
                // Note that it is valid if both lhs and rhs refers to the new value:
                // W_temp(i, 0) = select(i == 0, w(j), W_temp(i-1, 0))
                // So we only check positive dependence on the last value (W(i-1, 0) in this case)
                for (size_t i = 0; i < stt.num_new_space_vars; i++) {
                    Expr distance = simplify(stt.new_loop_vars[i] - op->args[i]);
                    if (is_positive_const(distance)) {
                        eliminate_temp = false;
                        return op;
                    }
                }
            }
            return IRMutator::visit(op);
        }
    };

    void process_new_loop_vars(const For *op) {
        SpaceTimeTransformParams &param = param_vector[0];
        // Add new space loops
        for (size_t k = 0; k < num_new_space_vars; k++) {
            size_t size = param.sch_vector.size();
            // 1. Create new space loop with the given variable
            string name = extract_first_token(op->name) + ".s0." + param.dst_vars[k];
            new_loop_vars[k] = Variable::make(Int(32), name);
            // 2. Calculate the new bound
            Expr min = 0;
            Expr extent = 0;
            for (size_t i = 0; i < size; i++) {
                int coeff = param.proj_matrix[k][i];
                int j = src_var_pos[i];
                if (coeff < 0) {
                    min = min + coeff * (loop_extents[j]-1);
                    extent = extent + coeff * loop_mins[j];
                } else {
                    min = min + coeff * loop_mins[j];
                    extent = extent + coeff * (loop_extents[j]-1);
                }
            }
            min = simplify(min);
            extent = simplify(extent + 1);
            new_loop_mins[k] = min;
            new_loop_extents[k] = extent;
            debug(3) << "loop: " << name << "("
                                    << min << ".."
                                    << extent << ")\n";
            // 3. set the register size to be the same as the loop bound
            for (auto &kv : reg_size_map) {
                if (!in_input_or_output) {
                    kv.second.mins[k] = min;
                    kv.second.maxs[k] = extent-1;
                    // if (!is_const(min) || !is_const(extent))
                    //     reg_annotation_map[kv.first] = {};
                }
            }
        }
        // If the register size is not a const(caused by non-constant space loop extent),
        // we need to add an annotation before realizing registers
        // and handle this case differently in CodeGen.
        for (auto &kv : reg_annotation_map) {
            if (kv.second.empty()) {
                std::vector<Expr> reg_vars(new_loop_vars.begin(), new_loop_vars.begin() + num_new_space_vars);
                kv.second = reg_vars;
            }
        }

        // Add new time loop
        // 1. Create new time loop with the given variable
        std::string loop_name = extract_first_token(op->name) + ".s0." + param.dst_vars.back();
        new_loop_vars[num_new_space_vars] = Variable::make(Int(32), loop_name);
        // 2. Calculate the new bound
        Expr min = 0;
        Expr extent = 0;
        for (size_t i = 0; i < num_space_vars + 1; i++) {
            int coeff = param.sch_vector[i];
            int j = src_var_pos[i];
            if (coeff < 0) {
                min += coeff * (loop_extents[j]-1);
                extent += coeff * loop_mins[j];
            } else {
                min += coeff * loop_mins[j];
                extent += coeff * (loop_extents[j]-1);
            }
        }
        min = simplify(min);
        extent = simplify(extent + 1);
        new_loop_mins[num_new_space_vars] = min;
        new_loop_extents[num_new_space_vars] = extent;
        debug(3) << "loop: " << loop_name << "("
                             << min << ".."
                             << extent << ")\n";
    }

    Stmt process_time_loop(const For *op, Stmt body) {
        // We would generate shift register logic like this:
        // unrolled for (i, 0, N-1) // extend is N-1
        //   sr[N-i] = sr[N-i-1]
        if (reg_size_map.size() > 0) {
            Stmt sr_body = Stmt();
            vector<string> temp_regs;
            for (auto kv : reg_size_map) {
                std::string name = kv.first;
                // check if the variable is used within the body
                if (used_vars.find(name) == used_vars.end()) continue;
                // the time dimension
                Expr reg_min = kv.second.mins[num_new_space_vars];
                Expr reg_max = kv.second.maxs[num_new_space_vars];
                Expr time_distance = simplify(reg_max - reg_min);
                temp_regs.push_back(name);
                if (is_zero(time_distance)) {
                    // allocate only one register that can be reused for new value
                    // so eliminate temp registers allocated previously
                    EliminateTemp rewriter(name, temp_regs, *this);
                    body = rewriter.mutate(body);
                    continue;
                }
                // create a new loop var
                // Note: below we give the new loop var's func prefix as some fake func
                // name, instead of the current func's name, because this loop is not
                // from the current func. Otherwise, the bound inference later will break
                // as it will use this loop var to infer the bounds of the current func.
                std::string sr_name = unique_name("dummy") + ".s0.t";
                Expr sr_var = Variable::make(Int(32), sr_name);
                // build the for loop body
                vector<Expr> provide_args;
                vector<Expr> call_args;
                for (size_t i = 0; i < num_new_space_vars; i++) {
                    debug(3) << "new_loop_vars " << new_loop_vars[i] << "\n";
                    provide_args.push_back(new_loop_vars[i]);
                    call_args.push_back(new_loop_vars[i]);
                }
                provide_args.push_back(simplify(time_distance -sr_var));
                call_args.push_back(simplify(time_distance -1 -sr_var));
                for (size_t i = num_new_space_vars+1; i < num_args; i++) {
                    provide_args.push_back(0);
                    call_args.push_back(0);
                }
                vector<Expr> calls;
                for (size_t k = 0; k < realize_types[name].size(); k++) {
                    Type t = (realize_types[name])[k];
                    Expr call = Call::make(t, name, call_args, Call::CallType::Halide,
                                           FunctionPtr(), k, Buffer<>(), Parameter());
                    calls.push_back(call);
                }

                Stmt provide = Provide::make(name, calls, provide_args);
                // build the for loop
                Stmt loop_body = For::make(sr_name, reg_min, reg_max,
                                           ForType::Unrolled, DeviceAPI::None, provide);
                if (sr_body.defined()) {
                    sr_body = Block::make(loop_body, sr_body);
                } else {
                    sr_body = loop_body;
                }
            }
            // store the value of temporary register into the corresponding shift register
            for (auto& name : temp_regs) {
                vector<Expr> args(new_loop_vars.begin(), new_loop_vars.begin() + num_new_space_vars);
                string temp_name = name + "_temp";
                Type func_type = env.at(name).output_types()[0];
                Expr tmp_call = Call::make(func_type, temp_name, args, Call::CallType::Halide);
                args.resize(num_args, 0);
                Stmt tmp_prov = Provide::make(name, {tmp_call}, args);
                sr_body = sr_body.defined() ? Block::make(sr_body, tmp_prov) : tmp_prov;
            }
            // add the space loop
            if (sr_body.defined()) {
                // add space loops
                for (size_t i = 0; i < num_new_space_vars; i++) {
                    std::string sp_name = new_loop_vars[i].as<Variable>()->name;
                    vector<std::string> names = split_string(sp_name, ".");
                    // Note: below we give the new loop var's func prefix as some fake func
                    // name, instead of the current func's name, because this loop is not
                    // from the current func.
                    std::string var_name = unique_name("dummy") + ".s0.s." + names[names.size()-1];
                    sr_body = substitute(sp_name, Variable::make(Int(32), var_name), sr_body);
                    ForType ftype = (i == 0 && target.has_feature(Target::IntelGPU))
                                        ? ForType::Vectorized :ForType::Unrolled;
                    sr_body = For::make(var_name, new_loop_mins[i], new_loop_extents[i],
                                        ftype, DeviceAPI::None, sr_body);
                }
                // merge with the main body
                body = Block::make(body, sr_body);
                for (auto& name : temp_regs) {
                    // realize registers to temporarily save value generated by systolic array
                    Region bounds;
                    RegBound reg_bounds;
                    for (size_t i = 0; i < num_new_space_vars; i++) {
                        Expr min = reg_size_map[name].mins[i];
                        Expr max = simplify(reg_size_map[name].maxs[i]+1);
                        bounds.emplace_back(min, max);
                        reg_bounds.mins.emplace_back(min);
                        reg_bounds.maxs.emplace_back(max);
                    }
                    string temp_name = name + "_temp";
                    Function original = env.at(name);
                    body = Realize::make(temp_name, original.output_types(), MemoryType::Auto,
                                        bounds, const_true(), body);
                    reg_size_map[temp_name] = reg_bounds;
                    // create a new function temp_name and initialize it
                    vector<Var> args;
                    for (size_t i = 0; i < num_new_space_vars; i++) {
                        string name = new_loop_vars[i].as<Variable>()->name;
                        args.push_back(name);
                    }
                    Func temp(temp_name, original.place());
                    temp(args) = 0;
                    temp.compute_at(original.schedule().compute_level());
                    temp.function().lock_loop_levels();
                    env.insert({ temp_name, temp.function() });
                }
            }
        }
        if (in_scheduled_stt) {
            // Re-build time loop in scheduled space-time transform
            size_t t = num_new_space_vars;
            string name = new_loop_vars[t].as<Variable>()->name;
            body = For::make(name, new_loop_mins[t], new_loop_extents[t],
                            ForType::Serial, op->device_api, body);
        }
        return body;
    }

    Stmt process_space_loop(const For *op, Stmt body) {
        // modify the innermost loop body
        SpaceTimeTransformParams &param = param_vector[0];
        if (param.check_time == SpaceTimeTransform::CheckTime) {
            for (size_t k = 0; k <= num_space_vars; k++) {
                Expr condition = (loop_vars[k] >= loop_mins[k])
                                && (loop_vars[k] < loop_mins[k] + loop_extents[k]);
                body = IfThenElse::make(simplify(condition), body);
            }
        }
        // update variables to the decorated form
        map<string, Expr> var_map;
        for (size_t k = 0; k < param.dst_vars.size(); k++) {
            string var = param.dst_vars[k];
            var_map.insert({ var, new_loop_vars[k] });
        }
        // insert reverse definition
        for (auto &p : param.reverse) {
            string name = extract_first_token(op->name) + ".s0." + p.first;
            Expr def = substitute(var_map, p.second);
            body = LetStmt::make(name, def, body);
        }
        if (in_scheduled_stt) {
            // In scheduled space-time transform, space loops are rebuilt according to projection matrix,
            // which is required to be consecutive starting from the innermost level.
            for (size_t k = 0; k < num_new_space_vars; k++) {
                string name = new_loop_vars[k].as<Variable>()->name;
                // Need to set the space loop as unrolled or vectorized: for IntelGPU and FPGA,
                // set the loop as Vectorized if the loop is at innermost level after stt and is
                // marked as Vectorized originally; for all the other cases, set the
                // loop as Unrolled.
                ForType for_type = target.has_feature(Target::IntelFPGA) ? ForType::Unrolled : ForType::Serial;
                if (k == 0) {
                    if ((target.has_feature(Target::IntelFPGA) || (target.has_feature(Target::OneAPI) && !target.has_feature(Target::IntelGPU))) && vectorized_loop_name != "") {\
                        debug(4) << "Vectorize loop: " << vectorized_loop_name << "\n";
                        internal_assert(extract_last_token(vectorized_loop_name) == param.dst_vars[k])
                        << "After space time transformation, the vectorized loop "
                        << vectorized_loop_name
                        << " must be at the inner-most level. \n"
                        << R"(e.g. The original loop oreder is {k, j, i} and we apply sapce time transformation like
A.space_time_transform({k, j},
               {j, t},
               {0, 1,
                1, 0},
               {k, t,
                j, j}))"
                        << "\nWe can not vectorize loop k which is not inner-most loop after stt. \n";
                        for_type = ForType::Vectorized;
                    }
                    if (target.has_feature(Target::IntelGPU)) {
                        // automatically vectorize the innermost space loop even if not specified
                        // for_type = ForType::Vectorized;
                    }
                }
                body = For::make(name, new_loop_mins[k], new_loop_extents[k],
                                 for_type, op->device_api, body);
            }
        }
        return body;
    }

    Stmt visit(const For* op) override {
        if (need_rewrite) {
            for_level -= 1;

            // Special handling of vectorized loop
            if (op->for_type == ForType::Vectorized) {
                internal_assert(vectorized_loop_name == "") << "Cannot vectorize more than one loops: "
                                                            << vectorized_loop_name << " " << op->name << "\n";
                vectorized_loop_name = op->name;
                debug(3) << "loop " << op->name << " is vectorized. \n";
            }
            if (for_level == 0) {
                // Add new variables used for transforming loop body
                process_new_loop_vars(op);
            }
            Stmt body = mutate(op->body);
            auto src_vars = param_vector[0].src_vars;
            // The time loop is the first non-space loop starting from innermost,
            // which is recorded by the one following the last space var.
            size_t time_var = src_var_pos[param_vector[0].num_space_vars];
            if (for_level == 0) {
                body = process_space_loop(op, body);
            }
            if (for_level == time_var) {
                body = process_time_loop(op, body);
            }
            // Skipping the original loops that are transformed into new space/time loops
            if (in_scheduled_stt) {
                auto end = src_var_pos.begin() + num_space_vars + num_time_vars;
                auto pos = std::find(src_var_pos.begin(), end, for_level);
                if (pos != end) {
                    for_level += 1;
                    return body;
                }
            }
            for_level += 1;
            return For::make(op->name, op->min, op->extent, op->for_type, op->device_api, body);
        }
        return IRMutator::visit(op);
    }

    Expr visit(const Call* op) override {
        Function func;
        vector<Expr> args = op->args;

        if (need_rewrite) {
            if (op->call_type == Call::CallType::Halide) {
                if (function_is_in_environment(op->name, env, func)
                && (func.definition().schedule().is_merged() || func.has_merged_defs())
                && !func.definition().schedule().is_output()
                && !func.definition().schedule().is_input()) {
                    used_vars[op->name] = true;
                    // if time distance is zero, it refers to the latest value in temporary registers
                    if (recalculate_func_referrence_args(op->name, args)) {
                        string temp_name = op->name + "_temp";
                        vector<Expr> temp_args(args.begin(), args.begin() + num_new_space_vars);
                        return Call::make(op->type, temp_name, temp_args, op->call_type);
                    }
                    return Call::make(op->type, op->name, args, op->call_type,
                                      op->func, op->value_index, op->image, op->param);
                }
            }
            if (op->is_intrinsic(Call::IntrinsicOp::likely))
                return op->args[0];
        }
        return IRMutator::visit(op);
    }

    Stmt visit(const Provide* op) override {
        if (need_rewrite) {
            vector<Expr> values(op->values.size());
            vector<Expr> args = op->args;
            // check if the loop vars are reordered
            // if so, find the mapping
            if (loop_var_pos.size() > 0 && loop_var_pos[0] == -1) {
                for (size_t i = 0; i < args.size(); i++) {
                    for (size_t j = 0; j < num_args; j++) {
                        const Variable* old_arg = args[i].as<Variable>();
                        const Variable* new_arg = loop_vars[j].as<Variable>();
                        internal_assert(old_arg);
                        internal_assert(new_arg);
                        if (old_arg->name == new_arg->name) {
                            loop_var_pos[j] = i;
                            break;
                        }
                    }
                }
            }
            // check if the provide node is the final realize
            Function func;
            if (function_is_in_environment(op->name, env, func)
            && (func.definition().schedule().is_merged() || func.has_merged_defs())
            && !func.definition().schedule().is_output()
            && !func.definition().schedule().is_input()) {
                used_vars[op->name] = true;
                for (size_t i = 0; i < op->values.size(); i++) {
                    values[i] = mutate(op->values[i]);
                }
                // save the value into temporary registers
                internal_assert(recalculate_func_referrence_args(op->name, args));
                string temp_name = op->name + "_temp";
                vector<Expr> temp_args(args.begin(), args.begin() + num_new_space_vars);
                return Provide::make(temp_name, std::move(values), std::move(temp_args));
            }
        }
        return IRMutator::visit(op);
    }

};

// We detect systolic pattern like this:
// X(a) = select(a == 0, input, A(a-1))
class PreRewriter : public IRMutator {
  public:
    PreRewriter(const std::map<std::string, Function>& _env)
        : env(_env) {}

  private:
    using IRMutator::visit;
    const std::map<std::string, Function> &env;

    Stmt visit(const Provide *op) override {
        Expr value = remove_lets(op->values[0]);
        auto f_val = value.as<Select>();
        if (!f_val) {
            return IRMutator::visit(op);
        }
        // Check condition
        auto cond = f_val->condition.as<EQ>();
        if (!cond) {
            return IRMutator::visit(op);
        }
        auto a = cond->a.as<Variable>();
        if (!a || !is_zero(cond->b)) {
            return IRMutator::visit(op);
        }
        // Check true value
        auto true_val = f_val->true_value.as<Call>();
        if (!true_val) {
            return IRMutator::visit(op);
        }
        // Check false value
        auto false_val = f_val->false_value.as<Call>();
        if (!false_val || false_val->name != op->name) {
            return IRMutator::visit(op);
        }
        auto call_args = false_val->args;
        internal_assert(call_args.size() == op->args.size());

        for (size_t i = 0; i < call_args.size(); i++) {
            Expr diff = simplify(op->args[i] - call_args[i]);
            auto var = op->args[i].as<Variable>();
            internal_assert(var);
            if (!is_zero(diff) && var->name != a->name) {
                return IRMutator::visit(op);
            }
        }
        return Provide::make(op->name, {f_val->true_value}, op->args);
    }
};

// Move realize nodes inner the GPU thread loops
// Otherwise the code generator cannot find them
class PostRewriter : public IRMutator {
  public:
    PostRewriter(const std::map<std::string, Function>& _env)
        : env(_env) {}

  private:
    using IRMutator::visit;

    const std::map<std::string, Function> &env;
    vector<const Realize*> realize_nodes;
    std::string last_gpu_loop;

    Stmt visit(const Realize *op) override {
        realize_nodes.push_back(op);
        return mutate(op->body);
    }

    Stmt visit(const ProducerConsumer *op) override {
        return mutate(op->body);
    }

    Stmt visit(const For *op) override {
        if (op->for_type == ForType::GPUThread) {
            last_gpu_loop = op->name;
        }

        Stmt body = mutate(op->body);
        if (op->name == last_gpu_loop) {
            for (auto &r : realize_nodes) {
                // body = ProducerConsumer::make(r->name, true, body);
                body = Realize::make(r->name, r->types, r->memory_type, r->bounds, r->condition, body);
            }
        }
        return For::make(op->name, op->min, op->extent, op->for_type, op->device_api, body);
    }
};

} // namespace

Stmt apply_space_time_transform(Stmt s,
                                std::map<std::string, Function> &env,
                                const Target &target,
                                std::map<std::string, RegBound> &reg_size_map) {
    // Simplify the incoming loop first
    SelectToIfConverter converter;
    debug(4) << converter.mutate(s);
    s = no_if_simplify(converter.mutate(s), true);

    if (target.has_feature(Target::IntelGPU)) {
        PreRewriter rewriter(env);
        s = rewriter.mutate(s);
        debug(4) << "Convert systolic into broadcast on GPUs:\n"
                 << s << "\n";
    }
    // Apply space time transformation
    SpaceTimeTransformer mutator(env, target);
    s = mutator.mutate(s);

    if (target.has_feature(Target::IntelGPU)) {
        PostRewriter rewriter(env);
        s = rewriter.mutate(s);
        // Export original loops to memory-related schedules,
        // since transformed loops may confuse the bound inference on GPUs
        Adaptor loops = {mutator.loop_vars, mutator.loop_mins, mutator.loop_extents};
        s = do_prepare_memory_schedule(s, env, loops);
    }

    // So far, the shift registers are still referenced as common memory like
    //    C(s.kkk, s.jj, s.ii, (15 - t), 0, 0, 0, 0, 0)
    // If we access them in write/read_shift_reg intrincs, we will confuse the bounds inference.
    // So keep the current form, but pass the shift register information to a later phase.
    reg_size_map = mutator.reg_size_map;
    return s;
}

} // namespace Internal
} // namespace Halide
