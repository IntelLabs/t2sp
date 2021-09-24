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
#include "Stensor.h"

namespace Halide {
namespace Internal {

using std::vector;

class SttChecker : public IRVisitor {
  public:
    SttChecker(const std::vector<Var> &a)
        : args(a.begin(), a.end()),
          num_stt_args(0) {}
    
    size_t get_num_stt_args(void) const {
        return num_stt_args;
    }

  private:
    using IRVisitor::visit;
    std::vector<Var> args;
    // the outermost dimension having dependence
    // the stt spec should cover this dimension
    size_t num_stt_args;

    void visit(const Call* op) override {
        if (op->call_type != Call::Halide) {
            IRVisitor::visit(op);
            return;
        }

        Function func(op->func);
        if (func.definition().schedule().is_merged() ||
            func.has_merged_defs()) {
            vector<Expr> call_args = op->args;
            size_t off_args = args.size() - call_args.size();
            internal_assert(off_args >= 0);
            for (size_t i = off_args; i < args.size(); i++) {
                debug(3) << args[i] - call_args[i - off_args] << ", ";
                if (!is_zero(simplify(args[i] - call_args[i - off_args])))
                    if (num_stt_args < i + 1)
                        num_stt_args = i + 1;
            }
            debug(3) << "\n";
        }
        IRVisitor::visit(op);
    }
};


void check_space_time_transform(const Func &func) {
    if (func.function().has_merged_defs()
        && func.function().has_stt()
        && !func.function().definition().schedule().is_output()
        && !func.function().definition().schedule().is_input()) {
        const auto &args = func.args();
        SttChecker chker(args);

        for (auto &f : func.function().definition().schedule().merged_ures()) {
            Tuple values = f.values();
			for (size_t i = 0; i < values.size(); i++) {
				values[i].accept(&chker);
			}
        }
        size_t num_stt_args = chker.get_num_stt_args();
        auto& param = func.function().definition().schedule().transform_params()[0];
        size_t num_src_vars = param.sch_vector.size();
        size_t num_space_vars = param.proj_matrix.size();

        // Take 3D-loop-2-1 as an example:
        // the reverse transform have the following form:
        // {i2, i2,             keep the space loop unchanged
        //  j2, dst % J2        J2 is the original time loop bound
        //  k2, dst / J2        k2 is the flattened loop, dst = j2 + J2*k2
        if (num_src_vars < num_stt_args) {
            debug(4) << "STT does not cover all the dependencies, "
                            "try to flatten the time loop...";
            int loop_bounds = 0;
            auto dims = func.function().definition().schedule().dims();

            for (size_t i = 0; i < num_space_vars; i++) {
                for (size_t j = 0; j < num_stt_args - num_src_vars; j++)
                    param.proj_matrix[i].push_back(0);
            }
            // the original time loop bound
            for (size_t i = 0; i < num_src_vars; i++) {
                auto expr = func.function().get_bounds(dims[i].var).second;
                const IntImm *bound = expr.as<IntImm>();
                user_assert(bound)
                    << "You must specify the loop bound of " << dims[i].var;
                loop_bounds += param.sch_vector[i] * (int)bound->value;
            }
            string name = func.name() + ".s0." + dims[num_src_vars-1].var;
            Expr ori_rev_expr = param.reverse[name];
            Expr rev_expr = ori_rev_expr % (int)loop_bounds;
            param.reverse[name] = simplify(rev_expr);
            debug(3) << name << ": " << rev_expr << "\n";

            // the flattened loop
            rev_expr = ori_rev_expr / loop_bounds;
            param.sch_vector.push_back(loop_bounds);
            for (size_t i = num_src_vars; i < num_stt_args-1; i++) {
                auto expr = func.function().get_bounds(dims[i].var).second;
                const IntImm *bound = expr.as<IntImm>();
                user_assert(bound)
                    << "You must specify the loop bound of " << dims[i].var;
                rev_expr = rev_expr % (int)bound->value;
                name = func.name() + ".s0." + dims[i].var;
                param.reverse.insert({ name, simplify(rev_expr) });
                debug(3) << name << ": " << rev_expr << "\n";

                loop_bounds *= bound->value;
                param.sch_vector.push_back(loop_bounds);
                rev_expr = ori_rev_expr / loop_bounds;
            }
            name = func.name() + ".s0." + dims[num_stt_args - 1].var;
            param.reverse.insert({ name, simplify(rev_expr) });
            debug(3) << name << ": " << rev_expr << "\n";
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

void t2s_preprocess_before_lower(map<string, Func> &env, const Target &target) {
    debug(4) << "Preprocessing functions in the environment:\n";
    for (auto &e : env) {
        auto &func = e.second;

        // In the function, for loops marked as removed, make their extents as 1. For the
        // calls of the function, later we will fix their args corresponding to the loops.
        convert_removed_loops_to_unit_loops(func);

        // Space-time transform
        func.apply_same_loop_transform_to_merged_ures();
        check_space_time_transform(func);
    }
}

}
}
