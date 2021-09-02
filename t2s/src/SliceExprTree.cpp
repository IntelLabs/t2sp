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
#include "./SliceExprTree.h"
#include "./DebugPrint.h"
#include "../../Halide/src/IREquality.h"
#include "../../Halide/src/IROperator.h"
#include <sstream>

namespace Halide {
namespace Internal {

using std::vector;
using std::string;

// We do not match IntImm, UIntImm, FloatImm, StringImm, Variable, statements,
// or any Expr that a user cannot specify in the source code (e.g. Load, Store,
// Provide, Let, etc.) However, we may use IRVisitor to match their sub-expressions.

// Match an expression. Only one place is allowed to be matched.
template<typename T>
void SliceExprTree::match_expr(const T* op) {
    debug(4) << "SliceExprTree: visiting Expr " << Internal::to_string(Expr(op)) << "\n";
    if (expr_to_match.defined() && equal(expr_to_match, op)) {
        user_assert(!matched_operand.defined())
            << error_msg_header << "Multiple matches for the expression "
            << expr_to_match << "\n";
        matched_operand = op;
    }
}

// Commonly, we try to match an expression. No matter it matches or not, we will
// continue to match the sub-expressions: even if a match already happens, we
// would like to check that there is only 1 match in the entire tree.
void SliceExprTree::visit(const Add *op) { match_expr(op); IRVisitor::visit(op); }
void SliceExprTree::visit(const Sub *op) { match_expr(op); IRVisitor::visit(op); }
void SliceExprTree::visit(const Mul *op) { match_expr(op); IRVisitor::visit(op); }
void SliceExprTree::visit(const Div *op) { match_expr(op); IRVisitor::visit(op); }
void SliceExprTree::visit(const Mod *op) { match_expr(op); IRVisitor::visit(op); }
void SliceExprTree::visit(const Min *op) { match_expr(op); IRVisitor::visit(op); }
void SliceExprTree::visit(const Max *op) { match_expr(op); IRVisitor::visit(op); }
void SliceExprTree::visit(const EQ *op)  { match_expr(op); IRVisitor::visit(op); }
void SliceExprTree::visit(const NE *op)  { match_expr(op); IRVisitor::visit(op); }
void SliceExprTree::visit(const LT *op)  { match_expr(op); IRVisitor::visit(op); }
void SliceExprTree::visit(const LE *op)  { match_expr(op); IRVisitor::visit(op); }
void SliceExprTree::visit(const GT *op)  { match_expr(op); IRVisitor::visit(op); }
void SliceExprTree::visit(const GE *op)  { match_expr(op); IRVisitor::visit(op); }
void SliceExprTree::visit(const And *op) { match_expr(op); IRVisitor::visit(op); }
void SliceExprTree::visit(const Or *op)  { match_expr(op); IRVisitor::visit(op); }
void SliceExprTree::visit(const Not *op) { match_expr(op); IRVisitor::visit(op); }
void SliceExprTree::visit(const Load *op) { match_expr(op); IRVisitor::visit(op); }

void SliceExprTree::visit(const Select *op) {
    match_expr(op);
    Expr old_condition = condition;
    if (matched_operand.defined()) {
        // We cannot change the matched operand, nor the condition leading to it.
        // We just want to check that the sub-expressions do not have another match.
        IRVisitor::visit(op);
    } else {
        condition = old_condition.defined() ? old_condition && op->condition : op->condition;
        op->true_value.accept(this);
        if (matched_operand.defined()) {
            // Check that the false value does not have another match.
            if (op->false_value.defined()) {
                op->false_value.accept(this);
            }
        } else {
            condition = old_condition.defined() ? old_condition && !op->condition : !op->condition;
            if (op->false_value.defined()) {
                op->false_value.accept(this);
                if (!matched_operand.defined()) {
                    // Recover the original condition
                    condition = old_condition;
                }
            }
        }

        // Check that the Select's condition does not have a match
        op->condition.accept(this);
    }
}

void SliceExprTree::visit(const Call *op) {
    debug(4) << "SliceExprTree: visiting call " << Internal::to_string(Expr(op)) << "\n";
    if (func_to_match != "") {
        if (op->name == func_to_match) {
            user_assert(!matched_operand.defined())
                << error_msg_header << "Multiple matches for calling Func "
                << func_to_match << "\n";
            matched_operand = op;
        }
    } else {
        match_expr(op);
    }

    IRVisitor::visit(op);
}

void SliceExprTree::visit(const Let *op) {
    if (!matched_operand.defined()) {
        lets.push_back(op);
        // No match_expr(op) because user cannot specify a Let Expr in source code.
        IRVisitor::visit(op);
        if (!matched_operand.defined()) {
            lets.pop_back();
        }
    } else {
        // Just check the sub_expressions does not have another match
        IRVisitor::visit(op);
    }
}

std::string SliceExprTree::to_string() const {
    if (!matched_operand.defined()) {
        return "No slice found";
    } else {
        Expr e = matched_operand;
        for (size_t j = 0; j < lets.size(); j++) {
            e = Let::make(lets[j]->name, lets[j]->value, e);
        }
        std::ostringstream s;
        s << "Select(" << condition << ", " << e << ")";
        return s.str();
    }
}

}
};
