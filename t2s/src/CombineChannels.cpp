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
#include "./CombineChannels.h"
#include "./DebugPrint.h"
#include "./StructType.h"
#include "./Util.h"
#include "../../Halide/src/IRMutator.h"
#include "../../Halide/src/IREquality.h"
#include "../../Halide/src/Substitute.h"

// Original IR:
//  Realize cha[I][J][Depth], chb[I][J][Depth] // The channels have the same dimensions (including depths)
//    Produce cha, chb
//      if (c)                                 // Same path condition
//          write_channel(cha[i][j], x)        // Same channel indices ([i][j]) in writing
//          write_channel(chb[i][j], y)
//    Consume cha, chb
//      if (c)
//          x = read_channel(cha[i][j])        // Channel indices in reading is the same as that in writing
//          y = read_channel(chb[i][j])
//
// After combination:
//  Realize chab[I][J][Depth]
//    Produce chab
//     Realize t1, t2                          // Insert this at the loop level enclosing the original writes
//      if (c)
//          Provide t1 = x
//          Provide t2 = y
//          write_channel(chab[i][j], {t1, t2})
//    Consume chab
//     Realize t                              // Insert this  at the loop level enclosing the original reads
//      if (c) t = read_channel(chab[i][j])
//      if (c)
//          x = read_field(t, 0)
//          y = read_field(t, 1)
// Assumption:
//      Eevery channel is read under the same condition as it is written. We do not check this assumption.

namespace Halide {

using std::string;
using std::vector;
using std::tuple;
using std::pair;
using std::map;

using namespace Internal;

namespace Internal {
namespace {
struct ChannelAccess {
    bool           is_write;  // channel write/read
    string         channel;   // channel name
    string         func;      // the func immediately enclosing this access
    Type           type;      // data type
    Expr           condition; // the path condition when accessing the channel.
    vector<Expr>   args;      // args used when accessing the channel
    vector<string> loops;     // loops around the channel access
};

struct CombinedChannel {
    string                combined_channel;
    vector<ChannelAccess> original_channel_writes;
    string                producer;            // The func that writes the original channels
    string                consumer;            // The func that reads the original channels

    // We will insert something for writing and reading a combined channel as illlustrated above.
    Expr                 read_condition;       // c as illustrated above
    string               temporary_for_read;   // t as illustrated above
    vector<string>       temporary_for_writes; // t1, t2 as illustrated above
    Type                 struct_type;          // type for the combined channel
    vector<Expr>         write_args;           // args used when writing the combined channel from the producer Func
    vector<Expr>         read_args;            // args used when reading the combined channel from the consumer Func
    string               enclosing_write_loop; // the innermost loop enclosing the combined channel from the producer Func
    string               enclosing_read_loop;  // the innermost loop enclosing the combined channel from the consumer Func
    string               outermost_write_loop; // the outermost loop enclosing the combined channel from the producer Func
    string               outermost_read_loop;  // the outermost loop enclosing the combined channel from the consumer Func
    Region               bounds;               // bounds of the combined channel
};

/*
class HasLoopInside : public IRVisitor {
    using IRVisitor::visit;
public:
    bool loop_inside = false;

private:
    void visit(const For* op) override {
        loop_inside = true;
        return;
    }
};
*/

class GatherChannelAccess : public IRVisitor {
    using IRVisitor::visit;

public:
    vector<ChannelAccess> channel_accesses;  // Accesses in their lexical order in the IR
    map<string, Region>   channel_to_bounds;

    GatherChannelAccess() { condition = const_true();
                            in_function = false;
                          }

private:
    Expr   condition;         // The path condition to the current IR node
    bool   in_function;       // The current IR is in a function
    string func;              // Current func
    vector<string> loops;     // Loops around the current IR node

    void visit(const Call* op) override {
        // TOFIX: Currently do not handle non-blocking channel reads
        if (op->is_intrinsic(Call::read_channel) || op->is_intrinsic(Call::write_channel)) {
            const StringImm *v = op->args[0].as<StringImm>();
            string channel = v->value;
            vector<Expr> args;
            if (op->is_intrinsic(Call::read_channel)) {
                vector<Expr> args1(op->args.begin() + 1, op->args.end());
                args = std::move(args1);
            } else {
                vector<Expr> args1(op->args.begin() + 2, op->args.end());
                args = std::move(args1);
            }
            ChannelAccess access = {op->is_intrinsic(Call::write_channel),
                                    channel, func, op->type, condition, std::move(args), loops};
            channel_accesses.push_back(std::move(access));
        }
        IRVisitor::visit(op);
    }

    void visit(const ProducerConsumer *op) override {
        bool old_in_function = in_function;
        if (op->is_producer) {
            func = op->name;
            in_function = true;
        }
        IRVisitor::visit(op);
        in_function = old_in_function;
    }

    void visit(const For *op) override {
        // Skip the dummy loops that only tell the compiler this is a device function
        bool dummy_loop = ends_with(op->name, ".run_on_device");
        if (!dummy_loop) {
            loops.push_back(op->name);
        }
        op->body.accept(this);
        if (!dummy_loop) {
            loops.pop_back();
        }
    }

    void visit(const Realize *op) override {
        if (ends_with(op->name, ".channel")) {
            channel_to_bounds[op->name] = op->bounds;
        }
        IRVisitor::visit(op);
    }

    void visit(const IfThenElse *op) override {
        if (!in_function) {
            // We do not gather condition outside of a function.
            IRVisitor::visit(op);
            return;
        }
        Expr old_condition = condition;

        condition = equal(old_condition, const_true()) ? op->condition : old_condition && op->condition;
        op->then_case.accept(this);

        if (op->else_case.defined()) {
            condition = equal(old_condition, const_true()) ? !op->condition : old_condition && !op->condition;
            op->else_case.accept(this);
        }

        condition = old_condition;
    }

    void visit(const Select *op) override {
        if (!in_function) {
            // We do not gather condition outside of a function.
            IRVisitor::visit(op);
            return;
        }
        Expr old_condition = condition;

        condition = equal(old_condition, const_true()) ? op->condition : old_condition && op->condition;
        op->true_value.accept(this);

        if (op->false_value.defined()) {
            condition = equal(old_condition, const_true()) ? !op->condition : old_condition && !op->condition;
            op->false_value.accept(this);
        }

        condition = old_condition;
    }
};

bool same_args(const vector<Expr> &args1, const vector<Expr> &args2) {
    if (args1.size() != args2.size()) {
        return false;
    }
    for (size_t i = 0; i < args1.size(); i++) {
        if (is_const(args1[i]) && is_const(args2[i])) {
            if (equal(args1[i], args2[i])) {
                continue;
            } else {
                return false;
            }
        }
        const Variable *arg1 = args1[i].as<Variable>();
        const Variable *arg2 = args2[i].as<Variable>();
        if (!arg1 || !arg2) {
            return false;
        }
        // Strip away prefixes, and leave only the loop names.
        vector<string> tokens1 = split_string(arg1->name, ".");
        vector<string> tokens2 = split_string(arg2->name, ".");
        string loop1 = tokens1.back();
        string loop2 = tokens2.back();
        if (loop1 != loop2) {
            return false;
        }
    }
    return true;
}

// Check if two channels' bounds are completely the same, except their last dimensions, which
// are their depths.
bool same_channel_bounds(const std::vector<Range> &bounds1, const std::vector<Range> &bounds2) {
    if (bounds1.size() != bounds2.size()) {
        return false;
    }
    for (size_t i = 0; i < bounds1.size() - 1; i++) {
        if (!equal(bounds1[i].min, bounds2[i].min)) {
            return false;
        }
        if (!equal(bounds1[i].extent, bounds2[i].extent)) {
            return false;
        }
    }
    return true;
}

// Assumption: the given two channels' bounds are completely the same, except their last dimensions,
// which are their depths. Return a merged bounds, which is the same as the original two bounds,
// but with the widest of the two bounds' last dimensions as its last dimension.
std::vector<Range> merge_channel_bounds(const std::vector<Range> &bounds1, const std::vector<Range> &bounds2) {
    internal_assert(same_channel_bounds(bounds1, bounds2));

    std::vector<Range> merged_bounds = bounds1;
    size_t i = bounds1.size() - 1;
    Range r(min(bounds1[i].min, bounds2[i].min), max(bounds1[i].extent, bounds2[i].extent));
    merged_bounds[i] = r;
    return std::move(merged_bounds);
}

Expr replace_prefix(const Expr &condition, const vector<string> &producer_loops,
                    const vector<string> &consumer_loops) {
    Expr cond = condition;
    for (auto p : producer_loops) {
        vector<string> p_tokens = split_string(p, ".");
        string p_loop = p_tokens.back();
        for (auto c : consumer_loops) {
            vector<string> c_tokens = split_string(c, ".");
            string c_loop = c_tokens.back();
            if (p_loop == c_loop) {
                cond = substitute(Var(p), Var(c), cond);
                break;
            }
        }
    }
    return cond;
}

void decide_channels_to_combine(const vector<ChannelAccess>  &channel_accesses,
                                const map<string, Region>    &channel_to_bounds,
                                map<string, CombinedChannel> &channel_to_combined) {
    for (size_t i = 0; i < channel_accesses.size(); i++) {
        const ChannelAccess &c1 = channel_accesses[i];
        if (channel_to_combined.find(c1.channel) != channel_to_combined.end()) {
            // this channel is already combined before
            continue;
        }
        CombinedChannel combined;
        vector<Type> field_types;
        for (auto j = i + 1; j < channel_accesses.size(); j++) {
            const ChannelAccess &c2 = channel_accesses[j];
            if (channel_to_combined.find(c2.channel) != channel_to_combined.end()) {
                // this channel is already combined before
                continue;
            }
            // Look at writes in the same func under same path condition in same args for combining
            if (!c1.is_write || !c2.is_write) {
                continue;
            }
            if (c1.func != c2.func) {
                continue;
            }
            // Below we require that both channel writes must be combined
            // into a single channel. This simplifies later phases such as buffering and
            // scattering, because they need care only 1 channel.
            if (!equal(c1.condition, c2.condition)) {
                user_warning << "Failed to combine channels " << c1.channel << " and " << c2.channel
                        << " in function " << c1.func << ": Path conditions differ\n"
                        << "\t" << c1.condition << " vs. " << c2.condition << "\n";
                continue;
            }
            if (!same_args(c1.args, c2.args)) {
                user_warning << "Failed to combine channels " << c1.channel << " and " << c2.channel
                        << " in function " << c1.func << ": Arguments differ\n"
                        << "\t" << to_string(c1.args) << " vs. " << to_string(c2.args) << "\n";
                continue;
            }
            if (!same_channel_bounds(channel_to_bounds.at(c1.channel), channel_to_bounds.at(c2.channel))) {
                user_warning << "Failed to combine channels " << c1.channel << " and " << c2.channel
                        << " in function " << c1.func << ": Bounds differ\n"
                        << "\t" << to_string(channel_to_bounds.at(c1.channel))
                        << " vs. " << to_string(channel_to_bounds.at(c2.channel)) << "\n";
                continue;
            }
            // Test the two channels are read in the same consumer func with the same args
            string consumer1, consumer2;
            vector<string> consumer1_loops, consumer2_loops;
            vector<Expr>   consumer1_args, consumer2_args;
            Expr           consumer1_read_condition, consumer2_read_condition;
            for (auto acc : channel_accesses) {
                if (!acc.is_write) {
                    if (acc.channel == c1.channel) {
                        internal_assert(consumer1.empty()); // Otherwise, this channel is read at two different lexical positions in the IR, which is forbidden
                        consumer1 = acc.func;
                        consumer1_loops = acc.loops;
                        consumer1_args  = acc.args;
                        consumer1_read_condition = acc.condition;
                    } else if (acc.channel == c2.channel) {
                        internal_assert(consumer2.empty()); // Otherwise, this channel is read at two different lexical positions in the IR, which is forbidden
                        consumer2 = acc.func;
                        consumer2_loops = acc.loops;
                        consumer2_args  = acc.args;
                        consumer2_read_condition = acc.condition;
                    }
                }
            }
            internal_assert(!consumer1.empty()); // There must be a place to read the channel 1
            internal_assert(!consumer2.empty());  // There must be a place to read the channel 2
            if (consumer1 != consumer2) {
                user_warning << "Failed to combine channel " << c1.channel << " with channel " << c2.channel
                        << " in function " << c1.func << ": they are read in different funcs: " << consumer1
                        << " and " << consumer2 << "\n";
                continue;
            }
            if (!same_args(consumer1_args, consumer2_args)) {
                user_warning << "Failed to combine channel " << c1.channel << " with channel " << c2.channel
                        << " in function " << c1.func << ": they are read in different args: ("
                        << to_string<Expr>(consumer1_args) << ") and (" << to_string<Expr>(consumer2_args) << ")\n";
                continue;
            }
            if (consumer1_loops.size() != consumer2_loops.size()) {
                user_warning << "Failed to combine channel " << c1.channel << " with channel " << c2.channel
                        << " in function " << c1.func << ": they are read in different loops: ("
                        << to_string<string>(consumer1_loops) << ") and (" << to_string<string>(consumer2_loops) << "\n";
                continue;
            }
            if ((consumer1_loops.size() > 0) && (consumer1_loops.back() != consumer2_loops.back())) {
                user_warning << "Failed to combine channel " << c1.channel << " with channel " << c2.channel
                        << " in function " << c1.func << ": they are read in different loops: ("
                        << to_string<string>(consumer1_loops) << ") and (" << to_string<string>(consumer2_loops) << "\n";
                continue;
            }
            if (!equal(consumer1_read_condition, consumer2_read_condition)) {
                user_warning << "Failed to combine channel " << c1.channel << " with channel " << c2.channel
                        << " in function " << c1.func << ": they are read under different conditions: ("
                        << to_string(consumer1_read_condition) << ") and (" << to_string(consumer2_read_condition) << "\n";
                continue;
            }
            if (combined.original_channel_writes.empty()) {
                combined.combined_channel = unique_name(c1.func) + ".channel";
                combined.original_channel_writes.push_back(c1);
                combined.producer = c1.func;
                combined.consumer = consumer1;
                combined.read_condition = consumer1_read_condition;
                combined.temporary_for_read = unique_name("t") + ".temp";
                combined.temporary_for_writes.push_back(unique_name("t") + ".temp");
                combined.write_args = c1.args;
                combined.read_args = consumer1_args;
                combined.enclosing_write_loop = c1.loops.back();
                combined.enclosing_read_loop = consumer1_loops.back();
                combined.outermost_write_loop = c1.loops[0];
                combined.outermost_read_loop = consumer1_loops[0];
                combined.bounds = channel_to_bounds.at(c1.channel);
                field_types.push_back(c1.type);
            }
            combined.original_channel_writes.push_back(c2);
            combined.temporary_for_writes.push_back(unique_name("t") + ".temp");
            combined.bounds = merge_channel_bounds(combined.bounds, channel_to_bounds.at(c2.channel));
            field_types.push_back(c2.type);
        }
        if (!combined.original_channel_writes.empty()) {
            combined.struct_type = generate_struct(field_types);
            debug(4) << "To combine: ";
            for (auto o : combined.original_channel_writes) {
                channel_to_combined[o.channel] = combined;
                debug(4) << o.channel << " ";
            }
            debug(4) << " to channel " << combined.combined_channel << "\n";
        }
    }
}

class CombineChannels : public IRMutator {
    using IRMutator::visit;

public:
    CombineChannels(const map<string, CombinedChannel> &channel_to_combined) :
       channel_to_combined(channel_to_combined) {}

private:
    const map<string, CombinedChannel> &channel_to_combined;

    string func; // The current func

public:
    Stmt visit(const ProducerConsumer *op) override {
        if (op->is_producer) {
            func = op->name;
            if (channel_to_combined.find(op->name) == channel_to_combined.end()) {
                // Not to combine with anything
                return IRMutator::visit(op);
            }
            // Remove this Produce. If this channel is the first of several channels combined, insert
            // a Produce for the combined channel
            const CombinedChannel &combined = channel_to_combined.at(op->name);
            Stmt stmt = IRMutator::mutate(op->body);
            if (op->name == combined.original_channel_writes[0].channel) {
                stmt = ProducerConsumer::make(combined.combined_channel, true, stmt);
            }
            return stmt;
        } else {
            return IRMutator::visit(op);
        }
    }

    Stmt visit(const Realize *op) override {
        if (channel_to_combined.find(op->name) == channel_to_combined.end()) {
            // Not to combine with anything
            return IRMutator::visit(op);
        }
        const CombinedChannel &combined = channel_to_combined.at(op->name);

        // Remove this Realize. If this channel is the first of several channels combined, insert
        // a Realize for the combined channel
        Stmt stmt = IRMutator::mutate(op->body);
        if (op->name == combined.original_channel_writes[0].channel) {
            stmt = Realize::make(combined.combined_channel, {combined.struct_type}, op->memory_type, combined.bounds, const_true(), stmt);
        }
        return stmt;
    }

    Stmt visit(const For* op) override {
        Stmt new_body = IRMutator::mutate(op->body);

        // Insert reads of combined channels in this func
        vector<string> processed;
        for (auto entry : channel_to_combined) {
            const CombinedChannel &combined = entry.second;
            if (combined.consumer != func) {
                continue;
            }
            if (std::find(processed.begin(), processed.end(), combined.combined_channel) != processed.end()) {
                continue;
            }
            if (combined.enclosing_read_loop != op->name) {
                continue;
            }
            processed.push_back(combined.combined_channel);
            vector<Expr> args = combined.read_args;
            args.insert(args.begin(), combined.combined_channel);
            Expr new_call = Call::make(combined.struct_type, Call::read_channel, args, Call::Intrinsic);
            Stmt read = Provide::make(combined.temporary_for_read, {new_call}, {Expr(0)});
            if (!equal(combined.read_condition, const_true())) {
                read = IfThenElse::make(combined.read_condition, read, Stmt());
            }
            new_body = Block::make(read, new_body);
        }

        Stmt s = For::make(op->name, op->min, op->extent, op->for_type, op->device_api, new_body);

        // Insert Realizes of temporaries for reading/writing combined channels before the outermost loop
        processed.clear();
        for (auto entry : channel_to_combined) {
            const CombinedChannel &combined = entry.second;
            if (std::find(processed.begin(), processed.end(), combined.combined_channel) != processed.end()) {
                continue;
            }
            if (combined.producer == func && combined.outermost_write_loop == op->name) {
                for (size_t i = 0; i < combined.temporary_for_writes.size(); i++) {
                    const ChannelAccess &write = combined.original_channel_writes[i];
                    s = Realize::make(combined.temporary_for_writes[i], {write.type}, MemoryType::Auto,
                                            {Range(0, 1)}, const_true(), s);
                }
                processed.push_back(combined.combined_channel);
            }
            if (combined.consumer == func && combined.outermost_read_loop == op->name) {
                s = Realize::make(combined.temporary_for_read, {combined.struct_type}, MemoryType::Auto,
                                        {Range(0, 1)}, const_true(), s);
                processed.push_back(combined.combined_channel);
            }
        }
        return s;
    }

    Expr visit(const Call* op) override {
        if (op->is_intrinsic(Call::read_channel)) {
            const StringImm *v = op->args[0].as<StringImm>();
            string channel = v->value;
            if (channel_to_combined.find(channel) == channel_to_combined.end()) {
                return IRMutator::visit(op);
            }
            const CombinedChannel &combined = channel_to_combined.at(channel);
            size_t i;
            for (i = 0; i < combined.original_channel_writes.size(); i++) {
                if (combined.original_channel_writes[i].channel == channel) {
                    break;
                }
            }
            Expr get_temp = Call::make(combined.struct_type, combined.temporary_for_read, {Expr(0)}, Call::PureIntrinsic);
            Expr e = Call::make(op->type, Call::read_field, {get_temp, Expr(i)}, Call::PureIntrinsic);
            return e;
        }
        return IRMutator::visit(op);
    }

    Expr mutate(const Expr &expr) override {
        return IRMutator::mutate(expr);
    }

    Stmt mutate(const Stmt &stmt) override {
        const Evaluate *eval = stmt.as<Evaluate>();
        if (!eval) {
            return IRMutator::mutate(stmt);
        }
        const Call *call = eval->value.as<Call>();
        if (!call || !call->is_intrinsic(Call::write_channel)) {
            return IRMutator::mutate(stmt);
        }
        const StringImm *v = call->args[0].as<StringImm>();
        string channel = v->value;
        if (channel_to_combined.find(channel) == channel_to_combined.end()) {
            return IRMutator::mutate(stmt);
        }
        const CombinedChannel &combined = channel_to_combined.at(channel);
        bool is_last = false;
        string temp;
        for (size_t i = 0; i < combined.original_channel_writes.size(); i++) {
            if (combined.original_channel_writes[i].channel == channel) {
                temp = combined.temporary_for_writes[i];
                if (i == combined.original_channel_writes.size() - 1) {
                    is_last = true;
                }
                break;
            }
        }
        Stmt provide = Provide::make(temp, {mutate(call->args[1])}, {0});
        if (!is_last) {
            return provide;
        }

        vector<Expr> args, temporaries;
        args.push_back(Expr(combined.combined_channel));
        for (size_t i = 0; i < combined.temporary_for_writes.size(); i++) {
            Expr call = Call::make(combined.original_channel_writes[i].type, combined.temporary_for_writes[i], {Expr(0)}, Call::PureIntrinsic);
            temporaries.push_back(call);
        }
        Expr make_struct = Call::make(combined.struct_type, Call::make_struct, temporaries, Call::PureIntrinsic);
        args.push_back(make_struct);
        for (auto a : combined.write_args) {
            args.push_back(a);
        }
        Stmt write = Evaluate::make(Call::make(combined.struct_type, Call::write_channel, args, Call::Intrinsic));
        Stmt block = Block::make(provide, write);
        return block;
    }
};
} // namespace

Stmt combine_channels(const Stmt &s) {
    GatherChannelAccess gatherer;
    s.accept(&gatherer);

    map<string, CombinedChannel> channel_to_combined;
    decide_channels_to_combine(gatherer.channel_accesses, gatherer.channel_to_bounds, channel_to_combined);

    CombineChannels combiner(channel_to_combined);
    Stmt stmt = combiner.mutate(s);
    return stmt;
}

} // Internal
} // Halide
