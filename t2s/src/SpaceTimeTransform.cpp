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

#include "../../Halide/src/Func.h"
#include "../../Halide/src/Function.h"
#include "../../Halide/src/IR.h"
#include "../../Halide/src/Simplify_Internal.h"
#include "Simplify.h"
#include "Substitute.h"
#include "IRMutator.h"
#include "./DebugPrint.h"
#include "./NoIfSimplify.h"
#include "./SpaceTimeTransform.h"
#include "Math.h"
#include "./Utilities.h"

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
    user_assert(vars[0].name() == func_dims[0].var) << "Space loop " << vars[0]
            << " is expected to be at the inner-most level.\n";
    for (size_t i = 0; i < vars.size(); i++) {
        user_assert(vars[i].name() == func_dims[i].var)
            << "The " << i << "'th space loop is expected to be loop " << func_dims[i].var
            << ", not loop " << vars[i].name()
            << ". Make sure the space loops are specified in the same order as the Func arguments.\n";
    }
    // Check if the number of coefficients is the same as the number of variables
    user_assert(vars.size() == coefficients.size()) << "Number of space variables (" << vars.size()
        << ") should be the same as number of coefficients (" << coefficients.size() << ").";

    // Add the time loop variable
    size_t src_size = vars.size() + 1;
    Var t(func_dims[vars.size()].var);
    vector<Var> src_vars(vars);
    src_vars.push_back(t);
    debug(3) << "add source var: " << t << "\n";

    // Add the dst_vars
    // Since it projects to n-1 dimension, we get n-1 space loops plus 1 time loop
    size_t dst_size = vars.size() + 1;
    vector<Var> dst_vars(dst_size);
    for (size_t i = 0; i < dst_size-1; i++)
        dst_vars[i] = Var(vars[i].name());
    string t_name = (sch_vector_specified ? "t." : "") + func_dims[dst_size-1].var;
    // string t_name = func.definition().schedule().dims()[dst_size-1].var + ".t";
    dst_vars[dst_size-1] = Var(t_name);
    // dst_vars[dst_size-1] = Var("dst");

    // Generate allocation matrix
    Matrix::matrix_t alloc(dst_size, vector<int>(src_size));
    for (size_t i = 0; i < dst_size-1; i++)
        for (size_t j = 0; j < src_size; j++)
            alloc[i][j] = (i == j ? 1 : 0);
    alloc[dst_size-1] = coefficients;
    alloc[dst_size-1].push_back(1);

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
                                 const vector<vector<int>> &coefficients,
                                 SpaceTimeTransform check) {
    return space_time_transform(src_vars, dst_vars, coefficients, {}, check);
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

    for (size_t i = 0; i < dst_size; i++)
        new_coefficients.push_back(vector<int>(coefficients.begin()+i*src_size,
                                   coefficients.begin()+(i+1)*src_size));
    for (size_t i = 0; i < src_size; i++)
        new_reverse.push_back(std::make_pair(reverse[i*2], reverse[i*2+1]));

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
    std::map<std::string, Expr> var_map;

    // Check if the number of vars exceeds the number of loops variables
    user_assert(src_size <= this->args().size())
        << "Number of space variables (" << src_size
        << ") exceeds the number of loop variables (" << this->args().size()<< ").";

    // Check if the vars are given starting from the inner-most loops
    for (size_t i = 0; i < src_size; i++) {
        user_assert(src_vars[i].name() ==
                    func.definition().schedule().dims()[i].var)
            << "The " << i << "'th source loop (" << src_vars[i].name()
            << ") does not match the " << i << "'th innermost loop ("
            << func.definition().schedule().dims()[i].var << "). "
            << "A space-time transform expects a contiguous set of source loops to be specified, "
            << "starting from the innermost level.";
        std::string name = func.name() + ".s0." + src_vars[i].name();
        auto var = Variable::make(Int(32), name);
        var_map.insert({src_vars[i].name(), var});
    }

    user_assert(dst_size <= src_size);
    // Add prefix of variable name
    vector<std::string> dst_names(dst_size);
    for (size_t i = 0; i < dst_size; i++) {
        std::string name = func.name() + ".s0." + dst_vars[i].name();
        auto var = Variable::make(Int(32), name);
        var_map.insert({dst_vars[i].name(), var});
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
            string name = var_map[src_vars[i].name()].as<Variable>()->name;
            def = simplify(substitute(var_map, def));
            new_reverse.insert({ name, def });
            debug(3) << "Var " << name
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
                if (src_var && v.name() == src_var->name) {
                    is_found = true;
                }
            }
            user_assert(is_found)
                << "The reverse definitions should have the following format:\n"
                << "{ {source varible 1, definition},\n"
                   "  {source varible 2, definition}, ...}\n";

            // Check if the variable used in the expr has been defined previously
            reverse[i].second.accept(&rev_chk);
            rev_chk.add_visited_var(src_var->name);

            string name = var_map[src_var->name].as<Variable>()->name;
            Expr def = substitute(var_map, reverse[i].second);
            def = simplify(substitute(new_reverse, def));
            new_reverse.insert({ name, def });
            debug(3) << "Var " << name
                     << " = "  << def << "\n";
        }
    }

    SpaceTimeTransformParams params;
    params.sch_vector = sch_vector;
    params.check_time = check;
    params.proj_matrix = proj_matrix;
    params.dst_vars = dst_names;
    params.reverse = new_reverse;
    params.sch_vector_specified = true;
    params.num_src_vars_specified = src_vars.size();
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
    SpaceTimeTransformer(const std::map<std::string, Function>& _env)
        : env(_env), target(get_host_target()) {}

    SpaceTimeTransformer(const std::map<std::string, Function>& _env,
                         const Target& _t)
        : env(_env), target(_t) {}

    std::map<std::string, RegBound > reg_size_map;
    std::map<std::string, std::vector<Expr>> reg_annotation_map;

  private:
    const std::map<std::string, Function> &env;
    const Target &target;
    // Space-time transformation related params
    bool in_scheduled_stt;
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
    size_t for_level;
    vector<Expr> loop_vars;
    vector<Expr> loop_mins;
    vector<Expr> loop_extents;
    std::map<std::string, Expr> global_min;
    std::map<std::string, Expr> global_max;
    size_t new_for_level;
    vector<Expr> new_loop_vars;
    vector<Expr> new_loop_mins;
    vector<Expr> new_loop_extents; // loop extents after transformation
    vector<int> loop_var_pos;
    // A list of variables that will be realized
    vector<std::string> realize_names;
    struct RealizeParams {
        vector<Type> type;
        MemoryType memory_type;
        Expr condition;
    };
    std::map<std::string, RealizeParams > realize_map;
    // std::map<std::string, std::pair<vector<Type>, MemoryType> > realize_map;
    // A list of variables that are used
    std::map<std::string, bool> used_vars;
    // Check whether in isolated kernels
    bool in_input_or_output{true};

    // Helper functions
    void recalculate_func_referrence_args(std::string func_name, vector<Expr>& args) {
        // special case when every loops are space loops
        // if (num_space_vars == num_args) return;
        // reorder the args if necessary
        vector<Expr> new_args = args;
        for (size_t i = 0; i < num_args; i++) {
            new_args[i] = args[loop_var_pos[i]];
        }
        // first, handle the time loops that are not transformed
        SpaceTimeTransformParams param = param_vector[0];
        size_t size = param.sch_vector.size();
        args.resize(num_args, 0);
        debug(3) << func_name << "(";

        // modify the index for space loops
        const size_t t = num_new_space_vars;
        for (size_t i = 0; i < t; i++) {
            Expr space_expr = IntImm::make(Int(32), 0);
            for (size_t j = 0; j < size; j++)
                space_expr += (loop_vars[j] - new_args[j]) * param.proj_matrix[i][j];
            space_expr = new_loop_vars[i] - space_expr;
            args[i] = simplify(space_expr);
            debug(3) << args[i] << ", ";
        }

        // modify the index for time loops
        Expr time_expr = IntImm::make(Int(32), 0);
        for (size_t j = 0; j < size; j++)
            time_expr += (loop_vars[j] - new_args[j]) * param.sch_vector[j];
        args[t] = simplify(time_expr);
        debug(3) << args[t] << ", ";

        for (size_t i = num_new_space_vars + 1; i < num_args; i++) {
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
    }

    // Helper Classes
    class LoopInfoCollector : public IRVisitor {
      public:
        LoopInfoCollector(size_t for_level, vector<Expr>& loop_vars, vector<Expr>& loop_mins,
                vector<Expr>& loop_extents, std::map<std::string, Expr>& global_min,
                std::map<std::string, Expr>& global_max) : for_level(for_level),
                loop_vars(loop_vars), loop_mins(loop_mins), loop_extents(loop_extents),
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
            global_max[op->name] = simplify(extent);
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
        realize_names.push_back(op->name);
        realize_map[op->name] = { op->types, op->memory_type, op->condition };
        Stmt body = mutate(op->body);
        Region bounds = op->bounds;

        if (reg_size_map.find(op->name) != reg_size_map.end()) {
            if (target.has_feature(Target::IntelGPU)) {
#if 0
                // Since stt rewrites buffer bounds which may cause bound infererence failed,
                // we should save the original bounds via LetStmt
                const Function& func = env.at(op->name);
                internal_assert(func.args().size() == bounds.size());
                for (size_t i = 0; i < bounds.size(); ++i) {
                    string name = op->name + ".s0." + func.args().at(i) + ".def";
                    auto b = func.get_bounds(func.args().at(i));
                    body = LetStmt::make(name + ".max", b.second - 1, body);
                    body = LetStmt::make(name + ".min", b.first, body);
                }
#endif
                return body;
            }
            bounds.clear();
            auto &mins = reg_size_map[op->name].mins;
            auto &maxs = reg_size_map[op->name].maxs;
            for (size_t i = 0; i < mins.size(); i++) {
                bounds.emplace_back(simplify(mins[i]),
                                    simplify(maxs[i]+1));
            }
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
        in_scheduled_stt = false;
        if (op->is_producer // we only care about producer
            && function_is_in_environment(op->name, env, func)
            && func.definition().schedule().transform_params().size() > 0) {
            // collect the information for later processing
            param_vector = func.definition().schedule().transform_params();
            in_scheduled_stt = param_vector[0].sch_vector_specified;
            // calculate the number of space loops and new time loops
            num_args = func.args().size();
            num_time_vars = 1;
            num_space_vars = param_vector[0].sch_vector.size() - 1;
            num_other_vars = num_args - num_space_vars - 1;
            num_new_space_vars = param_vector[0].dst_vars.size() - 1;
            num_new_args = param_vector[0].dst_vars.size() + num_other_vars;
            internal_assert(num_space_vars > 0);
            // initialize useful variables
            for_level = num_args;
            new_for_level = num_new_args;
            vectorized_loop_name = "";
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
            if (!in_scheduled_stt) {
                for (size_t i = 0; i < num_space_vars; i++) {
                    if (!is_const(loop_mins[i]) || !is_const(loop_extents[i])) {
                        space_is_dynamic = true;
                        break;
                    }
                }
            }
            in_input_or_output = func.definition().schedule().is_output() || func.definition().schedule().is_input();
            for (size_t i = 0; i < realize_names.size(); i++) {
                Function merged_func;
                const string merged_func_name = realize_names[i];
                if (function_is_in_environment(merged_func_name, env, merged_func)
                    && (merged_func.definition().schedule().is_merged() || merged_func.has_merged_defs())
                    && !merged_func.definition().schedule().is_output()
                    && !merged_func.definition().schedule().is_input()
                    && reg_size_map.find(merged_func_name) == reg_size_map.end()) {
                    if (in_scheduled_stt) {
                        vector<Expr> reg_size;
                        reg_size.resize(num_args, 0);
                        reg_size_map[merged_func_name].mins = reg_size;
                        reg_size_map[merged_func_name].maxs = reg_size;
                    } else {
                        // Let the register bounds the same as the corresponding loop bounds (In unscheduled stt,
                        // at this moment, we do not change loops at all). We will minimize the bounds in a later phase.
                        for (size_t j = 0; j < num_args; j++) {
                            reg_size_map[merged_func_name].mins.push_back(loop_mins[j]);
                            reg_size_map[merged_func_name].maxs.push_back(simplify(loop_extents[j] - 1));
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
            used_vars.clear();
            // visit the body
            Stmt body = mutate(op->body);
            if (!target.has_feature(Target::IntelGPU))
                return ProducerConsumer::make(op->name, op->is_producer, body);
            else
                return body;
        }
        return IRMutator::visit(op);
    }

    string get_func_name(const string &op_name, size_t *p_pos = 0) {
        auto pos = op_name.find('.');
        internal_assert(pos != op_name.npos)
            << "Cannot find function name";
        if (p_pos)
            *p_pos = pos;
        return op_name.substr(0, pos);
    }

    Stmt process_time_loop(const For *op) {
        Stmt body;
        Expr min = 0;
        Expr extent = 0;
        SpaceTimeTransformParams param = param_vector[0];
        // we are in the time loop
        // 1. Create new time loop with the given variable
        string name = op->name.substr(0, op->name.find('.'))
               + ".s0." + param.dst_vars[num_new_space_vars];
        new_loop_vars[num_new_space_vars] = Variable::make(Int(32), name);
        // 2. Calculate the new bound
        size_t size = param.sch_vector.size();
        for (size_t i = 0; i < size; i++) {
            int coeff = param.sch_vector[i];
            if (coeff < 0) {
                min = min + coeff * (loop_extents[i]-1);
                extent = extent + coeff * loop_mins[i];
            } else {
                // TODO(hxc): reversed?
                min = min + coeff * loop_mins[i];
                extent = extent + coeff * (loop_extents[i]-1);
            }
        }
        internal_assert(!is_zero(min) && !is_zero(extent));
        min = simplify(min);
        extent = simplify(extent + 1);
        new_loop_mins[num_new_space_vars] = min;
        new_loop_extents[num_new_space_vars] = extent;
        debug(3) << "loop: " << name << "("
                             << min << ".."
                             << extent << ")\n";
        if (op->for_type == ForType::Vectorized) {
            internal_assert(vectorized_loop_name == "") << "Cannot vectorize more than one loops: "
                                                        << vectorized_loop_name << " " << op->name << "\n";
            vectorized_loop_name = op->name;
            debug(3) << "loop " << op->name << " is vectorized. \n";
        }
        // 3. Visit the body
        body = mutate(op->body);
        // for the time loop, we need to add shift register logics
        // we would generate logic like this
        // unrolled for (i, 0, N-1) // extend is N-1
        //   sr[N-i] = sr[N-i-1]
        if (num_space_vars < num_args
                && reg_size_map.size() > 0) {
            Stmt sr_body = Stmt();
            for (auto kv : reg_size_map) {
                std::string name = kv.first;
                // check if the variable is used within the body
                if (used_vars.find(name) == used_vars.end()) continue;
                // the time dimension
                Expr min = kv.second.mins[num_new_space_vars];
                Expr size = kv.second.maxs[num_new_space_vars];
                if (is_zero(size-min)) continue;
                // create a new loop var
                // Note: below we give the new loop var's func prefix as some fake func
                // name, instead of the current func's name, because this loop is not
                // from the current func. Otherwise, the bound inference later will break
                // as it will use this loop var to infer the bounds of the current func.
                std::string sr_name = unique_name("DUMMY") + ".s0.t";
                Expr sr_var = Variable::make(Int(32), sr_name);
                // build the for loop body
                vector<Expr> provide_args;
                vector<Expr> call_args;
                for (size_t i = 0; i < num_new_space_vars; i++) {
                    debug(3) << "new_loop_vars " << new_loop_vars[i] << "\n";
                    provide_args.push_back(new_loop_vars[i]);
                    call_args.push_back(new_loop_vars[i]);
                }
                provide_args.push_back(size-min-sr_var);
                call_args.push_back(simplify(size-min-1-sr_var));
                for (size_t i = num_new_space_vars+1; i < num_args; i++) {
                    provide_args.push_back(0);
                    call_args.push_back(0);
                }
                vector<Expr> calls;
                for(size_t k = 0; k < realize_map[name].type.size(); k++){
                    Type t = (realize_map[name].type)[k];
                    Expr call = Call::make(t, name, call_args, Call::CallType::Halide,
                                           FunctionPtr(), k, Buffer<>(), Parameter());
                    calls.push_back(call);
                }

                Stmt provide = Provide::make(name, calls, provide_args);
                // build the for loop
                Stmt loop_body = For::make(sr_name, min, size,
                                           ForType::Unrolled, DeviceAPI::None, provide);
                if (sr_body.defined()) {
                    sr_body = Block::make(loop_body, sr_body);
                } else {
                    sr_body = loop_body;
                }
            }
            // add the space loop
            if (sr_body.defined()) {
                for (size_t i = 0; i < num_new_space_vars; i++) {
                    std::string sp_name = new_loop_vars[i].as<Variable>()->name;
                    vector<std::string> names = split_string(sp_name, ".");
                    // Note: below we give the new loop var's func prefix as some fake func
                    // name, instead of the current func's name, because this loop is not
                    // from the current func.
                    std::string var_name = unique_name("DUMMY") + ".s0.s." + names[names.size()-1];
                    sr_body = substitute(sp_name, Variable::make(Int(32), var_name), sr_body);
                    // TODO: For GPU, maybe this can be vectorized for better efficiency
                    ForType ftype = ForType::Unrolled;
                    sr_body = For::make(var_name, new_loop_mins[i], new_loop_extents[i],
                                        ftype, DeviceAPI::None, sr_body);
                }
                // merge with the main body
                body = Block::make(sr_body, body);
            }
        }
        for_level += 1;
        return For::make(name, min, extent, ForType::Serial, op->device_api, body);
    }

    Stmt process_space_loop(const For *op) {
        Stmt body;
        Expr min = 0;
        Expr extent = 0;
        SpaceTimeTransformParams param = param_vector[0];
        // we are in the space loops
        // we must ensure that all the original loops are perfect loops
        if (for_level == num_space_vars - 1) {
            // the outermost space loop
            for (size_t k = 0; k < num_new_space_vars; k++) {
                size_t size = param.sch_vector.size();
                // 1. Create new space loop with the given variable
                string name = op->name.substr(0, op->name.find('.'))
                        + ".s0." + param.dst_vars[k];
                new_loop_vars[k] = Variable::make(Int(32), name);
                // 2. Calculate the new bound
                min = 0;
                extent = 0;
                for (size_t i = 0; i < size; i++) {
                    int coeff = param.proj_matrix[k][i];
                    if (coeff < 0) {
                        min = min + coeff * (loop_extents[i]-1);
                        extent = extent + coeff * loop_mins[i];
                    } else {
                        min = min + coeff * loop_mins[i];
                        extent = extent + coeff * (loop_extents[i]-1);
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
                        if (!is_const(min) || !is_const(extent))
                            reg_annotation_map[kv.first] = {};
                    }
                }
            }
            // If the register size is not a const(caused by non-constant space loop extent),
            // we need to add an annotation before realizing registers
            // and handle this case differently in CodeGen.
            for (auto &kv : reg_annotation_map) {
                if (kv.second.empty()) {
                    std::vector<Expr> reg_vars(new_loop_vars.begin(), new_loop_vars.begin() + num_space_vars);
                    kv.second = reg_vars;
                }

            }
        }
        if (op->for_type == ForType::Vectorized) {
            internal_assert(vectorized_loop_name == "") << "Cannot vectorize more than one loops: "
                                                        << vectorized_loop_name << " " << op->name << "\n";
            vectorized_loop_name = op->name;
            debug(3) << "loop " << op->name << " is vectorized. \n";
        }
        body = mutate(op->body);

        if (for_level == 0) {
            if (param.check_time == SpaceTimeTransform::CheckTime) {
                for (size_t k = 0; k <= num_space_vars; k++) {
                    Expr condition = (loop_vars[k] >= loop_mins[k])
                                    && (loop_vars[k] < loop_mins[k] + loop_extents[k]);
                    // condition = substitute(param_vector[0].reverse, condition);
                    body = IfThenElse::make(simplify(condition), body);
                }
            }
            map<string, Expr> var_map;
            string ori = get_func_name(param.reverse.begin()->first);
            string cur = get_func_name(op->name);
            for (size_t i = 0; i < param.dst_vars.size(); i++) {
                string ori_name = ori + ".s0." + param.dst_vars[i];
                string cur_name = cur + ".s0." + param.dst_vars[i];
                var_map.insert({ ori_name, Variable::make(Int(32), cur_name) });
            }
            for (auto &p : param.reverse) {
                size_t pos;
                get_func_name(p.first, &pos);
                string name = cur + p.first.substr(pos);
                Expr def = substitute(var_map, p.second);
                body = LetStmt::make(name, def, body);
            }
            for (size_t k = 0; k < num_new_space_vars; k++) {
                // the inner most loop, we emit all the space loops
                string name = new_loop_vars[k].as<Variable>()->name;
                // Need to set the space loop as unrolled or vectorized: for IntelGPU and FPGA,
                // set the loop as Vectorized if the loop is at innermost level after stt and is
                // marked as Vectorized originally; for all the other cases, set the
                // loop as Unrolled.
                ForType for_type = ForType::Unrolled;
                if (target.has_feature(Target::IntelGPU) || target.has_feature(Target::IntelFPGA)) {
                    if (k == 0 && vectorized_loop_name != "") {
                        std::string new_vectorized_loop_name = vectorized_loop_name.substr(0, vectorized_loop_name.find('.')) + ".s0." + param.dst_vars[k];
                        internal_assert(new_vectorized_loop_name == name) << "After space time transformation, the vectorized loop "
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
                }
                body = For::make(name, new_loop_mins[k], new_loop_extents[k],
                                 for_type, op->device_api, body);
            }
        }
        for_level += 1;
        debug(3) << "For body: " << body;
        return body;
    }

    Stmt visit(const For* op) override {
        if (in_scheduled_stt) {
            // handle loop related information
            for_level -= 1;
            // rebuild the loop if necessary
            if (for_level < num_time_vars + num_space_vars && for_level >= num_space_vars) {
                return process_time_loop(op);
            }
            if (for_level < num_space_vars) {
                return process_space_loop(op);
            }

            Stmt body = mutate(op->body);
            if (target.has_feature(Target::IntelGPU) && op->for_type == ForType::GPUThread) {
                for (auto& kv : realize_map) {
                    if (reg_size_map.find(kv.first) != reg_size_map.end()) {
                        Region bounds;
                        const auto &reg_bound = reg_size_map[kv.first];
                        for (size_t i = 0; i < reg_bound.mins.size(); i++)
                            bounds.emplace_back(simplify(reg_bound.mins[i]),
                                                simplify(reg_bound.maxs[i]+1));
                        body = ProducerConsumer::make(kv.first, true, body);
                        body = Realize::make(kv.first, kv.second.type, MemoryType::Register,
                                                bounds, kv.second.condition, body);
                    }
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

        if (in_scheduled_stt) {
            if (op->call_type == Call::CallType::Halide) {
                if (function_is_in_environment(op->name, env, func)
                && (func.definition().schedule().is_merged() || func.has_merged_defs())
                && !func.definition().schedule().is_output()
                && !func.definition().schedule().is_input()) {
                    used_vars[op->name] = true;
                    // recalculate the index
                    recalculate_func_referrence_args(op->name, args);
                    debug(3) << op->name << "(";
                    for (auto &arg : args)
                        debug(3) << arg << ", ";
                    debug(3) << ")\n";
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
        if (in_scheduled_stt) {
            vector<Expr> values(op->values.size());
            vector<Expr> args = op->args;
            // check if the loop vars are reordered
            // if so, find the mapping
            if (loop_var_pos.size() > 0 && loop_var_pos[0] == -1) {
                for (size_t i = 0; i < args.size(); i++) {
                    for (size_t j = 0; j < num_args; j++) {
                        const Variable* old_arg = args[i].as<Variable>();
                        const Variable* new_arg = loop_vars[j].as<Variable>();
                        if (old_arg->name == new_arg->name) {
                            loop_var_pos[j] = i;
                            break;
                        }
                    }
                }
            }
            // mutate the values
            for (size_t i = 0; i < op->values.size(); i++) {
                values[i] = mutate(op->values[i]);
            }
            // check if the provide node is the final realize
            Function func;
            if (function_is_in_environment(op->name, env, func)
            && (func.definition().schedule().is_merged() || func.has_merged_defs())
            && !func.definition().schedule().is_output()
            && !func.definition().schedule().is_input()) {
                used_vars[op->name] = true;
                // we need to recalculate the index
                recalculate_func_referrence_args(op->name, args);
                return Provide::make(op->name, values, args);
            }
            // the final realize -> no need to change the args
            return Provide::make(op->name, values, op->args);
        }
        return IRMutator::visit(op);
    }

};

} // namespace

Stmt apply_space_time_transform(Stmt s,
                                const std::map<std::string, Function> &env,
                                const Target &target,
                                std::map<std::string, RegBound > &reg_size_map) {
    // Simplify the incoming loop first
    SelectToIfConverter converter;
    debug(4) << converter.mutate(s);
    s = no_if_simplify(converter.mutate(s), true);

    // Apply space time transformation
    SpaceTimeTransformer mutator(env, target);
    s = mutator.mutate(s);

    // So far, the shift registers are still referenced as common memory like
    //    C(s.kkk, s.jj, s.ii, (15 - t), 0, 0, 0, 0, 0)
    // If we access them in write/read_shift_reg intrincs, we will confuse the bounds inference.
    // So keep the current form, but pass the shift register information to a later phase.
    reg_size_map = mutator.reg_size_map;
    return s;
}

} // namespace Internal
} // namespace Halide
