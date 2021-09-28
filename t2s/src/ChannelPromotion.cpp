#include "IR.h"
#include "IRMutator.h"
#include "IROperator.h"
#include "Simplify.h"
#include "Substitute.h"
#include "./DebugPrint.h"
#include "./LoopRemoval.h"
#include "Utilities.h"

namespace Halide {
namespace Internal {

#if 0
class ChannelPromotor : public IRMutator {
  public:
    ChannelPromotor() {}

  private:
    using IRMutator::visit;

    bool inside_unroll_loop{false};
    std::string loop_name;
    Expr loop_min;
    Expr loop_extent;
    Expr channel_cond;
    Expr channel_write;
    Expr channel_read;
    bool promote_channel{false};
    bool insert_head{false};

    Stmt visit(const For* op) override {
        // we can only handle one-level loop promotion
        promote_channel = false;
        inside_unroll_loop = false;
        if (op->for_type == ForType::Unrolled) {
            loop_name = op->name;
            loop_extent = op->extent;
            loop_min = op->min;
            inside_unroll_loop = true;
            insert_head = false;
            channel_cond = const_true();
            channel_write = Expr();
            channel_read = Expr();
            Stmt body = mutate(op->body);
            inside_unroll_loop = false;
            if (promote_channel) {
                if (channel_read.defined()) {
                    const Call* c = channel_read.as<Call>();
                    Stmt assign = Provide::make("pch.temp", {channel_read}, {});
                    Stmt ite = IfThenElse::make(channel_cond, assign);
                    body = For::make(op->name, op->min, op->extent, op->for_type,
                                     op->device_api, body);
                    if (insert_head) body = Block::make(ite, body);
                    else             body = Block::make(body, ite);
                    return Realize::make("pch.temp", {c->type}, MemoryType::Auto,
                                         {}, const_true(), body);
                } else if (channel_write.defined()) {
                    Stmt ite = IfThenElse::make(channel_cond, Evaluate::make(channel_write));
                    body = For::make(op->name, op->min, op->extent, op->for_type,
                                     op->device_api, body);
                    if (insert_head) body = Block::make(ite, body);
                    else             body = Block::make(body, ite);
                    return body;
                }
            }
            return For::make(op->name, op->min, op->extent, op->for_type, op->device_api, body);
        }
        return IRMutator::visit(op);
    }

    Stmt visit(const IfThenElse* op) override {
        if (inside_unroll_loop
                && !channel_read.defined()
                && !channel_write.defined()) {
            Expr cond_min = simplify(substitute(loop_name, loop_min, op->condition));
            Expr cond_ext = simplify(substitute(loop_name, loop_min+loop_extent-1, op->condition));
            if (is_one(cond_min) && op->condition.as<EQ>()) {
                promote_channel = true;
                insert_head = true;
            } else if (is_one(cond_ext) && op->condition.as<EQ>()) {
                promote_channel = true;
                insert_head = false;
            } else {
                channel_cond = channel_cond && op->condition;
            }
            Stmt then_case = mutate(op->then_case);
            if (!channel_read.defined() && !channel_write.defined()) {
                promote_channel = false;
                channel_cond = const_true();
            }
            return IfThenElse::make(op->condition, then_case, op->else_case);
        } else {
            return IRMutator::visit(op);
        }
    }

    Expr visit(const Call* op) override {
        if (promote_channel
                && !channel_read.defined()
                && !channel_write.defined()) {
            if (op->is_intrinsic(Call::write_channel)) {
                channel_write = op;
                return 0;
            } else if (op->is_intrinsic(Call::read_channel)) {
                channel_read = op;
                return Call::make(op->type, "pch.temp", {}, Call::Intrinsic); 
            } else {
                std::vector<Expr> args;
                for (size_t i = 0; i < op->args.size(); i++) {
                    args.push_back(mutate(op->args[i]));
                }
                return Call::make(op->type, op->name, args, op->call_type);
            }
        } else {
            return IRMutator::visit(op);
        }
    }
};

#endif

struct PromotedChannel {
    bool is_write_chn;          // True if it is a write_channel intrinsic
    bool is_simple_cond;        // True if it is guarded by simple condition that allows promotion
    string name;                // Channel name after eliminating unnecessary suffix
    string promotion_loop;      // Where the loop should be promoted
    vector<Expr> args;          // The read/write_channel arguments
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
            if (suffix != "channel")
                channel_name = v->value;
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

class ChannelVisitor : public IRVisitor {
  public:
    ChannelVisitor() {}

    vector<PromotedChannel> channels;
    vector<string> unrolled_loops;
    vector<string> loop_vars;
    Expr condition = const_true();

  private:
    using IRVisitor::visit;

    void visit(const For* op) override {
        if (op->for_type == ForType::Unrolled)
            unrolled_loops.push_back(op->name);
        loop_vars.push_back(op->name);

        op->body.accept(this);

        if (op->for_type == ForType::Unrolled)
            unrolled_loops.pop_back();
        loop_vars.pop_back();
    }

    void visit(const IfThenElse* op) override {
        // Pass some conditions out of loop nests
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

    bool find_loop_in_args(vector<Expr> args, string loop) {
        for (auto v : args) {
            if (v.as<Variable>() && v.as<Variable>()->name == loop)
                return true;
        }
        return false;
    }

    bool check_cond(Expr condition, vector<Expr> args, string &promotion_loop) {
        // Collect occured loop variables in the condition
        vector<Expr> conjunction = break_logic_into_conjunction(condition);
        std::set<string> occured_loops;
        bool is_simple_cond = true;
        for (auto &c : conjunction) {
            auto eq = c.as<EQ>();
            if (!eq) {
                if (!is_one(c))
                    is_simple_cond = false;
                continue;
            }
            // Two cases of simple condition:
            // 1. var_0 == var_1
            //    for (gather_iii, 0, III)
            //      unrolled for (iii, 0, III)
            //        if (gather_iii == iii)
            //          read/write_channel(name, iii)
            //    The channel operation can be promoted out of gather_iii (occured_loop)
            //    since its loop body only access a part of channel array
            // 2. var_0 == 0 (const)
            //    unrolled for (iii, 0, III)
            //      read/write_channel(name, 0)
            //    The channel operation can be promoted out of iii (boarder)
            auto lhs = eq->a.as<Variable>();
            auto rhs = eq->b.as<Variable>();
            if (lhs && rhs) {
                if (!find_loop_in_args(args, lhs->name))
                    occured_loops.insert(lhs->name);
                if (!find_loop_in_args(args, rhs->name))
                    occured_loops.insert(rhs->name);
            } else if (lhs && is_const(eq->b)) {
                occured_loops.insert(lhs->name);
            } else if (rhs && is_const(eq->a)) {
                occured_loops.insert(rhs->name);
            } else {
                is_simple_cond = false;
            }
        }
        // Find the promotion loop outward
        for (auto it = loop_vars.rbegin(); it != loop_vars.rend(); ++it) {
            promotion_loop = *it;
            // Promote channels outside the argument loops (always)
            // or the occured loops (if simple condition)
            if (!find_loop_in_args(args, *it)) {
                auto o = occured_loops.find(*it);
                if (o != occured_loops.end())
                    occured_loops.erase(o);
                else break;
            }
        }
        // If occured_loop is not empty, there exists condition like this:
        // for (gather_iii, 0, III)
        //   for (dummy, 0, D)
        //     for (iii, 0, III)
        //       if (gather_iii == iii) ...
        // In such case, we cannot guarantee the correctness of channel promotion
        return is_simple_cond && occured_loops.empty();
    }

    void visit(const Call* op) override {
        if (op->is_intrinsic(Call::write_channel) || op->is_intrinsic(Call::read_channel)) {
            bool is_write_chn = op->is_intrinsic(Call::write_channel) ? true : false;
            auto args = op->args;
            string chn_name = get_channel_name(args[0]);

            bool need_promotion = false;
            string promotion_loop = loop_vars.back();
            bool is_simple_cond = check_cond(condition, args, promotion_loop);
            if (promotion_loop != loop_vars.back())
                need_promotion = true;

            if (!is_write_chn) {
                // Check the producer-consumer relation to determine if the promotion is valid
                auto it = get_promoted_channel(channels, chn_name, true);
                if (it == channels.end()) {
                    // The producer is not promoted, so the consumer cannot be promoted
                    need_promotion = false;
                } else {
                    if (is_simple_cond) {
                        // The producer is promoted, and the consumer has simple condition
                        need_promotion = true;
                    } else {
                        // The consumer cannot be promoted, so revoke producer's promotion
                        need_promotion = false;
                        it = channels.erase(it);
                    }
                }
            }
            if (need_promotion) {
                auto it = get_promoted_channel(channels, chn_name, is_write_chn);
                if (it != channels.end()) {
                    // We allow multiple read/write to one channel
                    // But they must be promoted at the same loop
                    internal_assert(promotion_loop == it->promotion_loop);
                } else {
                    // Record the promoted channel
                    PromotedChannel c = { is_write_chn, is_simple_cond, chn_name, promotion_loop, args };
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

    Stmt visit(const For* op) override {
        Stmt body = mutate(op->body);
        for (auto &c : channels) {
            if (op->name == c.promotion_loop) {
                string chn_array = c.name + ".array";
                if (c.is_write_chn) {
                    // Read/write the entire array as a whole
                    // realize bool c.name.temp
                    // c.name.temp = 0
                    // for (name, min, extent) {...}
                    // if (c.name.temp == 1)
                    //  write_channel(c.name, read_array(chn_array))
                    Expr read_array = Call::make(Handle(), "read_array", { chn_array }, Call::PureIntrinsic);
                    Expr write = Call::make(Handle(), "write_channel", { c.name, read_array }, Call::PureIntrinsic);
                    Stmt write_stmt = Evaluate::make(write);
                    if (!c.is_simple_cond) {
                        Expr get_flag = Call::make(Bool(), c.name+".temp", {}, Call::Intrinsic);
                        write_stmt = IfThenElse::make(get_flag == 1, write_stmt);
                    }
                    body = Block::make(body, write_stmt);
                    if (!c.is_simple_cond) {
                        Stmt init_flag = Provide::make(c.name+".temp", {0}, {});
                        body = Block::make(init_flag, body);
                        body = Realize::make(c.name+".temp", { Bool() }, MemoryType::Auto, {}, const_true(), body);
                    }
                } else {
                    // Read the entire array as a whole
                    // write_array(chn_array, read_channel(c.name))
                    // for (name, min, extent) {...}
                    Expr read = Call::make(Handle(), "read_channel", { c.name }, Call::PureIntrinsic);
                    Expr write_array = Call::make(Handle(), "write_array", { chn_array, read }, Call::PureIntrinsic);
                    Stmt read_stmt = Evaluate::make(write_array);
                    body = Block::make(read_stmt, body);
                }
            }
        }
        body = For::make(op->name, op->min, op->extent, op->for_type, op->device_api, body);
        return body;
    }

    Expr visit(const Call* op) override {
        if (op->is_intrinsic(Call::write_channel) || op->is_intrinsic(Call::read_channel)) {
            bool is_write_chn = op->is_intrinsic(Call::write_channel) ? true : false;
            string chn_name = get_channel_name(op->args[0]);
            vector<Expr> args;
            for (size_t i = 0; i < op->args.size(); i++)
                args.push_back(mutate(op->args[i]));

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
        return IRMutator::visit(op);
    }

    Stmt visit(const Evaluate *op) override {
        Expr value = mutate(op->value);
        Stmt ret = Evaluate::make(value);

        auto call = value.as<Call>();
        if (call && call->is_intrinsic(Call::write_array)) {
            string chn_name = get_channel_name(call->args[0].as<StringImm>()->value);
            auto it = get_promoted_channel(channels, chn_name, true);

            if (!it->is_simple_cond) {
                string name = chn_name + ".temp";
                Stmt set_flag = Provide::make(name, {1}, {});
                ret = Block::make(ret, set_flag);
            }
        }
        return ret;
    }
};

Stmt channel_promotion(Stmt s) {
    ChannelVisitor cv;
    ChannelPromotor cp(cv);
    s.accept(&cv);
    s = cp.mutate(s);
    return s;
}

}
}
