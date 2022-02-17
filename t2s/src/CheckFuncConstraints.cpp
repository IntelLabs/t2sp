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
#include "./CheckFuncConstraints.h"
#include "./DebugPrint.h"
#include "../../Halide/src/Expr.h"
#include "../../Halide/src/IR.h"
#include "../../Halide/src/IRVisitor.h"
#include <string>
#include <set>

namespace Halide {
namespace Internal {
using std::string;
class IFSimpleExpr : public IRVisitor {
public:

    Expr expr;
    bool simple = true;
    string var_name;

    using IRVisitor::visit;

    IFSimpleExpr(Expr expr) : expr(expr) {}

    void visit(const FloatImm *num) override{
        simple = false;  // Don't allow i * 1.0
    }

    void visit(const StringImm *num) override{
        simple = false;
    }

    void visit(const Variable *op) override {
        if (var_name != "") {  // There is a second Variable
            simple = false;
            return;  
        }
        var_name = op->name;
    }

    void visit(const Cast *op) override{
        simple = false;
    }

    void visit(const Mul *op) override{
        simple = false;
    }

    void visit(const Div *op) override{
        simple = false;
    }

    void visit(const Mod *op) override{
        simple = false;
    }

    void visit(const Min *op) override{
        simple = false;
    }

    void visit(const Max *op) override{
        simple = false;
    }

    void visit(const EQ *op) override{
        simple = false;
    }

    void visit(const NE *op) override{
        simple = false;
    }

    void visit(const LT *op) override{
        simple = false;
    }

    void visit(const LE *op) override{
        simple = false;
    }

    void visit(const GT *op) override{
        simple = false;
    }

    void visit(const GE *op) override{
        op->a.accept(this);
        op->b.accept(this);
    }

    void visit(const And *op) override{
        simple = false;
    }

    void visit(const Or *op) override{
        simple = false;
    }

    void visit(const Not *op) override{
        simple = false;
    }

    void visit(const Select *op) override{
        simple = false;
    }

    void visit(const Call *op) override{
        simple = false;
    }

    void visit(const IfThenElse *op) override{
        simple = false;
    }

    void visit(const Shuffle *op) override{
        simple = false;
    }
};

class CheckCallsArgs : public IRVisitor {
public:
    typedef enum {
        NotSimple,
        WrongOrder,
        Lack
    } InvalidType;

    const std::vector<Func> &funcs; // URE candidates.
    vector<Var> args;
    bool is_ure = true;
    InvalidType invalid_type;
    const Call *invalid_call; // The call that fails the check

    using IRVisitor::visit;
    
    CheckCallsArgs(const std::vector<Func> &funcs, vector<Var> args) : funcs(funcs), args(args), invalid_call(NULL) { }

    void visit(const Call *call) override {
        bool is_ure_candidate = false;
        for (auto f : funcs) {
            if (f.name() == call->name) {
                is_ure_candidate = true;
                break;
            }
        }
        if (!is_ure_candidate) {
            // For example, let us say we want to check if Func X is URE, and
            //   X(i) = select(i == 0, Y(i), X(i - 1));
            // but Y is not a URE candidate. We should not check the call to Y then.
            return;
        }
        if (call->call_type == Call::Halide && call->func.defined() && is_ure) {
            Function f(call->func);
            if (f.definition().schedule().is_param_func()) {
                return; // The param function doesn't need to apply the constraints.
            }
            const vector<Expr> &exprs = call->args;
            vector<string> call_args;
            for (auto expr : exprs) {
                IFSimpleExpr simple_expr(expr);
                expr.accept(&simple_expr);
                is_ure = (is_ure && simple_expr.simple);
                if (simple_expr.var_name != "") {
                    call_args.push_back(simple_expr.var_name);
                }
                if (!is_ure) {
                    invalid_type = InvalidType::NotSimple;
                    invalid_call = call;
                    return;
                }
            }

            // if (call_args.size() < args.size()) {
            //     invalid_type = InvalidType::Lack;
            //     is_ure = false;
            //     invalid_call = call;
            //     return;
            // }

            // Compare the RHS order with LHS
            if (!args.empty() && !call_args.empty()) {
                unsigned i = 0, j = 0;
                while (i < args.size() && j < call_args.size()) {
                    if (args[i].name() == call_args[j]) {
                        i++, j++;
                    } else {
                        if (call_args.size() >= args.size()) {
                            j++;
                        } else {
                            i++;
                        }
                    }
                }
                if (i < args.size()) {
                    invalid_type = InvalidType::WrongOrder;
                    is_ure = false;
                    invalid_call = call;
                }
            }
        }
    }
};

class SelectCheck : public IRVisitor {
public:
    std::string func_name;

    using IRVisitor::visit;

    SelectCheck(std::string func_name) : func_name(func_name) { }

    void visit(const Select *op) override {
        user_assert(op->false_value.defined()) << "The Select node of Func " 
            << func_name << " doesn't have a false value.\n"
            << "It means you have a definition like " << func_name << "(...) = select(cond, true value). Note there is "
            << "no false value. However, this form is allowed only for the output Func and " << func_name << " is not an output Func. "
            << "Try add a false value to the definition of " << func_name << ".\n";

        IRVisitor::visit(op);
    }
};

void CheckFuncConstraints::check_declare(const Function &func) {

    std::set<std::string> names;
    for (const Expr& e : func.decl_args()) {
        std::string name = e.as<Variable>()->name;
        names.insert(name);
    }
    user_assert(names.size() == func.decl_args().size())
            << "Function " << func.name()
            << "'s definition contains repeated variables\n";
}

void CheckFuncConstraints::check_ures(const std::vector<Func> &funcs) {
    for (auto f : funcs) {
        CheckFuncConstraints::check_ure(f, funcs);
    }
}

void CheckFuncConstraints::check_ure(const Func& f, const std::vector<Func> &funcs, const vector<Var>& args) {
    CheckCallsArgs check_calls_args(funcs, f.args());
    f.function().accept(&check_calls_args);
    if (!check_calls_args.is_ure) {
        string message;
        switch(check_calls_args.invalid_type) {
            case CheckCallsArgs::InvalidType::NotSimple: 
                message = "The arguments of the call expected to be simple, i.e. have constant distances from the corresponding arguments of the definition of the Func.";
                break;
            case CheckCallsArgs::InvalidType::WrongOrder: 
                message = "The order of LHS arguments is different from RHS arguments.";
                break;
            case CheckCallsArgs::InvalidType::Lack: 
                message = "The RHS lack arguments compared to LHS.";
                break;
        }
        user_assert(check_calls_args.is_ure) << "The definition of Func " 
            << f.name() << "(" << names_to_string(f.args()) << ") doesn't satisfy URE constraints due to the call to "
            << to_string(check_calls_args.invalid_call) << ": " << message << "\n";
    }

    // Compare the LHS
    const vector<Var>& f_args = f.args();
    if (!args.empty() && !f_args.empty()) {
        unsigned i = 0, j = 0;
        while (i < args.size() && j < f_args.size()) {
            if (args[i].name() == f_args[j].name()) {
                i++, j++;
            } else {
                i++;
            }
        }
        user_assert(j == f_args.size()) << "The argument order of "
         << f.name() << " is different from the representative URE.\n";
    }

    // Check select
    if (!f.function().definition().schedule().is_output()) {
        SelectCheck select_check(f.function().name());
        f.function().accept(&select_check);
    }
}

void CheckFuncConstraints::check_merge_ures(const Func &control, const std::vector<Func> &funcs) {
    // All the UREs merged together should be at the same place
    Place place = control.function().place();
    for (auto f : funcs) {
        user_assert(f.function().place() == place) << "Func " << f.name() << " needs be declared to run at "
                << (place == Place::Host ? "Place::Host" : "Place::Device") << " in order to merge "
                << "with Func " << control.name() << ". That is, all Funcs must be declared "
                << "to run at the same place as the Func they are merged into.";
    }

    // Continue checking arguments, etc.
    vector<Var> args = control.args();
    CheckFuncConstraints::check_ure(control, funcs, args);
    for (auto f : funcs) {
        CheckFuncConstraints::check_ure(f, funcs, args);
    }
}

void CheckFuncConstraints::check_merge_ure(const Func& f) {
    // Todo
}

}
};
