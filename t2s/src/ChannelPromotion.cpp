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

class ChannelPromotor : public IRMutator {
  public:
    ChannelPromotor() {}
    struct Channel {
        bool is_write_chn;
        bool is_simple_cond;
        string name;
        string promotion_loop;
    };
    vector<Channel> channels;
    vector<string> unrolled_loops;
    vector<string> loop_vars;
    Expr condition = const_true();

  private:
    using IRMutator::visit;

    std::string get_channel_name(Expr e) {
        auto v = e.as<StringImm>();
        internal_assert(v);

        int size = (v->value).rfind(".");
        std::string channel_name = v->value;
        debug(4) << "channel name: " << channel_name << "\n";
        if (size < (int)(v->value.size())-1) {
            std::string suffix = (v->value).substr(size + 1, (int)v->value.size() - size - 1);
            // eliminate useless suffix
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
        if (op->for_type == ForType::Unrolled)
            unrolled_loops.push_back(op->name);
        loop_vars.push_back(op->name);

        Stmt body = mutate(op->body);
        for (auto &c : channels) {
            if (op->name == c.promotion_loop) {
                string chn_array = c.name + ".array";
                if (c.is_write_chn) {
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
                    Expr read = Call::make(Handle(), "read_channel", { c.name }, Call::PureIntrinsic);
                    Expr write_array = Call::make(Handle(), "write_array", { chn_array, read }, Call::PureIntrinsic);
                    Stmt read_stmt = Evaluate::make(write_array);
                    body = Block::make(read_stmt, body);
                }
            }
        }
        body = For::make(op->name, op->min, op->extent, op->for_type, op->device_api, body);
        if (op->for_type == ForType::Unrolled)
            unrolled_loops.pop_back();
        loop_vars.pop_back();
        return body;
    }

    Stmt visit(const IfThenElse* op) override {
        // in a loop body
        if (loop_vars.size() > 0) {
            Expr temp_cond = condition;
            condition = simplify(temp_cond && (op->condition));
            Stmt then_case = mutate(op->then_case);

            Stmt else_case = op->else_case;
            if (else_case.defined()) {
                condition = simplify(temp_cond && !(op->condition));
                else_case = mutate(op->else_case);
            }
            condition = temp_cond;
            return IfThenElse::make(op->condition, then_case, else_case);
        }
        return IRMutator::visit(op);
    }

    bool find_loop_in_args(vector<Expr> args, string loop) {
        for (auto v : args) {
            if (v.as<Variable>() && v.as<Variable>()->name == loop)
                return true;
        }
        return false;
    }

    bool check_cond(Expr condition, vector<Expr> args, string &promotion_loop) {
        // collect occured loop variables in the condition
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
        // check loops and find the promotion loop
        for (auto it = loop_vars.rbegin(); it != loop_vars.rend(); ++it) {
            promotion_loop = *it;
            if (!find_loop_in_args(args, *it)) {
                auto o = occured_loops.find(*it);
                if (o != occured_loops.end())
                    occured_loops.erase(o);
                else break;
            }
        }
        return is_simple_cond && occured_loops.empty();
    }

    Expr visit(const Call* op) override {
        if (op->is_intrinsic(Call::write_channel) || op->is_intrinsic(Call::read_channel)) {
            bool is_write_chn = op->is_intrinsic(Call::write_channel) ? true : false;
            auto args = op->args;
            string chn_name = get_channel_name(args[0]);

            bool need_promotion = false;
            string promotion_loop = loop_vars.back();
            bool is_simple_cond = check_cond(condition, args, promotion_loop);
            // The channel arguments contain unrolled loops and needs to be promoted
            if (promotion_loop != loop_vars.back())
                need_promotion = true;
            if (!is_write_chn) {
                bool found = false;
                for (auto &c : channels) {
                    if (chn_name == c.name && c.is_write_chn) {
                        // The producer is promoted, so the consumer needs to be promoted
                        // However, only the one guarded by simple condition can be promoted, so we check it here
                        internal_assert(is_simple_cond);
                        need_promotion = true;
                        found = true;
                    }
                }
                // The producer is not promoted, so the consumer cannot be promoted
                if (!found)
                    need_promotion = false;
            }
            if (need_promotion) {
                bool found = false;
                for (auto &c : channels) {
                    if (chn_name == c.name && is_write_chn == c.is_write_chn) {
                        internal_assert(promotion_loop == c.promotion_loop);
                        found = true;
                    }
                }
                if (!found) {
                    Channel c = { is_write_chn, is_simple_cond, chn_name, promotion_loop };
                    channels.push_back(std::move(c));
                }
                args[0] = StringImm::make(chn_name + ".array");
                if (is_write_chn)
                    return Call::make(op->type, "write_array", args, Call::PureIntrinsic);
                else
                    return Call::make(op->type, "read_array", args, Call::PureIntrinsic);
            }
        }
        return IRMutator::visit(op);
    }

    Stmt visit(const Evaluate *op) override {
        Expr value = mutate(op->value);
        Stmt ret = Evaluate::make(value);

        auto call = value.as<Call>();
        if (call && call->is_intrinsic(Call::write_array)) {
            auto &chn = channels.back();
            internal_assert(call->args[0].as<StringImm>()->value == chn.name + ".array");
            if (!chn.is_simple_cond) {
                string name = chn.name + ".temp";
                Stmt set_flag = Provide::make(name, {1}, {});
                ret = Block::make(ret, set_flag);
            }
        }
        return ret;
    }
};


Stmt channel_promotion(Stmt s) {
    ChannelPromotor mutator;
    return simplify(mutator.mutate(s));
}

}
}
