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
#include <string>
#include <set>
#include <map>
#include "../../Halide/src/Function.h"
#include "../../Halide/src/IRMutator.h"
#include "../../Halide/src/Substitute.h"
#include "RmLets.h"

namespace Halide{
namespace Internal{
using std::vector;
using std::string;
using std::set;

class RmLets : public IRMutator{
    using IRMutator::visit;
    const set<string>& funcs;
    bool isRemove;
    Stmt visit(const ProducerConsumer* op) override{
        if(op->is_producer){
            if(funcs.find(op->name) != funcs.end()){
                bool already_remove = isRemove;
                isRemove = true;
                Stmt s = IRMutator::visit(op);
                isRemove = already_remove;
                return s;
            }
        }
        return IRMutator::visit(op);
    }
    Stmt visit(const LetStmt* op) override{
        if(isRemove){
            string name = op->name;
            Expr value = op->value;
            Stmt body = op->body;
            body = substitute(name,(value),body);
            return IRMutator::mutate(body);
        }
        return IRMutator::visit(op);
    }
    Expr visit(const Let* op) override{
        if(isRemove){
            string name = op->name;
            Expr value = op->value;
            Expr body = op->body;
            body = substitute(name,(value),body);
            return IRMutator::mutate(body);
        }
        return IRMutator::visit(op);
    }
public:
    RmLets(const set<string>&_funcs):funcs(_funcs),isRemove(false){}
};

Stmt rm_lets(Stmt s, const std::map<std::string, Function> &env){
    set<string> funcs;
    for(auto func : env){
        string func_name = func.first;
        auto isBuffer = func.second.definition().schedule().buffer_params().size() > 0;
        auto isScatter = func.second.definition().schedule().scatter_params().size() > 0;
        if(isBuffer || isScatter){
            funcs.insert(func_name);
        }
    } 
    RmLets rm = RmLets(funcs);
    return rm.mutate(s);
}

}
}