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
#include "DeadDimRemoval.h"
#include "IR.h"
#include "IRMutator.h"
#include "IROperator.h"
#include "Utilities.h"

namespace Halide {
namespace Internal {

using std::vector;
using std::string;
using std::map;

/*
If the arguments to a dimension always be constant, we think it as a dead dimension.
This contains the following cases:
1. The access argument to a dimension is constant
2. The argument is not constant, but the path condition to this statement terminates the loop (like iii==0)
3. The write argument is constant or terminated, so the read argument is dead, and vice versa.
*/
struct StmtInfo {
    string name;
    bool is_write;
    vector<int> const_dims;
};
vector<StmtInfo> access_stmts;

StmtInfo &get_stmt_info(string name, bool is_write) {
    auto pos = std::find_if(access_stmts.begin(), access_stmts.end(),
                            [&](StmtInfo &s) { return s.name == name && s.is_write == is_write; });
    if (pos != access_stmts.end()) return *pos;

    access_stmts.push_back({ name, is_write, {} });
    return access_stmts.back();
}

class CollectDeadDims : public IRVisitor {
    Expr path_condition = const_true();

    void record_dims(const vector<Expr> &args, StmtInfo &si) {
        for (size_t i = 0; i < args.size(); i++) {
            int expected_value = -1;
            if (args[i].as<Variable>()) {
                string loop_name = args[i].as<Variable>()->name;
                Expr value;
                if (loop_var_is_constant_in_condition(loop_name, path_condition, value)) {
                    internal_assert(value.as<IntImm>());
                    expected_value = value.as<IntImm>()->value;
                }
            } else if (is_const(args[i])) {
                internal_assert(args[i].as<IntImm>());
                expected_value = args[i].as<IntImm>()->value;
            }
            if (si.const_dims.size() < args.size()) {
                si.const_dims.push_back(expected_value);
            } else if (expected_value != si.const_dims[i]) {
                si.const_dims[i] = -1;
            }
        }
    }

public:
    using IRVisitor::visit;

    void visit(const IfThenElse *op) override {
        op->condition.accept(this);
        Expr tmp_path_condition = path_condition;
        if (op->then_case.defined()) {
            path_condition = tmp_path_condition && op->condition;
            op->then_case.accept(this);
        }
        if (op->else_case.defined()) {
            path_condition = tmp_path_condition && !op->condition;
            op->else_case.accept(this);
        }
        path_condition = tmp_path_condition;
    }

    void visit(const Select *op) override {
        op->condition.accept(this);
        Expr tmp_path_condition = path_condition;
        if (op->true_value.defined()) {
            path_condition = tmp_path_condition && op->condition;
            op->true_value.accept(this);
        }
        if (op->false_value.defined()) {
            path_condition = tmp_path_condition && !op->condition;
            op->false_value.accept(this);
        }
        path_condition = tmp_path_condition;
    }

    void visit(const Provide *op) override {
        if (ends_with(op->name, ".temp")) {
            record_dims(op->args, get_stmt_info(op->name, true));
        }
        IRVisitor::visit(op);
    }

    void visit(const Call *op) override {
        if (op->is_intrinsic(Call::read_channel) || op->is_intrinsic(Call::write_channel)
        || op->is_intrinsic(Call::read_shift_reg) || op->is_intrinsic(Call::write_shift_reg)) {
            string name = op->args[0].as<StringImm>()->value;
            // Trick: remove the postfix like .0
            string postfix = extract_last_token(name);
            if (postfix != "channel" && postfix != "shreg") {
                name = remove_postfix(name, "." + postfix);
            }
            bool is_write = op->is_intrinsic(Call::write_channel) || op->is_intrinsic(Call::write_shift_reg);
            StmtInfo &si = get_stmt_info(name, is_write);

            vector<Expr> args;
            if (!is_write) {
                std::copy(op->args.begin()+1, op->args.end(), std::back_inserter(args));
            } else if (op->is_intrinsic(Call::write_channel)) {
                std::copy(op->args.begin()+2, op->args.end(), std::back_inserter(args));
            } else if (op->is_intrinsic(Call::write_shift_reg)) {
                std::copy(op->args.begin()+1, op->args.end()-1, std::back_inserter(args));
            }
            record_dims(args, si);
        }
        if (ends_with(op->name, ".temp")) {
            record_dims(op->args, get_stmt_info(op->name, false));
        }
        IRVisitor::visit(op);
    }
};

class RemoveDeadDims : public IRMutator {

public:
    using IRMutator::visit;

    // Shrink the dead dimension to Range(0, 1)
    Stmt visit(const Realize *op) override {
        auto &read_dims = get_stmt_info(op->name, false).const_dims;
        auto &write_dims = get_stmt_info(op->name, true).const_dims;

        Stmt body = mutate(op->body);
        Region bounds = op->bounds;
        for (size_t i = 0; i < bounds.size(); i++) {
            if ((read_dims.size() > i && read_dims[i] == 0)
            || (write_dims.size() > i && write_dims[i] == 0)) {
                bounds[i] = Range(0, 1);
            }
        }
        return Realize::make(op->name, op->types, op->memory_type, bounds, op->condition, body);
    }
};

Stmt remove_dead_dimensions(Stmt s) {
    CollectDeadDims cdd;
    RemoveDeadDims rdd;
    s.accept(&cdd);
    s = rdd.mutate(s);
    return s;
}

}
}