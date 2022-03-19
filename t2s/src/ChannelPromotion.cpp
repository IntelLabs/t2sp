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
#include "CSE.h"
#include "IR.h"
#include "IRMutator.h"
#include "IROperator.h"
#include "Simplify.h"
#include "Substitute.h"
#include "Utilities.h"

#include "./DebugPrint.h"
#include "./LoopRemoval.h"

namespace Halide {
namespace Internal {

struct PromotedChannel {
    bool is_write_chn;          // True if it is a write_channel intrinsic
    bool safe_promotion;        // True if it is guarded by promotable conditions
    string name;                // Channel name after eliminating unnecessary suffix
    string promotion_loop;      // The loop that the channel should be promoted above
    vector<Expr> args;          // The read/write_channel arguments
    Expr guarding_cond;         // The condition used to guard the operation after promotion
};

std::string get_channel_name(Expr e) {
    auto v = e.as<StringImm>();
    internal_assert(v);

    int size = (v->value).rfind(".");
    std::string channel_name = v->value;
    debug(4) << "channel name: " << channel_name << "\n";
    if (size < (int)(v->value.size())-1) {
        std::string suffix = (v->value).substr(size + 1, (int)v->value.size() - size - 1);
        // Eliminate useless suffix
        while (suffix != "channel") {
            channel_name = (channel_name).substr(0, size);
            size = (channel_name).rfind(".");
            if (size < (int)(channel_name.size())-1) {
                suffix = (channel_name).substr(size + 1, (int)channel_name.size() - size - 1);
            } else break;
            if (suffix != "channel") {
                channel_name = v->value;
            }
        }
    }
    debug(4) << "modified channel name: " << channel_name << "\n";
    return channel_name;
}

auto get_promoted_channel(vector<PromotedChannel> &channels, string name, bool is_write_chn)
-> vector<PromotedChannel>::iterator {
    auto it = channels.begin();
    for (; it != channels.end(); ++it) {
        if (it->name == name && it->is_write_chn == is_write_chn) break;
    }
    return it;
}

class VarsFinder : public IRVisitor
{
    string l = "__outermost";
    struct Container {
        vector<string> vars;
        vector<string> reads;
        vector<string> writes;
    };
    std::map<string, Container> loop_container;

public:
    using IRVisitor::visit;
    VarsFinder() {
        loop_container[l] = {};
    }

    void visit(const Variable* op) override {
        loop_container[l].vars.push_back(op->name);
    }

    void visit(const Call *op) override {
        if (op->is_intrinsic(Call::read_shift_reg)) {
            auto name = op->args[0].as<StringImm>();
            internal_assert(name);
            loop_container[l].reads.push_back(name->value);
        } else if (op->is_intrinsic(Call::write_shift_reg)) {
            auto name = op->args[0].as<StringImm>();
            internal_assert(name);
            loop_container[l].writes.push_back(name->value);
        } else if (ends_with(op->name, ".temp")) {
            loop_container[l].reads.push_back(op->name);
        }
    }

    void visit(const Provide *op) override {
        if (ends_with(op->name, ".temp")) {
            loop_container[l].writes.push_back(op->name);
        }
    }

    void visit(const For *op) override {
        string bak_l = l;
        l = op->name;
        loop_container[l] = {};

        op->body.accept(this);
        l = bak_l;
    }

    bool find_var(const vector<Expr> &args, const string &l = "__outermost") {
        auto &vars = loop_container[l].vars;
        for (auto &r : args) {
            auto v = r.as<Variable>();
            if (v) {
                auto p = std::find(vars.begin(), vars.end(), v->name);
                if (p != vars.end()) {
                    return true;
                }
            }
        }
        return false;
    }

    vector<string>& get_reads(const string &l = "__outermost") {
        return loop_container[l].reads;
    }

    bool find_writes(const vector<string> &names, const string &l = "__outermost") {
        auto &writes = loop_container[l].writes;
        for (auto &r : names) {
            auto p = std::find(writes.begin(), writes.end(), r);
            if (p != writes.end()) {
                return true;
            }
        }
        return false;
    }
};

class ChannelVisitor : public IRVisitor {
  public:
    ChannelVisitor(VarsFinder &vf)
        : loop_vf(vf) {}

    VarsFinder &loop_vf;
    vector<PromotedChannel> channels;
    vector<string> unrolled_loops;
    vector<string> loop_vars;

    // Path condition to a channel write/read
    Expr condition = const_true();

  private:
    using IRVisitor::visit;

    void visit(const For* op) override {
        if (op->for_type == ForType::Unrolled) {
            unrolled_loops.push_back(op->name);
        }
        loop_vars.push_back(op->name);

        op->body.accept(this);

        if (op->for_type == ForType::Unrolled) {
            unrolled_loops.pop_back();
        }
        loop_vars.pop_back();
    }

    void visit(const IfThenElse* op) override {
        if (loop_vars.size() > 0) {
            Expr temp_cond = condition;
            condition = simplify(temp_cond && (op->condition));
            op->then_case.accept(this);

            if (op->else_case.defined()) {
                condition = simplify(temp_cond && !(op->condition));
                op->else_case.accept(this);
            }
            condition = temp_cond;
            return;
        }
        IRVisitor::visit(op);
    }

    void visit(const Select *op) override {
        if (loop_vars.size() > 0) {
            Expr temp_cond = condition;
            condition = simplify(temp_cond && (op->condition));
            op->true_value.accept(this);

            if (op->false_value.defined()) {
                condition = simplify(temp_cond && !(op->condition));
                op->false_value.accept(this);
            }
            condition = temp_cond;
            return;
        }
        IRVisitor::visit(op);
    }

    bool find_loop_in_args(vector<Expr> args, string loop) {
        for (auto v : args) {
            if (v.as<Variable>() && v.as<Variable>()->name == loop)
                return true;
        }
        return false;
    }

    bool check_cond(Expr condition, vector<Expr> args, string &promotion_loop, Expr &guarding_cond) {
        vector<Expr> conjunction = break_logic_into_conjunction(condition);
        std::map<string, Expr> removable_conds;
        bool safe_promotion = true;
        // Three cases for condition:
        // 1. The condition is safe to be removed after promotion
        // 2. The condition cannot be removed but can be promoted
        // 3. The condition cannot be promoted or is unknown type
        VarsFinder cond_vf;
        for (auto &c : conjunction) {
            auto eq = c.as<EQ>();
            if (!eq) {
                c.accept(&cond_vf);
                if (cond_vf.find_var(args)) {
                    // For non-equality conjunction, if it contains channel arguments, the promotion is unsafe
                    safe_promotion = false;
                } else {
                    // Otherwise it is added as a part of guarding condition
                    guarding_cond = simplify(guarding_cond && c);
                }
                debug(4) << "\t" << (safe_promotion ? "promotable " : "unpromotable ")
                         << "conjunction: " << c << "\n";
                continue;
            }
            // Two cases of removable conditions:
            // 1. var_0 == var_1
            //    for (gather_iii, 0, III)
            //      unrolled for (iii, 0, III)
            //        if (gather_iii == iii)
            //          read/write_channel(name, iii)
            //    The channel operation can be promoted out of gather_iii (safe_loop)
            //    since its loop body accesses only a part of the channel array
            // 2. var_0 == 0 (const)
            //    unrolled for (iii, 0, III)
            //      if (iii == 0)
            //        read/write_channel(name, 0)
            //    The channel operation can be promoted out of iii (safe_loop)
            auto lhs = eq->a.as<Variable>();
            auto rhs = eq->b.as<Variable>();
            if ((lhs && rhs)
                || (lhs && is_const(eq->b))
                || (rhs && is_const(eq->a))) {
                if (lhs && !find_loop_in_args(args, lhs->name)) {
                    internal_assert(!removable_conds.count(lhs->name));
                    removable_conds[lhs->name] = c;
                }
                if (rhs && !find_loop_in_args(args, rhs->name)) {
                    internal_assert(!removable_conds.count(rhs->name));
                    removable_conds[rhs->name] = c;
                }
            } else {
                // Other cases are considered as unsafe
                safe_promotion = false;
            }
            debug(4) << "\t" << (safe_promotion ? "removable " : "unremovable ")
                     << "conjunction: " << c << "\n";
        }
        // Find the promotion loop outward
        for (auto it = loop_vars.rbegin(); it != loop_vars.rend(); ++it) {
            // If the loop body modifies one of the values used in condition, the promotion is unsafe
            if (loop_vf.find_writes(cond_vf.get_reads(), *it)) {
                if (ends_with(*it, "buf")) {
                    // Trick: This loop is generated with double buffering, we are sure the promotion is safe
                    promotion_loop = *it;
                }
                break;
            }
            // Promote channels outside the argument loops (always)
            // or the safe loops (for removable condition)
            if (find_loop_in_args(args, *it)) {
                promotion_loop = *it;
            } else {
                auto o = removable_conds.find(*it);
                if (o == removable_conds.end())
                    break;
                promotion_loop = *it;
                removable_conds.erase(o);
            }
        }
        for (auto &kv : removable_conds) {
            guarding_cond = guarding_cond && kv.second;
        }
        return safe_promotion;
    }

    void visit(const Call* op) override {
        if (op->is_intrinsic(Call::write_channel) || op->is_intrinsic(Call::read_channel)) {
            bool is_write_chn = op->is_intrinsic(Call::write_channel) ? true : false;
            auto args = op->args;
            string chn_name = get_channel_name(args[0]);

            bool need_promotion = false;
            string promotion_loop = loop_vars.back() + "__innermost";
            debug(4) << (is_write_chn ? "Write" : "Read")
                     << " channel " << chn_name
                     << " with condition: " << condition << "\n";
            Expr guarding_cond = UIntImm::make(Bool(1), 1);
            bool safe_promotion = check_cond(condition, args, promotion_loop, guarding_cond);
            // Note the write operation can be promoted even if not safe,
            // and the safe promotion can be disabled in some cases
            if (!ends_with(promotion_loop, "__innermost")) {
                need_promotion = true;
            }
            if (!is_write_chn) {
                // Check the producer-consumer relation to determine if the promotion is valid
                auto it = get_promoted_channel(channels, chn_name, true);
                if (it == channels.end()) {
                    // The producer is not promoted, so the consumer cannot be promoted
                    need_promotion = false;
                } else {
                    if (safe_promotion) {
                        // The producer is promoted, and the consumer has simple condition
                        need_promotion = true;
                    } else {
                        // The consumer cannot be promoted, so revoke producer's promotion
                        need_promotion = false;
                        it = channels.erase(it);
                    }
                }
            } else {
                // For write channels, if channel arguments occurs in the condition, we cannot ensure the entire array
                // is write at once (example is CNN-Kung-Song). So we disable promotion for safe.
                VarsFinder vf;
                condition.accept(&vf);
                if (vf.find_var(args)) {
                    need_promotion = false;
                }
            }
            if (need_promotion) {
                auto it = get_promoted_channel(channels, chn_name, is_write_chn);
                if (it != channels.end()) {
                    // We allow multiple read/write to one channel, but they must be promoted at the same loop
                    internal_assert(promotion_loop == it->promotion_loop);
                } else {
                    // Record the promoted channel
                    debug(4) << (safe_promotion ? "Safely" : "Unsafely") << " promote "
                             << (is_write_chn ? "write" : "read") << " channel " << chn_name
                             << " out of the loop " << promotion_loop
                             << " with the guarding cond " << guarding_cond << "\n";
                    PromotedChannel c = { is_write_chn, safe_promotion, chn_name, promotion_loop, args, guarding_cond };
                    channels.push_back(std::move(c));
                }
            }
        }
        IRVisitor::visit(op);
    }
};

class ChannelPromotor : public IRMutator {
  public:
    ChannelPromotor(ChannelVisitor &_cv)
        : channels(_cv.channels) {}

    vector<PromotedChannel> &channels;

  private:
    using IRMutator::visit;

    // Add suffix .array to promoted channels
    Stmt visit(const Realize* op) override {
        Stmt body = mutate(op->body);
        string name = op->name;
        for (auto &c : channels) {
            if (c.name == op->name) {
                name += ".array";
                break;
            }
        }
        return Realize::make(name, op->types, op->memory_type, op->bounds, op->condition, body);
    }

    Stmt make_channel_access(PromotedChannel &c, Stmt body) {
        if (c.is_write_chn) {
            /* Transform the IR like this:
             * realize c.name.temp[0,1] of type bool
             * c.name.temp = 0
             * for (name, min, extent) {...}
             * if (c.name.temp == 1)
             *   write_channel(c.name, read_array(chn_array))
             */
            string chn_array = c.name + ".array";
            Expr read_array = Call::make(Handle(), "read_array", { chn_array }, Call::PureIntrinsic);
            Expr write = Call::make(Handle(), "write_channel", { c.name, read_array }, Call::PureIntrinsic);
            Stmt write_stmt = Evaluate::make(write);
            if (!c.safe_promotion) {
                Expr get_flag = Call::make(Bool(), c.name+".temp", {}, Call::Intrinsic);
                write_stmt = IfThenElse::make(get_flag == 1, write_stmt);
            } else if (!is_one(c.guarding_cond)) {
                write_stmt = IfThenElse::make(c.guarding_cond, write_stmt);
            }
            body = Block::make(body, write_stmt);
            if (!c.safe_promotion) {
                Stmt init_flag = Provide::make(c.name+".temp", {0}, {});
                body = Block::make(init_flag, body);
                body = Realize::make(c.name+".temp", { Bool() }, MemoryType::Auto, {}, const_true(), body);
            }
        } else {
            /* Transform the IR like this:
             * if (guarding_cond)
             *   write_array(chn_array, read_channel(c.name))
             * for (name, min, extent) {...}
             */
            string chn_array = c.name + ".array";
            Expr read = Call::make(Handle(), "read_channel", { c.name }, Call::PureIntrinsic);
            Expr write_array = Call::make(Handle(), "write_array", { chn_array, read }, Call::PureIntrinsic);
            Stmt read_stmt = Evaluate::make(write_array);
            if (!is_one(c.guarding_cond)) {
                read_stmt = IfThenElse::make(c.guarding_cond, read_stmt);
            }
            body = Block::make(read_stmt, body);
        }
        return body;
    }

    Stmt visit(const For* op) override {
        Stmt body = mutate(op->body);
        Stmt loop = For::make(op->name, op->min, op->extent, op->for_type, op->device_api, body);
        for (auto &c : channels) {
            if (c.promotion_loop == op->name) {
                loop = make_channel_access(c, loop);
            }
            if (ends_with(c.promotion_loop, "__innermost")) {
                string loop_name = remove_postfix(c.promotion_loop, "__innermost");
                if (loop_name == op->name) {
                    body = make_channel_access(c, body);
                    loop = For::make(op->name, op->min, op->extent, op->for_type, op->device_api, body);
                }
            }
        }
        return loop;
    }

    Expr visit(const Select* op) override {
        auto call = op->true_value.as<Call>();
        if (call && (call->is_intrinsic(Call::write_channel) || call->is_intrinsic(Call::read_channel))) {
            bool is_write_chn = call->is_intrinsic(Call::write_channel) ? true : false;
            string chn_name = get_channel_name(call->args[0]);
            auto it = get_promoted_channel(channels, chn_name, is_write_chn);
            if (it != channels.end() && can_prove(op->condition == it->guarding_cond)) {
                // Remove redundant condition on read/write array as the condition is moved on read/write channel
                return mutate(op->true_value);
            }
        }
        return IRMutator::visit(op);
    }

    Stmt visit(const IfThenElse* op) override {
        auto eval = op->then_case.as<Evaluate>();
        if (!eval) {
            return IRMutator::visit(op);
        }
        auto call = eval->value.as<Call>();
        if (call && (call->is_intrinsic(Call::write_channel) || call->is_intrinsic(Call::read_channel))) {
            bool is_write_chn = call->is_intrinsic(Call::write_channel) ? true : false;
            string chn_name = get_channel_name(call->args[0]);
            auto it = get_promoted_channel(channels, chn_name, is_write_chn);
            if (it != channels.end() && can_prove(op->condition == it->guarding_cond)) {
                // Remove redundant condition on read/write array as the condition is moved on read/write channel
                return mutate(op->then_case);
            }
        }
        return IRMutator::visit(op);
    }

    Expr visit(const Call* op) override {
        if (op->is_intrinsic(Call::write_channel) || op->is_intrinsic(Call::read_channel)) {
            bool is_write_chn = op->is_intrinsic(Call::write_channel) ? true : false;
            string chn_name = get_channel_name(op->args[0]);
            vector<Expr> args;
            for (size_t i = 0; i < op->args.size(); i++) {
                args.push_back(mutate(op->args[i]));
            }

            auto it = get_promoted_channel(channels, chn_name, is_write_chn);
            // Replace read/write_channel with read/write_array
            // The channel operation is recreated when visiting For nodes
            if (it != channels.end()) {
                string call_name = is_write_chn ? "write_array" : "read_array";
                args[0] = StringImm::make(chn_name + ".array");
                return Call::make(op->type, call_name, args, Call::PureIntrinsic);
            }
            return Call::make(op->type, op->name, args, op->call_type);
        }
        if (op->is_intrinsic(Call::write_array) || op->is_intrinsic(Call::read_array)) {
            string chn_name = get_channel_name(op->args[0]);
            if (ends_with(chn_name, "channel")) {
                // Trick: A read/write_array call whose name is the channel name may be inserted beforehand,
                // so we update the name here. We know it only happens at write_channel of data relaying.
                auto it = get_promoted_channel(channels, chn_name, true);
                internal_assert(it != channels.end());
                vector<Expr> args;
                for (size_t i = 0; i < op->args.size(); i++) {
                    args.push_back(mutate(op->args[i]));
                }
                args[0] = StringImm::make(chn_name + ".array");
                return Call::make(op->type, op->name, args, Call::PureIntrinsic);
            }
        }
        return IRMutator::visit(op);
    }

    Stmt visit(const Evaluate *op) override {
        Expr value = mutate(op->value);
        Stmt ret = Evaluate::make(value);

        auto call = value.as<Call>();
        if (call && call->is_intrinsic(Call::write_array)) {
            string chn_name = get_channel_name(call->args[0].as<StringImm>()->value);
            auto it = get_promoted_channel(channels, chn_name, true);

            if (!it->safe_promotion) {
                string name = chn_name + ".temp";
                Stmt set_flag = Provide::make(name, {1}, {});
                ret = Block::make(ret, set_flag);
            }
        }
        return ret;
    }
};

Stmt channel_promotion(Stmt s) {
    VarsFinder vf;
    ChannelVisitor cv(vf);
    ChannelPromotor cp(cv);
    s = remove_lets(s, true, false, false, false, {});
    s.accept(&vf);
    s.accept(&cv);
    s = cp.mutate(s);
    return s;
}

}
}
