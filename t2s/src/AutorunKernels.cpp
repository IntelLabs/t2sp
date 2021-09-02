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
#include "Var.h"
#include "IR.h"
#include "Func.h"
#include "Function.h"
#include "IRVisitor.h"
#include "IRMutator.h"
#include "Substitute.h"
#include "./AutorunKernels.h"
#include "./DebugPrint.h"
#include "./Utilities.h"
#include <algorithm>
#include <set>

namespace Halide {

using namespace Internal;
using std::map;
using std::string;
using std::vector;
using std::set;

namespace Internal {
class FindNonAutorunnableFuncs: public IRVisitor {
    using IRVisitor::visit;
    const map<string, Function> &env;
    vector<string> producer_stack;
    bool in_device_func;
    Scope<> ignore;

public:
    FindNonAutorunnableFuncs(const map<string, Function> &_env) :
        env(_env), in_device_func(false) {}

    set<string> non_autorunnable_funcs;

    // For each non-autorunnable func, remember why it is not autorunnable
    set<string> hostFuncs;
    map<string, set<string>> func2ExternalVars;
    map<string, set<string>> func2ExternalAllocations;
    map<string, set<string>> func2CallsWithSideEffects;

private:
    void visit(const ProducerConsumer *op) override {
        if (!op->is_producer) {
            op->body.accept(this);
            return;
        }

        string func_name = extract_first_token(op->name);
        Function func;
        if (!function_is_in_environment(func_name, env, func)) {
            return;
        }

        if (func.place() != Place::Device) {
            // Host kernel cannot be autorun.
            non_autorunnable_funcs.emplace(func_name);
            hostFuncs.emplace(func_name);
        } else {
            bool old = in_device_func;
            in_device_func = true;

            producer_stack.push_back(func_name);
            op->body.accept(this);
            producer_stack.pop_back();

            in_device_func = old;
        }
    }

    void visit(const Variable *op) override {
        if (in_device_func) {
            // If func refers to any external variable, the func needs arguments
            // for the variable, and thus cannot be autorun (An autorun kernel
            // cannot have any argument.)
            if (!ignore.contains(op->name)) {
                for (string func_name : producer_stack) {
                    non_autorunnable_funcs.emplace(func_name);
                    func2ExternalVars[func_name].emplace(op->name);
                }
            }
        }
    }

    void visit(const Let *op) override {
        op->value.accept(this);
        if (in_device_func) {
            ScopedBinding<> p(ignore, op->name);
            op->body.accept(this);
        } else {
            op->body.accept(this);
        }
    }

    void visit(const LetStmt *op) override {
        op->value.accept(this);
        if (in_device_func) {
            ScopedBinding<> p(ignore, op->name);
            op->body.accept(this);
        } else {
            op->body.accept(this);
        }
    }

    void visit(const Load* op) override {
        for (string func_name : producer_stack) {
            non_autorunnable_funcs.emplace(func_name);
            func2ExternalAllocations[func_name].emplace(op->name);
        }
    }

    void visit(const Store* op) override {
        for (string func_name : producer_stack) {
            non_autorunnable_funcs.emplace(func_name);
            func2ExternalAllocations[func_name].emplace(op->name);
        }
    }

    void visit(const For* op) override {
        op->min.accept(this);
        op->extent.accept(this);
        if (in_device_func) {
            ScopedBinding<> p(ignore, op->name);
            op->body.accept(this);
        } else {
            op->body.accept(this);
        }
    }

    void visit(const Call* op) override {
        if (op->call_type == Call::Extern ||
            op->call_type == Call::ExternCPlusPlus
            // TODO: for whatever reason, we have fake intrinsic calls like counter.temp, etc. which
            // do not really affect anything.
            /*||
            op->call_type == Call::Intrinsic*/) {
            for (string func_name : producer_stack) {
                non_autorunnable_funcs.emplace(func_name);
                func2CallsWithSideEffects[func_name].emplace(op->name);
            }
        }
        IRVisitor::visit(op);
    }
};

class AutorunFuncs: public IRMutator {
    using IRMutator::visit;
    const map<string, Function> &env;
    const map<string, string> &non_autorunnable_funcs_and_why;

public:
    AutorunFuncs(const map<string, Function> &_env,
            const map<string, string> &_non_autorunnable_funcs_and_why) :
            env(_env), non_autorunnable_funcs_and_why(_non_autorunnable_funcs_and_why) { }

    Stmt visit(const ProducerConsumer *op) override {
        string func_name = extract_first_token(op->name);
        if (env.find(func_name) == env.end()) {
            return op;
        }

        if (op->is_producer) {
            if (non_autorunnable_funcs_and_why.find(func_name) != non_autorunnable_funcs_and_why.end()) {
                return op;
            } else {
                Stmt stmt = mutate(op->body);
                return ProducerConsumer::make(op->name, op->is_producer, std::move(stmt));
            }
        } else {
            Stmt stmt = mutate(op->body);
            return ProducerConsumer::make(op->name, op->is_producer, std::move(stmt));
        }
    }

    Stmt visit(const For *op) override {
        if (ends_with(op->name, ".run_on_device")) {
            Stmt new_body = mutate(op->body);
            Stmt new_for = For::make(op->name.substr(0, op->name.length() - 14) + ".autorun.run_on_device",
                                     op->min, op->extent, op->for_type, op->device_api, new_body);
            return new_for;
        }

        if (op->for_type != ForType::Serial) {
            return IRMutator::visit(op);
        }

        if (ends_with(op->name, ".infinite")) {
            return op;
        }

        // Infinitize first serial loop
        Stmt new_stmt = Provide::make("counter.temp",
                            {Call::make(Int(32), "counter.temp", {}, Call::Intrinsic) + 1}, {});
        Stmt new_body = substitute(Variable::make(Int(32), op->name),
                            Call::make(Int(32), "counter.temp", {}, Call::Intrinsic), op->body);
        new_stmt = Block::make(new_body, new_stmt);
        new_stmt = For::make(op->name + ".infinite", 0, 10, ForType::Serial, op->device_api, new_stmt);
        new_stmt = Block::make(Provide::make("counter.temp", {0}, {}), new_stmt);
        new_stmt = Realize::make("counter.temp", {Int(32)},
                        MemoryType::Auto, {}, const_true(), new_stmt);
        return new_stmt;
    }
};

void are_kernels_autorunnable(Stmt s, const map<string, Function> &env, map<string, string> &non_autorunnable_funcs_and_why) {
    FindNonAutorunnableFuncs finder(env);
    s.accept(&finder);

    for (auto e : env) {
        string func_name = e.first;
        if (finder.non_autorunnable_funcs.find(func_name) != finder.non_autorunnable_funcs.end()) {
            string why = "Func " + func_name + " is not autorunnable for the following reasons:\n";
            if (finder.hostFuncs.find(func_name) != finder.hostFuncs.end()) {
                why += "  Func is not on the device\n";
            }
            if (finder.func2ExternalVars.find(func_name) != finder.func2ExternalVars.end()) {
                why += ("  Func refers to external variables: " + to_string<string>(finder.func2ExternalVars[func_name]) + "\n");
            }
            if (finder.func2ExternalAllocations.find(func_name) != finder.func2ExternalAllocations.end()) {
                why += ("  Func refers to external buffers: " + to_string<string>(finder.func2ExternalAllocations[func_name]) + "\n");
            }
            if (finder.func2CallsWithSideEffects.find(func_name) != finder.func2CallsWithSideEffects.end()) {
                why += ("  Func contains calls with side effects: " + to_string<string>(finder.func2CallsWithSideEffects[func_name]) + "\n");
            }
            non_autorunnable_funcs_and_why[func_name] = why;
        }
    }
}

Stmt autorun_kernels(Stmt stmt, const map<string, Function> &env) {
    map<string, string> non_autorunnable_funcs_and_why;
    are_kernels_autorunnable(stmt, env, non_autorunnable_funcs_and_why);

    for (auto n : non_autorunnable_funcs_and_why) {
        debug(4) << n.second << "\n";
    }

    AutorunFuncs autorun(env, non_autorunnable_funcs_and_why);
    Stmt s = autorun.mutate(stmt);
    
    return s;
}

}
}
