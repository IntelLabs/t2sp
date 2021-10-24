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
#include "IRMutator.h"
#include "IRVisitor.h"
#include "Simplify.h"
#include <list>
#include <vector>
#include "./DebugPrint.h"
#include "./PreprocessBeforeLower.h"
#include "./Stensor.h"

namespace Halide {
namespace Internal {

using std::vector;

class SttChecker : public IRVisitor {
  public:
    std::set<std::string> extend_dims;
    SttChecker(const std::vector<Var> &a, const std::vector<string> &s)
        : args(a), src_vars(s) {}
  private:
    using IRVisitor::visit;
    const std::vector<Var> args;
    const std::vector<string> src_vars;

    void visit(const Call* op) override {
        if (op->call_type == Call::Halide) {
            // Check all UREs' dependences and record their dimension indexes
            Function func(op->func);
            if ((func.definition().schedule().is_merged() || func.has_merged_defs())
                && !func.definition().schedule().is_output()
                && !func.definition().schedule().is_input()) {
                vector<Expr> call_args = op->args;
                internal_assert(call_args.size() == args.size());

                for (size_t i = 0; i < args.size(); i++) {
                    string dim_name = args[i].name();
                    auto diff = simplify(args[i] - call_args[i]);
                    auto src_it = std::find(src_vars.begin(), src_vars.end(), dim_name);
                    if (!is_zero(diff) && src_it == src_vars.end()) {
                        extend_dims.insert(dim_name);
                    }
                }
            }
        }
        IRVisitor::visit(op);
    }
};


void check_space_time_transform(Func &func, Target target) {
    if (func.function().has_merged_defs()
        && func.function().definition().schedule().has_stt()
        && !func.function().definition().schedule().is_output()
        && !func.function().definition().schedule().is_input()) {
        auto& param = func.function().definition().schedule().transform_params()[0];
        auto& src_vars = param.src_vars;
        SttChecker chker(func.args(), src_vars);

        vector<Func> ures = func.function().definition().schedule().merged_ures();
        ures.push_back(func);
        for (auto &f : ures) {
            Tuple values = f.values();
            for (size_t i = 0; i < values.size(); i++) {
                values[i].accept(&chker);
            }
        }
        auto& extend_dims = chker.extend_dims;

        if (extend_dims.size() > 0) {
            debug(4) << "STT does not cover all the dependencies, "
                        "try to flatten the time loop...\n";
            Expr loop_bound = 0;
            auto func_dims = func.function().definition().schedule().dims();

            // the original time loop bound
            for (size_t i = 0; i < param.sch_vector.size(); i++) {
                string var = src_vars[i];
                auto min_expr = func.function().get_bounds(var).first;
                auto max_expr = func.function().get_bounds(var).second;
                auto diff = simplify(max_expr - min_expr - 1);
                loop_bound += param.sch_vector[i] * diff;
            }
            loop_bound = simplify(loop_bound + 1);

            for (size_t i = 0; i < func_dims.size()-1; i++) {
                string cur_dim = func_dims[i].var;
                auto src_it = std::find(src_vars.begin(), src_vars.end(), cur_dim);
                // We find a dependence not covered by space-time transform
                if (src_it == src_vars.end()) {
                    if (extend_dims.find(cur_dim) != extend_dims.end()) {
                        src_vars.push_back(cur_dim);
                        for (size_t i = 0; i < param.proj_matrix.size(); i++) {
                            param.proj_matrix[i].push_back(0);
                        }
                        user_assert(is_const(loop_bound))
                                << "We cannot flatten the time loop with dynamic bound."
                                << "Please provide a schedule vector covering all dependencies\n";
                        param.sch_vector.push_back(loop_bound.as<IntImm>()->value);
                    }
                    auto min_expr = func.function().get_bounds(cur_dim).first;
                    auto max_expr = func.function().get_bounds(cur_dim).second;
                    loop_bound = simplify(loop_bound * (max_expr - min_expr));
                }
            }
        }
    }
}

void convert_removed_loops_to_unit_loops(Func &func) {
    const auto &remove_params = func.function().definition().schedule().remove_params();
    if (remove_params.empty()) {
        return;
    }

    debug(4) << "Remove loops " << to_string<string>(remove_params) << " from Func " << func.name() << "\n";
    for (auto &r: remove_params) {
        auto original_bounds = func.function().get_bounds(r);
        if (original_bounds.first.defined()) {
            func.function().set_bounds({r}, {original_bounds.first}, {Expr(1)});
        } else {
            func.function().set_bounds({r}, {Expr(0)}, {Expr(1)});
        }
    }
}

void reorder_gpu_loops(Func func) {
    if (func.function().has_merged_defs()
        && func.function().definition().schedule().has_stt()
        && !func.function().definition().schedule().is_output()
        && !func.function().definition().schedule().is_input()) {
        // A kernel starts from GPU loops, so the outer loops cannot enter into codegen.
        // We move GPU loops as the outermost levels to be portable acorss targets.
        auto func_dims = func.function().definition().schedule().dims();
        vector<VarOrRVar> loops, gpu_threads, gpu_blocks;
        for (auto &d : func_dims) {
            if (d.for_type == ForType::GPUThread) {
                gpu_threads.push_back(Var(d.var));
            } else if (d.for_type == ForType::GPUBlock) {
                gpu_blocks.push_back(Var(d.var));
            } else {
                loops.push_back(Var(d.var));
            }
        }
        internal_assert(loops.back().name() == "__outermost");
        loops.insert(loops.end()-1, gpu_threads.begin(), gpu_threads.end());
        loops.insert(loops.end()-1, gpu_blocks.begin(), gpu_blocks.end());
        func.reorder(loops);
    }
}

void annotate_loop_type(Func func) {
    if (func.function().definition().schedule().has_stt()) {
        // We annotate space loops as unrolled when invoking unscheduled stt, since
        // the target platform is unknown at that time. But the innermost space loop
        // should be vectorized while others should be serial on GPUs (loops involved
        // in memory address can be automatically unrolled in a later stage).
        auto& param = func.function().definition().schedule().transform_params()[0];
        if (!param.sch_vector_specified) {
            auto& src_vars = param.src_vars;
            func.vectorize(Var(src_vars[0]));
            for (size_t i = 1; i < src_vars.size(); i++) {
                func.serial(Var(src_vars[i]));
            }
        }
    }
}


void t2s_preprocess_before_lower(map<string, Func> &env, const Target &target) {
    debug(4) << "Preprocessing functions in the environment:\n";
    for (auto &e : env) {
        auto &func = e.second;

        // In the function, for loops marked as removed, make their extents as 1. For the
        // calls of the function, later we will fix their args corresponding to the loops.
        convert_removed_loops_to_unit_loops(func);

        // GPU-related transform
        if (target.has_feature(Target::IntelGPU)) {
            reorder_gpu_loops(func);
            annotate_loop_type(func);
        }
        // Space-time transform
        func.apply_same_loop_transform_to_merged_ures();
        check_space_time_transform(func, target);
    }
}

}
}
