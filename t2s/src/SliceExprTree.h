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
#ifndef T2S_SLICE_EXPR_TREE_H
#define T2S_SLICE_EXPR_TREE_H

#include "../../Halide/src/IRVisitor.h"
#include "../../Halide/src/Util.h"

#include <set>
#include <map>
#include <string>
#include <sstream>

/** \file
 * Slice an express tree for a path leading to an operand.
 * The operand can be a Func Call or an Expr.
 * There can be only a single match.
 * The operand cannot be an immediate number, string, variable,
 * or in the condition of a Select.
 */

namespace Halide {
namespace Internal {

// In an expression tree, match an operand (based on either a Func name,
// or an Expr). Record the condition of the path leading to the operand,
// and the Let Exprs on the path.
class SliceExprTree : public IRVisitor {
private:
    // The Func or Expr to match.
    std::string              func_to_match;
    Expr                     expr_to_match;

    // The message header when an error is encountered.
    std::string              error_msg_header;

public:
    // Results
    Expr                     matched_operand;
    Expr                     condition;
    std::vector<const Let*>  lets;

    using IRVisitor::visit;

    SliceExprTree(std::string func_to_match, Expr expr_to_match,
                  std::string error_msg_header = "") :
        func_to_match(func_to_match), expr_to_match(expr_to_match),
        error_msg_header(error_msg_header) {
            // Must match a Func name, or an Expr, but not both
            internal_assert(func_to_match != "" || expr_to_match.defined());
            internal_assert(!(func_to_match != "" && expr_to_match.defined()));
            matched_operand = Expr();
    }

    template<typename T>
    void match_expr(const T* op);

    void visit(const Add *op) override;
    void visit(const Sub *op) override;
    void visit(const Mul *op) override;
    void visit(const Div *op) override;
    void visit(const Mod *op) override;
    void visit(const Min *op) override;
    void visit(const Max *op) override;
    void visit(const EQ *op) override;
    void visit(const NE *op) override;
    void visit(const LT *op) override;
    void visit(const LE *op) override;
    void visit(const GT *op) override;
    void visit(const GE *op) override;
    void visit(const And *op) override;
    void visit(const Or *op) override;
    void visit(const Not *op) override;
    void visit(const Select *op) override;
    void visit(const Call *op) override;
    void visit(const Let *op) override;
    void visit(const Load *op) override;

    // Print out the slice found.
    std::string to_string() const;
};


}
}

#endif
