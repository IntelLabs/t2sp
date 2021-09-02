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
#include <set>
#include <string>

#include "CheckRecursiveCalls.h"
#include "../../Halide/src/IRVisitor.h"

namespace Halide {
namespace Internal {

using std::map;
using std::set;
using std::string;

class FindCallsInExpr : public IRVisitor {
public:

    map<string, Function> calls;

    using IRVisitor::visit;

    void visit(const Call *call) override {
        IRVisitor::visit(call);

        if (call->call_type == Call::Halide && call->func.defined()) {
            Function f(call->func);
            calls[call->name] = f;
        }
    }
};

/* Find all the halide calls that have no condition in an expr */
class FindNoConditionCalls : public IRVisitor {
public:
    string func_name;
    set<string> calls;
    set<string> already_tranversed;

    using IRVisitor::visit;

    void tranverse(string name, const Function& f) {
        func_name = name;
        calls.insert(name);
        f.definition().accept(this);
    }

    void clear() {
        calls.clear();
    }

    void include_function(const Function& f) {
        set<string>::iterator iter = calls.find(f.name());

        user_assert(iter == calls.end())
            << "Can't find a base value for function: "
            << func_name << "\n";

        calls.insert(f.name());
    }

    void tranverse_same_calls(FindCallsInExpr& calls1, FindCallsInExpr& calls2) {
        for (auto element : calls1.calls) {
            if (calls2.calls.find(element.first) != calls2.calls.end()) {
                include_function(element.second);
                element.second.accept(this);
            }
        }
    }

    void visit(const Call *call) override {
        if (call->call_type == Call::Halide && call->func.defined()) {
            Function f(call->func);

            if (already_tranversed.find(f.name()) != already_tranversed.end()) {
                return;
            }

            include_function(f);
            f.definition().accept(this);
            already_tranversed.insert(f.name());
        } else {
            IRVisitor::visit(call);
        }
    }

    void visit(const Select* op) override {
        // Because Select node can include initial condition 
        // and we can't do thorough analyze for this,
        // traversal must stops at here.
        // View Select node as Expr(0).

        // But one simple situation we can handle:
        // Both true condition and false condition have calls to same function.
        // Like g(i) = select(i == 0, f(i), f(i));
        // This means the f is definitely called.
        // const Expr& true_value = op->true_value;
        // const Expr& false_value = op->false_value;
        // FindCallsInExpr calls1, calls2;
        // true_value.accept(&calls1);
        // if (false_value.defined())    // allow false_value is none
        //     false_value.accept(&calls2);

        // // Keep tranverse the calls that appear in both true and false branch
        // tranverse_same_calls(calls1, calls2);

        // The condition expr is definitely evaluated.
        const Expr& condition = op->condition;
        condition.accept(this);
    }

    void visit(const IfThenElse* op) override {
        // Same reason as Select node.
        // const Stmt& true_value = op->then_case;
        // const Stmt& false_value = op->else_case;
        // FindCallsInExpr calls1, calls2;
        // true_value.accept(&calls1);
        // false_value.accept(&calls2);

        // tranverse_same_calls(calls1, calls2);

        // The condition is definitely evaluated.
        const Expr& condition = op->condition;
        condition.accept(this);
    }

    void visit(const Let* op) override {
        // Do nothing
        // For the situation that the common function calls are extract form the original definition
    }
};

/* TODO
class RecursiveCallTree {

    set<string> funcs_has_direct_value_node;
    map<string, set<string>> funcs_to_their_father;

public:
    void insert_path_to_func(string caller, string callee) {
        if (funcs_to_their_father.find(callee) == funcs_to_their_father.end()) {
            funcs_to_their_father[callee] = set<string>({caller}); 
        } else {
            funcs_to_their_father[callee].insert(caller);
        }
    }

    void insert_path_to_value(string caller) {
        funcs_has_direct_value_node.insert(caller);
    }

    void has_direct_initial_value(string func_name) {
        return 
    }

};

void build_call_tree() {
    
}
*/

void check_recursive_calls(const map<string, Function> &env) {
    FindNoConditionCalls calls;
    for (auto element : env) {
        calls.clear();
        calls.tranverse(element.first, element.second);
    }
}

}  // namespace Internal
}  // namespace Halide
