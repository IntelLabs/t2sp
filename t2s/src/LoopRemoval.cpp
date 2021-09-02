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


#include "../../Halide/src/IR.h"
#include "../../Halide/src/Func.h"
#include "../../Halide/src/Function.h"
#include "../../Halide/src/Definition.h"
#include "../../Halide/src/Util.h"

#include "IRMutator.h"
#include "Bounds.h"
#include "Substitute.h"
#include "./DebugPrint.h"

#include "./LoopRemoval.h"


namespace Halide {

using namespace Internal;

Func &Func::remove(VarOrRVar var) {
    // Allow removal of the var only when the Func was isolated as a producer.
    string isolated_from = func.isolated_from_as_consumer();
    user_assert(isolated_from == "") << "Dimension " << var.name() << " cannot be removed from Func " << name()
        << " because the Func was isolated from Func " << isolated_from << " as a consumer, in which case the Func is"
        << " supposed to keep its dimensions intact";
    isolated_from = func.isolated_from_as_producer();
    user_assert(isolated_from != "") << "Dimension " << var.name() << " cannot be removed from Func " << name()
        << ". Only a Func that was isolated from some other Func as a producer is allowed to remove some dimensions.\n";
    debug(4) << "Dimension " << var.name() << " is to be removed from Func " << name() << " and all its merged UREs! Make sure that the dimension is indeed not needed.\n";
    invalidate_cache();
    
    // Stage(func, func.definition(), 0).remove(var);
    std::vector<std::string>& remove_params = func.definition().schedule().remove_params();
    remove_params.push_back(var.name());
    debug(4) << "After removing " << var.name() << " from " << name() << ":\n"
             << to_string(*this, false, true) << "\n";

    return *this;
}

Func &Func::remove(const std::vector<VarOrRVar> &vars) {
    for (auto v : vars) {
        remove(v);
    }
    return *this;
}

namespace Internal {

class FixCallArgsForRemovedLoops : public IRMutator {
    using IRMutator::visit;
private:
    // Info for the current Func
    string func_name;           // Current func name
    map<string, Expr> loop2min; // Loop full name --> loop's min
public:
    const map<string, Function> &env;

    FixCallArgsForRemovedLoops(const map<string, Function> &env) : env(env) {}

    Stmt visit(const ProducerConsumer *op) override {
        if (op->is_producer) {
            func_name = op->name;
            loop2min.clear();
            Stmt s = IRMutator::visit(op);
            func_name.clear();
            loop2min.clear();
            return s;
        } else {
            internal_assert(func_name.empty());
            internal_assert(loop2min.empty());
            return IRMutator::visit(op);
        }
    }

    Stmt visit(const For *op) override {
        if(!ends_with(op->name,".run_on_device")){
            loop2min[op->name] = op->min;
        }
        return IRMutator::visit(op);
    }

    Expr visit(const Call *op) override {
        if (env.find(op->name) != env.end()) {
            const Function &f = env.at(op->name);
            const auto &removed_loops = f.definition().schedule().remove_params();
            if (!removed_loops.empty()) {
                vector<Expr> new_args(op->args);
                for (auto &r : removed_loops) {
                    string loop = func_name + ".s0." + r;
                    if (loop2min.find(loop) != loop2min.end()) {
                        Expr min = loop2min.at(loop);
                        vector<Expr> temp;
                        for (auto &a : new_args) {
                            Expr new_arg = substitute(loop, min, a);
                            temp.push_back(new_arg);
                        }
                        new_args = std::move(temp);
                    }
                }
                return Call::make(op->type, op->name, new_args, op->call_type, op->func, op->value_index, op->image, op->param);
            }
        }
        return op;
    }
};

Stmt fix_call_args_for_removed_loops(Stmt s, const std::map<std::string, Function> &env) {
    return FixCallArgsForRemovedLoops(env).mutate(s);
}

} // Internal
} // Halide

