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
#include "ComputeLoopBounds.h"
#include "../../Halide/src/Simplify.h"
#include "../../Halide/src/Substitute.h"

namespace Halide {
namespace Internal {
class GlobalLoopBoundsCollector : public IRVisitor {
public:
    GlobalLoopBoundsCollector(LoopBounds &bounds) : global_bounds(bounds) {} 

private:
    using IRVisitor::visit;

    LoopBounds &global_bounds;
    std::map<std::string, Expr> global_min;
    std::map<std::string, Expr> global_max;

    void visit(const For* op) override {
        Expr extent = op->extent;
        Expr loop_min = op->min;

        if (extent.as<IntImm>() == nullptr && !global_max.empty()) {
            // This substitute system has some problems
            // We should use a combination way to subsitute max and min
            Expr max_substitute = substitute(global_max, extent);
            Expr min_substitute = substitute(global_min, extent);
            extent = Max::make(max_substitute, min_substitute);
            max_substitute = substitute(global_max, loop_min);
            min_substitute = substitute(global_min, loop_min);
            loop_min = Min::make(max_substitute, min_substitute);
        }
        global_max[op->name] = simplify(extent + loop_min);
        global_min[op->name] = simplify(loop_min);
        global_bounds[op->name] = Interval(global_min[op->name], global_max[op->name]);
        op->body.accept(this);
        global_max.erase(op->name);
        global_min.erase(op->name);
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
    }
};

LoopBounds compute_global_loop_bounds(const Stmt &stmt) {
    LoopBounds bounds;
    GlobalLoopBoundsCollector visitor(bounds);
    stmt.accept(&visitor);
    return bounds;
}

}  // namespace Internal
}  // namespace Halide