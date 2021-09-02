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
#include "IR.h"
#include "IRMutator.h"
#include "IROperator.h"
#include "LateFuse.h"

namespace Halide {
namespace Internal {

class LateFuse: public IRMutator {
    using IRMutator::visit;

    Function fuse_func;
    std::string fuse_level;
    const ProducerConsumer *producer_node;
    const Realize *realize_node;

public:
    LateFuse(Function f, std::string v)
        : fuse_func(f), fuse_level(v) {}

    Stmt visit(const For *op) override {
        if (starts_with(op->name, fuse_level)) {
            Stmt body_s = mutate(op->body);
            Stmt produce_s = ProducerConsumer::make(producer_node->name, true,
                                                  producer_node->body);
            Stmt block_s = Block::make(produce_s, body_s);
            Stmt realize_s = Realize::make(realize_node->name, realize_node->types,
                                         realize_node->memory_type, realize_node->bounds,
                                         realize_node->condition, block_s);
            Stmt for_s = For::make(op->name, op->min, op->extent,
                                 op->for_type, op->device_api, realize_s);
            return for_s;
        }
        return IRMutator::visit(op); 
    }

    Stmt visit(const ProducerConsumer *op) override {
        if (op->name == fuse_func.name()) {
            if (op->is_producer) {
                producer_node = op;
                return Stmt();
            } else
                return mutate(op->body);
        }
        return IRMutator::visit(op);
    }

    Stmt visit(const Realize *op) override {
        if (op->name == fuse_func.name()) {
            realize_node = op;
            return mutate(op->body);
        }
        return IRMutator::visit(op);
    }
}; 

Stmt do_late_fuse(Stmt stmt, const std::map<std::string, Function> &env) {
    for (auto &pair : env) {
        const std::string &lvl = pair.second.schedule().late_fuse_level();
        if (!lvl.empty()) {
            LateFuse fuse(pair.second, lvl);
            stmt = fuse.mutate(stmt);
        }
    }

    return stmt;
}

}  // namespace Internal
}  // namespace Halide