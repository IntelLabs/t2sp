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
#include "../../Halide/src/IRMutator.h"
#include "../../Halide/src/IRVisitor.h"
#include "../../Halide/src/Simplify.h"
#include "../../Halide/src/IREquality.h"
#include "./RemoveConds.h"
#include "./Utilities.h"

namespace Halide {
namespace Internal {

using std::string;
using std::vector;

class RemoveCond : public IRMutator
{
    struct Reduce {
        string name;
        string sink_loop;
        Type t;
        Expr init_value;
        Expr update_value;
        const Call *ori_call;
    };
    vector<string> loops;
    vector<Reduce> reduce_vars;
    vector<std::pair<string, Type>> allocs;
    Stmt update;

    bool find_reduce(string w_name, Expr v, vector<Expr> w_dims) {
        auto add = v.as<Add>();
        auto sel = add ? add->a.as<Select>() : 0;
        if (!add || !sel) return false;

        vector<Expr> conds = break_logic_into_conjunction(sel->condition);
        string sink_loop;
        // Check if x==x.min and index contains no x
        for (auto l = loops.rbegin(); l != loops.rend(); ++l) {
            string tmp = sink_loop;
            for (auto c = conds.begin(); c != conds.end(); ++c) {
                const EQ *eq = c->as<EQ>();
                if (!eq) {
                    continue;
                }
                auto a = eq->a.as<Variable>();
                auto b = eq->b.as<IntImm>();
                if ((a && a->name == *l) && (b && b->value == 0)) {
                    bool find_var = false;
                    for (auto &d : w_dims) {
                        auto v = d.as<Variable>();
                        if (v && v->name == a->name) {
                            find_var = true;
                        }
                    }
                    if (!find_var) {
                        sink_loop = a->name;
                        c = conds.erase(c);
                        break;
                    }
                }
            }
            if (tmp == sink_loop) {
                break;
            }
        }
        // Check if the false value is an reduction
        bool is_reduce = true;
        auto read_call = sel->false_value.as<Call>();
        if (read_call && read_call->is_intrinsic(Call::read_shift_reg)) {
            string r_name = read_call->args[0].as<StringImm>()->value;
            vector<Expr> r_dims(read_call->args.begin()+1, read_call->args.end());
            if (r_name == w_name) {
                internal_assert(w_dims.size() == r_dims.size());
                for (size_t i = 0; i < w_dims.size(); i++) {
                    if (!equal(w_dims[i], r_dims[i])) {
                        is_reduce = false;
                    }
                }
            }
        }
        if (!sink_loop.empty() && is_reduce) {
            Expr new_cond = const_true();
            for (auto &c : conds) {
                new_cond = new_cond && c;
            }

            Reduce tmp;
            tmp.t = add->a.type();
            tmp.name = unique_name(w_name + ".temp");
            tmp.sink_loop  = sink_loop;
            tmp.init_value = Select::make(simplify(new_cond), sel->true_value, sel->false_value);
            tmp.update_value = Call::make(tmp.t, tmp.name, {0}, Call::Intrinsic) + add->b;
            reduce_vars.push_back(std::move(tmp));
            return true;
        }
        return false;
    }

public:
    using IRMutator::visit;

    Expr visit(const Call *op) override {
        if (op->is_intrinsic(Call::write_shift_reg)) {
            string name = op->args[0].as<StringImm>()->value;
            vector<Expr> dims(op->args.begin()+1, op->args.end()-1);
            Expr value = op->args.back();
            if (find_reduce(name, value, dims)) {
                Reduce &tmp = reduce_vars.back();
                tmp.ori_call = op;
                update = Provide::make(tmp.name, {tmp.update_value}, {0});
                return 0;
            }
        }
        return IRMutator::visit(op);
    }

    Stmt visit(const Evaluate *op) override {
        Expr value = mutate(op->value);
        if (update.defined()) {
            Stmt tmp = update;
            update = Stmt();
            return tmp;
        }
        return Evaluate::make(value);
    }

    Stmt visit(const For *op) override {
        vector<Reduce> backup;
        reduce_vars.swap(backup);

        loops.push_back(op->name);
        Stmt body = mutate(op->body);
        loops.pop_back();

        if (ends_with(op->name, "run_on_device")) {
            for (auto &p : allocs) {
                body = Realize::make(p.first, {p.second}, MemoryType::Auto, {Range(0, 1)}, const_true(), body);
            }
            allocs.clear();
        }
        body = For::make(op->name, op->min, op->extent,
                         op->for_type, op->device_api, body);
        for (auto it = reduce_vars.begin(); it != reduce_vars.end(); ) {
            if (it->sink_loop == op->name) {
                // Above the loop body, we initialize the reduction variable
                Expr value = it->init_value;
                Stmt init = Provide::make(it->name, {value}, {0});
                body = Block::make(init, body);
                // Below the loop body, we write back the reduction variable
                auto call = it->ori_call;
                vector<Expr> call_args(call->args.begin(), call->args.end()-1);
                call_args.push_back(Call::make(it->t, it->name, {0}, Call::Intrinsic));
                Expr write_back = Call::make(call->type, Call::write_shift_reg, call_args, Call::Intrinsic);
                body = Block::make(body, Evaluate::make(write_back));
                // Allocates the reduction variable
                allocs.push_back({ it->name, it->t });
                it = reduce_vars.erase(it);
            } else ++it;
        }
        reduce_vars.insert(reduce_vars.begin(), backup.begin(), backup.end());
        return body;
    }
};

Stmt remove_conditions(Stmt s) {
    RemoveCond rc;
    s = rc.mutate(s);
    return s;
}

}
}
