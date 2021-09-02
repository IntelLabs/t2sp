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

#include "./NoIfSimplify.h"
#include "Simplify_Internal.h"
#include "IR.h"
#include "IRMutator.h"

namespace Halide {
namespace Internal {

namespace {

// Resolve and reduce the number of IfThenElse.
class Simplifier : public Simplify {
  public:
    Simplifier(bool keep_loops) :
        Simplify(true, &Scope<Interval>::empty_scope(), &Scope<ModulusRemainder>::empty_scope()),
        keep_loops(keep_loops) {}

  private:
    using Simplify::visit;

    bool keep_loops; // Keep a for loop even if its extent is 1, unless the loop is a fused loop

    // Do not propagate the simplified condition
    Stmt visit(const IfThenElse *op) override {
        Expr condition = mutate(op->condition, nullptr);
        
        if (is_one(condition)) {
            return mutate(op->then_case);
        }
        if (is_zero(condition)) {
            if (op->else_case.defined()) {
                return mutate(op->else_case);
            } else {
                return Evaluate::make(0);
            }
        }
        Stmt then_case, else_case;


        then_case = mutate(op->then_case);
        if (op->else_case.defined()) {
            else_case = mutate(op->else_case);
        }

        if (is_no_op(then_case) && is_no_op(else_case)) {
            return then_case;
        }

        if (equal(then_case, else_case)) {
            return then_case;
        }

        return IfThenElse::make(condition, then_case, else_case);
    }

    // Do not simplify loops with extent of 1
    Stmt visit(const For *op) override {
        std::vector<std::string> names = split_string(op->name, ".");
        if (keep_loops && names[2] != "fused") {
            Stmt body = mutate(op->body);
            Expr min = mutate(op->min, nullptr);
            Expr extent = mutate(op->extent, nullptr);
            return For::make(op->name, min, extent, op->for_type, op->device_api, body);
        } else {
            return Simplify::visit(op);
        }
    }
};

}

Stmt no_if_simplify(Stmt s, bool keep_loops) {
    Simplifier simplifier(keep_loops);
    return simplifier.mutate(s);
}

}
}
