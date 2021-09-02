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
#include "CmdQueue.h"
#include "Overlay.h"

#include "../../Halide/src/Argument.h"
#include "../../Halide/src/Buffer.h"
#include "../../Halide/src/Definition.h"
#include "../../Halide/src/Expr.h"
#include "../../Halide/src/Func.h"
#include "../../Halide/src/Function.h"
#include "../../Halide/src/IREquality.h"
#include "../../Halide/src/IRMutator.h"
#include "../../Halide/src/IRVisitor.h"
#include "../../Halide/src/ImageParam.h"
#include "../../Halide/src/Schedule.h"

namespace Halide {

using namespace Internal;

std::set<int> queueNoSet;

// Add symbolic arguments to overlay kernels
Func &Func::command(int index, std::vector<Argument> inputs, std::vector<Argument> outputs, std::vector<Argument> inouts) {
    user_assert(this->defined())
        << "Overlay function not defined...\n";
    user_assert(func.definition().schedule().cmd_params().empty())
        << "can only define command interface once.\n";
    user_assert(index >= 0)
        << "Invalid queueNo " << index << "\n";
    user_assert(queueNoSet.insert(index).second)
        << "Duplicated queueNo " << index << "\n";
    user_assert(inouts.size() <= 1)
        << "At most one output buffer can be declared. \n";

    std::vector<CmdQueueItem> &cmd_params = func.definition().schedule().cmd_params();
    cmd_params.push_back(CmdQueueItem(index, inputs, inouts));
    return *this;
}

// Encode the dependency information into overlay
Func &Func::depend(Func &f, std::vector<Expr> &vars, std::vector<Expr> conds) {

    user_assert(vars.size() > 0) << "IterVar not defined.";
    for (Expr arg : vars)
        debug(4) << "Depending vars: " << arg << "\n";
    for (Expr arg : conds)
        debug(4) << "Depending conditions: " << arg << "\n";
    debug(4) << "Check overlay size: "
             << (*this).function().overlay().exprs.size() << "\n";
    // Extract IterVar offset (e.g. index = (i-1, j-2, k))
    class CollectOffset : public IRVisitor {
        using IRVisitor::visit;
        void visit(const Add *op) override {
            offset.push_back(op->b.as<IntImm>()->value);
            iter_vars.push_back(op->a);
        }
        void visit(const Sub *op) override {
            offset.push_back(-1 * op->b.as<IntImm>()->value);
            iter_vars.push_back(op->a);
        }

    public:
        vector<int> offset;
        vector<Expr> iter_vars;
    } offset;

    // Check the task id of depending funcs
    auto &funcs = (*this).function().overlay().definition().taskItems();
    int index_base = 0, task_id = -1;
    for (auto &func : funcs) {
        if (func.name() == f.name()) {
            task_id = index_base;
            break;
        }
        index_base++;
    }
    user_assert(task_id != -1) << "Not found function " << f.name();

    std::map<int, std::vector<Expr>> &task_deps = (*this).function().definition().schedule().task_deps();
    vector<Expr> dep_vec;
    if (conds.size() > 0) {
        for (auto &expr : vars) {
            auto size = offset.offset.size();
            expr.accept(&offset);
            // user_assert(size == offset.offset.size() - 1) << "Not found offset in " << expr;
            if (size == offset.offset.size()) {
                offset.iter_vars.push_back(expr);
                offset.offset.push_back(0);
            }
            dep_vec.push_back(offset.iter_vars[size]);
            dep_vec.push_back(offset.offset[size]);
        }
        for (auto &cond : conds)
            dep_vec.push_back(cond);

        user_assert((*this).defined() && (*this).function().updates().size() == 0);

        // Regular iter space index
        // Task2.depend(Task1, i).depend(Task0, i);
    } else {
        for (auto &expr : vars) {
            dep_vec.push_back(expr);
            dep_vec.push_back(make_const(Int(32), 0));
        }
    }
    user_assert(dep_vec.size() > 0);
    task_deps[task_id] = dep_vec;
    return *this;
}

namespace Internal {

} // namespace Internal
} // namespace Halide
