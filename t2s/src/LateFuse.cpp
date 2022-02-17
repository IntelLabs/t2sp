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
#include "DebugPrint.h"
#include "LateFuse.h"
#include "Utilities.h"

namespace Halide {
namespace Internal {

class ReplaceChannelsWithRegs: public IRMutator {
    using IRMutator::visit;

    string fuse_func_name;
    Region bounds;
    const std::vector<string> &access_args;
    std::vector<string> loop_names;

public:
    ReplaceChannelsWithRegs(string fuse_func_name, Region bounds, const std::vector<string> &access_args) 
                            : fuse_func_name(fuse_func_name), bounds(bounds), access_args(access_args) {}

    Expr visit(const Call *op) override {
        if (op->is_intrinsic(Call::read_channel)) {
            internal_assert(op->args[0].as<StringImm>());
            string name = op->args[0].as<StringImm>()->value;
            if (name == fuse_func_name + ".channel") {
                std::vector<Expr> cons_read_args;  // args for consumer to read registers
                for (auto arg : access_args) {
                    bool found = false;
                    for (auto name : loop_names) {
                        if (extract_last_token(name) == arg) {
                            found = true;
                            cons_read_args.push_back(Variable::make(Int(32), name));
                            break;
                        }
                    }
                    if (!found) {
                        cons_read_args.push_back(Call::make(Int(32), arg + ".temp", {}, Call::Intrinsic));
                    }
                }
                debug(4) << "cons_read_args: " << to_string<Expr>(cons_read_args) << "\n";
                std::vector<Expr> new_args(cons_read_args);
                new_args.insert(new_args.begin(), StringImm::make(fuse_func_name + ".shreg"));
                return Call::make(op->type, Call::read_shift_reg, new_args, op->call_type, op->func, op->value_index,
                                  op->image, op->param);
            } else {
                return IRMutator::visit(op);
            }
        }
        if (op->is_intrinsic(Call::write_channel)) {
            internal_assert(op->args[0].as<StringImm>());
            string name = op->args[0].as<StringImm>()->value;
            if (name == fuse_func_name + ".channel") {
                std::vector<Expr> prod_write_args; // args for producer to write registers
                for (auto arg : access_args) {
                    bool found = false;
                    for (auto name : loop_names) {
                        if (extract_last_token(name) == arg) {
                            found = true;
                            prod_write_args.push_back(Variable::make(Int(32), name));
                            break;
                        }
                    }
                    if (!found) {
                        prod_write_args.push_back(Call::make(Int(32), arg + ".temp", {}, Call::Intrinsic));
                    }
                }
                debug(4) << "prod_write_args: " << to_string<Expr>(prod_write_args) << "\n";
                std::vector<Expr> new_args(prod_write_args);
                new_args.insert(new_args.begin(), StringImm::make(fuse_func_name + ".shreg"));
                new_args.push_back(op->args[1]);
                return Call::make(op->type, Call::write_shift_reg, new_args, op->call_type, op->func, op->value_index,
                                  op->image, op->param);
            } else {
                return IRMutator::visit(op);
            }
        }
        return IRMutator::visit(op);
    }

    Stmt visit(const For *op) override {
        if (!ends_with(op->name, ".run_on_device")) {
            loop_names.push_back(op->name);
        }
        Stmt s = IRMutator::visit(op);
        if (!ends_with(op->name, ".run_on_device")) {
            loop_names.pop_back();
        }
        return s;
    }

    Stmt visit(const Realize *op) override {
        if (op->name == fuse_func_name + ".channel") {
            Stmt body = mutate(op->body);
            return Realize::make(fuse_func_name + ".shreg", op->types,
                                 op->memory_type, bounds,
                                 op->condition, body);
        }
        return IRMutator::visit(op);
    }
};

class CollectLoopInfo: public IRMutator {
    using IRMutator::visit;

    std::string fuse_level;
    bool under_fuse_level;
    std::map<std::string, Range> &loop_info;

public:
    CollectLoopInfo(std::string fuse_level, std::map<std::string, Range> &loop_info) : fuse_level(fuse_level), loop_info(loop_info) {
        under_fuse_level = false;
    }

    Stmt visit(const For *op) override {
        if (op->name == fuse_level) {
            under_fuse_level = true;
            Stmt s = (ends_with(op->name, ".run_on_device")) ? IRMutator::visit(op) : mutate(op->body);
            under_fuse_level = false;
            return s;
        } else if (under_fuse_level) {
            std::string loop_var = extract_last_token(op->name);
            if (loop_var != "scatter" && loop_var != "gather" && loop_var != "run_on_device") {
                loop_info.insert({loop_var, Range(op->min, op->extent)});
            }
            return IRMutator::visit(op);
        } else {
            return (ends_with(op->name, ".run_on_device")) ? IRMutator::visit(op) : mutate(op->body);
        }
    }
};

class PreprocessBeforeFusing: public IRMutator {
    using IRMutator::visit;

    Function fuse_func;
    std::string fuse_level;
    std::map<std::string, Range> &loop_info;

public:
    Stmt fused_body;
    PreprocessBeforeFusing(Function f, std::string v, std::map<std::string, Range> &loop_info) 
                           : fuse_func(f), fuse_level(v), loop_info(loop_info) {
    }

    Stmt visit(const ProducerConsumer *op) override {
        if (op->name == fuse_func.name()) {
            if (op->is_producer) {
                std::string loop_name = fuse_func.name() + ".s0" + "." + extract_last_token(fuse_level);
                CollectLoopInfo collector(loop_name, loop_info);
                fused_body = collector.mutate(op->body);
                return Stmt();
            } else {
                return mutate(op->body);
            }
        }
        return IRMutator::visit(op);
    }
}; 

class LateFuse: public IRMutator {
    using IRMutator::visit;

    bool fuse_flag;
    std::string fuse_level;
    Stmt fused_body;

public:
    LateFuse(bool fuse_flag, std::string v, Stmt fused_body) 
             : fuse_flag(fuse_flag), fuse_level(v), fused_body(fused_body) {
    }

    Stmt visit(const For *op) override {
        if (starts_with(op->name, fuse_level)) {
            Stmt body_s = mutate(op->body);
            Stmt block_s;
            if (fuse_flag) {
                block_s = Block::make(body_s, fused_body);
            } else {
                block_s = Block::make(fused_body, body_s);
            }
            Stmt for_s = For::make(op->name, op->min, op->extent,
                                   op->for_type, op->device_api, block_s);
            return for_s;
        }
        return IRMutator::visit(op); 
    }
}; 

Stmt do_late_fuse(Stmt stmt, const std::map<std::string, Function> &env) {
    for (auto &pair : env) {
        const std::string &lvl = pair.second.schedule().late_fuse_level();
        if (!lvl.empty()) {
            /* A.late_fuse(B, i) */
            /* original IR */
            /* 
            realize channel ch[] {
                producer A {                  // kernel A
                    for  i, j, k
                        write channel ch[]
                }
                consumer A {
                    producer B {              // kernel B
                        for i, j, k
                            read channel ch[]
                    }
                }
            }
            */
            std::map<std::string, Range> loop_info;
            Stmt fused_body;
            PreprocessBeforeFusing processor(pair.second, lvl, loop_info);
            stmt = processor.mutate(stmt);
            bool fuse_flag;
            
            if (pair.second.isolated_from_as_producer() == extract_first_token(lvl)) {
                fuse_flag = false;
                debug(3) << "Fuse producer " << pair.first << " with consumer " << extract_first_token(lvl) <<"\n";
            } else {
                fuse_flag = true;
                debug(3) << "Fuse producer " << extract_first_token(lvl) << " with consumer " << pair.first <<"\n";
            }
            Region bounds;                   // bounds for realize of the registers
            std::vector<string> access_args; // args for accessing registers
            for (auto kv : loop_info) {
                bounds.push_back(kv.second);
                access_args.push_back(kv.first);
            }

            LateFuse fuse(fuse_flag, lvl, processor.fused_body);
            stmt = fuse.mutate(stmt);
            /*
            realize channel ch[] {
                producer B {
                    for i
                        for j, k
                            write channel ch[]
                        for j, k
                            read channel ch[]
                }
            }
            */
            if (!fuse_flag) {
                ReplaceChannelsWithRegs replacer(pair.second.name(), bounds, access_args);
                stmt = replacer.mutate(stmt);
            } else {
                ReplaceChannelsWithRegs replacer(pair.second.isolated_from_as_consumer(), bounds, access_args);
                stmt = replacer.mutate(stmt);
            }
            
            /*
            realize shreg r[][] {
                producer B {
                    for i
                        for j, k
                            write reg r[][]
                        for j, k
                            read reg r[][]
                }
            }
            */
        }
    }

    return stmt;
}

}  // namespace Internal
}  // namespace Halide