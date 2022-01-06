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
#include "../../Halide/src/Simplify.h"
#include "./Place.h"
#include "./BuildCallRelation.h"
#include "./DebugPrint.h"
#include "./SpaceTimeTransform.h"
#include "./Utilities.h"
#include "Substitute.h"
#include "IRMutator.h"
#include <algorithm>

namespace Halide {
namespace Internal {
class PlaceDeviceFunctions : public IRMutator {
    using IRMutator::visit;
public:
    PlaceDeviceFunctions(const map<string, Function> &_env, const Target &t) :
        env(_env), target(t)
    {
    }

private:
    const map<string, Function> &env;
    const Target &target;

    Stmt visit(const ProducerConsumer *op) override {
        Function func;
        if (op->is_producer && function_is_in_environment(op->name, env, func) && func.place() == Place::Device) {
            user_assert(target.has_feature(Target::IntelFPGA)) << "A device (currently IntelFPGA) is expected for the target when using Place::Device for Func "
                << op->name << "\n" << "Avoid this error by adding code like this: target.set_feature(Target::IntelFPGA);\n";
            internal_assert(func.definition().defined() && func.updates().size() == 0)
                << "Device Func " << op->name << " is expected to have one and only one definition\n";
            Stmt body = mutate(op->body);
            Stmt new_body = For::make(op->name + ".s0.run_on_device",
                                      0,
                                      1,
                                      ForType::Parallel, // The loop type is arbitrarily chosen here: it does not really matter.
                                      DeviceAPI::OpenCL, // TODO: allow other device APIs
                                      body);
            Stmt stmt = ProducerConsumer::make(op->name, op->is_producer, new_body);
            return stmt;
        } else {
            return IRMutator::visit(op);
        }
    }
};

class ReplaceReferencesWithChannels : public IRMutator {
    using IRMutator::visit;
public:
    ReplaceReferencesWithChannels(const map<string, Region>       &channel2bounds,
                                  const map<string, vector<Expr>> &channel2write_args,
                                  const map<string, vector<Expr>> &channel2read_args,
                                  const map<string, Function>     &env) :
                                      channel2bounds(channel2bounds), channel2write_args(channel2write_args),
                                      channel2read_args(channel2read_args), env(env) { user_assert(env.size() > 0); }

private:
    const map<string, Region>       &channel2bounds;
    const map<string, vector<Expr>> &channel2write_args;
    const map<string, vector<Expr>> &channel2read_args;
    const map<string, Function>     &env;
    map<string, int>                 multiple_values;

    Stmt visit(const Realize *op) override {
        // Process the body of Realize; in that process, the compiler might create a channel array for this function
        // due to a visit to a Provide inside the Realize. If so, we realize the channel array here.
        if (channel2bounds.find(op->name) != channel2bounds.end()) {
            if(op->types.size() == 1){
                Stmt stmt = Realize::make(op->name + ".channel", op->types, op->memory_type, channel2bounds.at(op->name), mutate(op->condition), mutate(op->body));
                return stmt;
            } else {
                multiple_values[op->name] = op->types.size();
                Stmt stmt = mutate(op->body);
                for (size_t i = 0; i < op->types.size(); i++) {
                    std::vector<Type> types;
                    types.push_back(op->types[i]);
                    stmt = Realize::make(op->name + "." + std::to_string(i) + ".channel", types, op->memory_type, channel2bounds.at(op->name), mutate(op->condition), stmt);
                }
                return stmt;
            }
        } else {
            Stmt stmt = Realize::make(op->name, op->types, op->memory_type, op->bounds, mutate(op->condition), mutate(op->body));
            return stmt;
        }
    }

    Stmt visit(const Provide *op) override {
        if (channel2write_args.find(op->name) != channel2write_args.end()) {
            if(op->values.size() == 1){
                // internal_assert(op->values.size() == 1) << "Values of a Provide is expected to have been combined into a tuple before writing to a channel\n";
                Expr value = mutate(op->values[0]);
                vector<Expr> args(channel2write_args.at(op->name));
                args.insert(args.begin(), value);
                args.insert(args.begin(), Expr(op->name + ".channel"));
                Stmt write_call = Evaluate::make(Call::make(value.type(), Call::write_channel, args, Call::Intrinsic));
                return write_call;
            } else {
                Stmt write_call = Stmt();
                for (size_t i = 0; i < op->values.size(); i++) {
                    Expr value = mutate(op->values[i]);
                    vector<Expr> args(channel2write_args.at(op->name));
                    args.insert(args.begin(), value);
                    args.insert(args.begin(), Expr(op->name + "." + std::to_string(i) + ".channel"));
                    if (write_call.defined()) {
                        write_call = Block::make(Evaluate::make(Call::make(value.type(), Call::write_channel, args, Call::Intrinsic)),
                                                 write_call);
                    } else {
                        write_call = Evaluate::make(Call::make(value.type(), Call::write_channel, args, Call::Intrinsic));
                    }
                }
                return write_call;
            }
        } else {
            return IRMutator::visit(op);
        }
    }

    Expr visit(const Call *op) override {
        if (channel2read_args.find(op->name) != channel2read_args.end()) {
            vector<Expr> args(channel2read_args.at(op->name));
            if (multiple_values.find(op->name) != multiple_values.end() && multiple_values[op->name] > 1) {
                args.insert(args.begin(), Expr(op->name + "." + std::to_string(op->value_index) + ".channel"));
            } else {
                args.insert(args.begin(), Expr(op->name + ".channel"));
            }
            Expr new_call = Call::make(op->type, Call::read_channel, args, Call::Intrinsic);
            return new_call;
        } else {
            return IRMutator::visit(op);
        }
    }
};

class ReplaceReferencesWithMemChannels : public IRMutator {
    using IRMutator::visit;
public:
    ReplaceReferencesWithMemChannels(const set<string> &_mem_channels,
                                    const map<string, Function> &_env,
                                    vector<std::pair<string, Expr>> &_lets) :
        mem_channels(_mem_channels), env(_env), letstmts_backup(_lets) {
            user_assert(env.size() > 0);
            is_set_bounds = true;
        }

private:
    const set<string> &mem_channels;
    const map<string, Function> &env;
    vector<string> enclosing_unrolled_loops;
    set<string> channels;
    vector<std::pair<string, Expr>> &letstmts_backup;
    bool is_set_bounds;

    string get_channel_name(string name) {
        string func_name = extract_first_token(name);
        for (auto &m : mem_channels) {
            // Multiple inputs can be isolated into the same function. For example,
            // inputs A and B are isolated into function A_serializer:
            // realize A_serializer_B_im
            //   realize A_serializer
            //     for (A_serializer.s0.i, 0, N)
            //       A_serializer(A_serializer.s0.i) = A(A_serializer.s0.i)
            //       A_serializer_B_im(A_serializer.s0.i) = B(A_serializer.s0.i)
            // However, only A_serializer is identified as mem_channel.
            // To help perform SoA to AoS optimization, we also replace B with mem_channel,
            // and in the combine_channels stage, two mem_channels are combined together.
            if (func_name == m || (starts_with(func_name, m) && ends_with(func_name, "_im"))) {
                debug(4) << "Find references " << name << " associated with channel " << m << "\n";
                return m;
            }
        }
        return "";
    }

    Stmt visit(const For *op) override {
        vector<string> names = split_string(op->name, ".");
        if ((op->for_type == ForType::Vectorized) && names.size() > 2) {
            enclosing_unrolled_loops.push_back(op->name);
        }
        if (op->device_api != DeviceAPI::OpenCL && (op->min.as<Variable>() || op->extent.as<Variable>())) {
            is_set_bounds = false;
        }
        Stmt stmt = IRMutator::visit(op);
        if ((op->for_type == ForType::Vectorized) && names.size() > 2) {
            enclosing_unrolled_loops.pop_back();
        }
        return stmt;
    }

    // Mark the memory channel with a special name
    Stmt visit(const Allocate *op) override {
        is_set_bounds = true;
        Stmt body = mutate(op->body);
        if (!get_channel_name(op->name).empty() && is_set_bounds) {
            string name = op->name + ".mem_channel";
            return Allocate::make(name, op->type, op->memory_type , op->extents,
                                op->condition, body, op->new_expr, op->free_function);
        }
        return Allocate::make(op->name, op->type, op->memory_type, op->extents,
                            op->condition, body, op->new_expr, op->free_function);
    }

    Stmt visit(const LetStmt *op) override {
        Stmt body = mutate(op->body);
        string name = op->name;
        Expr value = op->value;
        if (!get_channel_name(op->name).empty() && is_set_bounds) {
            string func_name = extract_first_token(op->name);
            string new_func_name = func_name + ".mem_channel";
            name = name.replace(name.find(func_name), func_name.length(), new_func_name);
            if (extract_token(op->name, 2) == "stride") {
                int idx = atoi(extract_token(op->name, 3).c_str());
                string last_dim = func_name + ".stride." + to_string(idx -1);
                string new_last_dim = new_func_name + ".stride." + to_string(idx -1);
                value = substitute(last_dim, Variable::make(Int(32), new_last_dim), value);
            }
            letstmts_backup.push_back({ name, value });
        } else {
            letstmts_backup.push_back({ name, value });
        }
        return LetStmt::make(name, value, body);
    }

    Stmt visit(const Store *op) override {
        if (!get_channel_name(op->name).empty() && is_set_bounds) {
            vector<Expr> args;
            vector<string> loops;
            args.push_back(Expr(op->name + ".mem_channel"));
            Expr value = mutate(op->value);
            args.push_back(value);
            for (auto &l : enclosing_unrolled_loops) {
                Expr var = Variable::make(Int(32), l);
                args.push_back(var);
            }
            if (channels.find(op->name) == channels.end()) {
                channels.insert(op->name);
            }
            Stmt write_call = Evaluate::make(Call::make(value.type(), Call::write_mem_channel, args, Call::Intrinsic, FunctionPtr(), 0, Buffer<> (), op->param));
            return write_call;
        } else {
            return IRMutator::visit(op);
        }
    }

    Expr visit(const Load *op) override {
        if (!get_channel_name(op->name).empty() && is_set_bounds) {
            internal_assert(channels.find(op->name) != channels.end());
            vector<Expr> args;
            args.push_back(Expr(op->name + ".mem_channel"));
            for (auto &l : enclosing_unrolled_loops) {
                Expr var = Variable::make(Int(32), l);
                args.push_back(var);
            }
            Expr new_call = Call::make(op->type, Call::read_mem_channel, args, Call::Intrinsic, FunctionPtr(), 0, op->image, op->param);
            return new_call;
        } else {
            return IRMutator::visit(op);
        }
    }
};

class IdentifyChannelDimensions : public IRVisitor {
    using IRVisitor::visit;
public:
    IdentifyChannelDimensions(const vector<string>  &_funcs_outputting_to_channels, const map<string, Function> &_env,
                              const LoopBounds &_global_bounds) : funcs_outputting_to_channels(_funcs_outputting_to_channels),
                              env(_env), global_bounds(_global_bounds) { }

    map<string, Region>       channel2bounds;     // Channel name -> bounds for Realize of the channel
    map<string, vector<Expr>> channel2write_args; // Channel name -> args for writing the channel
    map<string, vector<Expr>> channel2read_args;  // Channel name -> args for reading the channel

private:
    const vector<string>                          &funcs_outputting_to_channels;
    const map<string, Function>                   &env;
    const LoopBounds                              &global_bounds;
    vector<tuple<string, Expr, bool>>              enclosing_loops;     // <Full name, extent, is unrolled loop> for
                                                                        // every loop enclosing the current IR
    map<string, vector<tuple<string, Expr, bool>>> channel2write_loops; // Channel name --> <Full name, extent, is unrolled loop>
                                                                        // for loops enclosing the (only) writing to the channel
    // map<string, int>                               multiple_values;

    void visit(const For *op) override {
        bool dummy_loop = ends_with(op->name, ".run_on_device");
        if (dummy_loop) {
            IRVisitor::visit(op);
            return;
        }

        Expr extent;
        if (global_bounds.find(op->name) != global_bounds.end()) {
            extent = global_bounds.at(op->name).max - global_bounds.at(op->name).min;
        } else {
            extent = op->extent;
        }
        extent = simplify(extent);
        bool is_unrolled = (op->for_type == ForType::Unrolled || op->for_type == ForType::Vectorized);
        // user_assert(!is_unrolled || is_const(extent)) << "Space loop "
        //             << op->name << " expects a constant extent. Right now the extent is " << to_string(extent) << "\n";

        enclosing_loops.push_back(tuple<string, Expr, bool>(op->name, extent, is_unrolled));
        IRVisitor::visit(op);
        enclosing_loops.pop_back();
    }

    void visit(const Provide *op) override {
        if (std::find(funcs_outputting_to_channels.begin(), funcs_outputting_to_channels.end(), op->name) != funcs_outputting_to_channels.end()) {
            // Statically there can only be 1 writing of the channel
            internal_assert(channel2write_loops.find(op->name) == channel2write_loops.end());
            channel2write_loops[op->name] = enclosing_loops;
            debug(4) << "Enclosing write loops of channel for " << op->name << ": "
                     << to_string(enclosing_loops, true) << "\n";
            // if (op->values.size() > 1) {
            //     multiple_values[op->name] == op->values.size();
            // }
        }
        IRVisitor::visit(op);
    }

    void visit(const Call *op) override {
        if (std::find(funcs_outputting_to_channels.begin(), funcs_outputting_to_channels.end(), op->name) != funcs_outputting_to_channels.end()) {
            // Writing of the channel must have already been processed in a producer Func.
            internal_assert(channel2write_loops.find(op->name) != channel2write_loops.end());

            if (op->value_index == 0) {
                // Statically there can only be 1 reading of the channel
                internal_assert(channel2bounds.find(op->name) == channel2bounds.end());
                internal_assert(channel2write_args.find(op->name) == channel2write_args.end());
                internal_assert(channel2read_args.find(op->name) == channel2read_args.end());
  
                debug(4) << "Enclosing read loops of channel for " << op->name << ": "
                        << to_string(enclosing_loops, true) << "\n";

                // Merge the set of unroll loops from both the write and the read. For example, if
                // the write's unrolled loops are {x, y}, and the read's unrolled loops are {y, z},
                // then we should allocate a channel whose dimensions are the extents of loop {x, y, z}.
                // We assume that the order and bounds of loop x, y, z in the producer are consistent
                // with those in the consumer.
                Region bounds;
                vector<Expr> write_args, read_args;
                for (const auto &l : enclosing_loops) {
                    const string &read_loop_name = std::get<0>(l);
                    const Expr &read_loop_extent = std::get<1>(l);
                    bool read_loop_is_unrolled = std::get<2>(l);
                    const auto &write_loops = channel2write_loops.at(op->name);
                    for (const auto &wl : write_loops) {
                        const string &write_loop_name = std::get<0>(wl);
                        if (extract_after_tokens(read_loop_name, 2) != extract_after_tokens(write_loop_name, 2)) {
                            continue;
                        }
                        const Expr &write_loop_extent = std::get<1>(wl);
                        bool write_loop_is_unrolled = std::get<2>(wl);
                        if (read_loop_is_unrolled || write_loop_is_unrolled) {
                            // Usually the extents of the read and write loop should be equal. But it is possible
                            // that one of them might have been marked as removed, and consequently has been
                            // changed into a unit loop.
                            user_assert(equal(read_loop_extent, 1) || equal(write_loop_extent, 1) ||
                                        equal(read_loop_extent, write_loop_extent)) << "Same extent is expected for loop "
                                << write_loop_name << " and " << read_loop_name << ", as one or both of the loops are unrolled\n";
                            write_args.push_back(Variable::make(Int(32), write_loop_name));
                            read_args.push_back(Variable::make(Int(32), read_loop_name));
                            bounds.push_back(Range(0, max(read_loop_extent, write_loop_extent)));
                        }
                        break;
                    }
                }

                // Add the min depth.
                internal_assert(env.find(op->name) != env.end());
                Function func = env.at(op->name);
                Expr min_depth(func.min_depth());
                bounds.push_back(Range(0, min_depth));

                debug(4) << "channel2bounds[" << op->name  << "]: " << to_string(bounds) << "\n";
                debug(4) << "channel2write_args[" << op->name  << "]: " << to_string<Expr>(write_args) << "\n";
                debug(4) << "channel2read_args[" << op->name  << "]: " << to_string<Expr>(read_args) << "\n";

                channel2bounds[op->name] = std::move(bounds);
                channel2write_args[op->name] = std::move(write_args);
                channel2read_args[op->name] = std::move(read_args);
            }
        }
        IRVisitor::visit(op);
    }
};

class ReplaceReferencesWithShiftRegisters : public IRMutator {
    using IRMutator::visit;
public:
    ReplaceReferencesWithShiftRegisters(const std::map<std::string, RegBound > &_reg_size_map) :
        reg_size_map(_reg_size_map) { }

private:
    const std::map<std::string, RegBound > &reg_size_map;
    map<string, vector<size_t>>             remove_dim_map;
    map<string, int>                        multiple_values;

    Stmt visit(const Realize *op) override {
        if (reg_size_map.find(op->name) != reg_size_map.end()) {
            Region bounds = op->bounds;
            bounds.clear();
            for (size_t i = 0; i < op->bounds.size(); i++) {
                if (is_one(op->bounds[i].extent)) {
                    remove_dim_map[op->name].push_back(i);
                } else {
                    bounds.emplace_back(op->bounds[i].min, op->bounds[i].extent);
                }
            }

            if(op->types.size() == 1) {
                Stmt stmt = Realize::make(op->name + ".shreg", op->types, op->memory_type, bounds, mutate(op->condition), mutate(op->body));
                return stmt;
            } else {
                multiple_values[op->name] = op->types.size();
                Stmt stmt = mutate(op->body);
                for (size_t i = 0; i < op->types.size(); i++) {
                    std::vector<Type> types;
                    types.push_back(op->types[i]);
                    stmt = Realize::make(op->name + "." + std::to_string(i) + ".shreg", types, op->memory_type, bounds, mutate(op->condition), stmt);
                }
                return stmt;
            }
        }
        return IRMutator::visit(op);
    }

    Stmt visit(const Provide *op) override {
        if (reg_size_map.find(op->name) != reg_size_map.end()) {
            if(op->values.size() == 1) {
                // internal_assert(op->values.size() == 1) << "Values of a Provide is expected to have been combined into a tuple before writing to a channel\n";
                vector<Expr> args;
                args.push_back(Expr(op->name + ".shreg"));
                for (size_t j = 0; j < op->args.size(); j++) {
                    bool remove = false;
                    for (size_t k = 0; k < remove_dim_map[op->name].size(); k++) {
                        if (j == remove_dim_map[op->name][k]) {
                            remove = true;
                            break;
                        }
                    }
                    if (!remove) {
                        args.push_back(op->args[j]);
                    }
                }
                // We should only have one value
                args.push_back(mutate(op->values[0]));
                return Evaluate::make(Call::make(op->values[0].type(), Call::write_shift_reg, args, Call::Intrinsic));
            } else {
                Stmt write_call = Stmt();
                for (size_t i = 0; i < op->values.size(); i++) {
                    Expr value = mutate(op->values[i]);
                    vector<Expr> args;
                    args.push_back(Expr(op->name + "." + std::to_string(i) + ".shreg"));
                    for (size_t j = 0; j < op->args.size(); j++) {
                        bool remove = false;
                        for (size_t k = 0; k < remove_dim_map[op->name].size(); k++) {
                            if (j == remove_dim_map[op->name][k]) {
                                remove = true;
                                break;
                            }
                        }
                        if (!remove) {
                            args.push_back(op->args[j]);
                        }
                    }
                    args.push_back(value);
                    if (write_call.defined()) {
                        write_call = Block::make(Evaluate::make(Call::make(value.type(), Call::write_shift_reg, args, Call::Intrinsic)),
                                                 write_call);
                    } else {
                        write_call = Evaluate::make(Call::make(value.type(), Call::write_shift_reg, args, Call::Intrinsic));
                    }
                }
                return write_call;
            }
        }
        return IRMutator::visit(op);
    }

    Expr visit(const Call *op) override {
        if (reg_size_map.find(op->name) != reg_size_map.end()) {
            vector<Expr> args;
            if(multiple_values.find(op->name) != multiple_values.end() && multiple_values[op->name] > 1) {
                args.push_back(Expr(op->name + "." + std::to_string(op->value_index) + ".shreg"));
            } else {
                args.push_back(Expr(op->name+ ".shreg"));
            }

            for (size_t j = 0; j < op->args.size(); j++) {
                bool remove = false;
                for (size_t k = 0; k < remove_dim_map[op->name].size(); k++) {
                    if (j == remove_dim_map[op->name][k]) {
                        remove = true;
                        break;
                    }
                }
                if (!remove) {
                    args.push_back(op->args[j]);
                }
            }
            return Call::make(op->type, Call::read_shift_reg, args, Call::Intrinsic);
        }
        return IRMutator::visit(op);
    }
};

class ProducerConsumerChecker : public IRVisitor {
    public:
        ProducerConsumerChecker(
                 std::map<std::string, bool> &used_in_producer,
                 std::map<std::string, bool> &used_in_consumer,
                 std::map<std::string, bool> &used_in_realize) :
            used_in_producer(used_in_producer),
            used_in_consumer(used_in_consumer),
            used_in_realize(used_in_realize) {}

    private:
        using IRVisitor::visit;

        std::map<std::string, bool> &used_in_producer;
        std::map<std::string, bool> &used_in_consumer;
        std::map<std::string, bool> &used_in_realize;
        std::vector<std::string> current_producers;
        std::vector<std::string> current_consumers;

        void visit(const ProducerConsumer *op) override {
            if (op->is_producer) {
                current_producers.push_back(op->name);
                op->body.accept(this);
                current_producers.pop_back();
            } else {
                current_consumers.push_back(op->name);
                op->body.accept(this);
                current_consumers.pop_back();
            }
        }

        void visit(const Realize *op) override {
            used_in_realize[op->name] = true;
            op->body.accept(this);
        }

        void visit(const Provide *op) override {
            for (size_t i = 0; i < current_producers.size(); i++) {
                if (op->name == current_producers[i])
                    used_in_producer[op->name] = true;
            }
            for (size_t i = 0; i < op->values.size(); i++) {
                op->values[i].accept(this);
            }
        }

        void visit(const Call *op) override {
            for (size_t i = 0; i < current_consumers.size(); i++) {
                if (op->name == current_consumers[i])
                    used_in_consumer[op->name] = true;
            }
            for (size_t i = 0; i < op->args.size(); i++) {
                op->args[i].accept(this);
            }
        }
};

Stmt place_device_functions(Stmt s, const map<string, Function> &env, const Target &t) {
    PlaceDeviceFunctions placer(env, t);
    return placer.mutate(s);
}

// Find the functions that write their outputs to channels. This happens when a producer and consumer function are both on device.
// Also when it is written in the producer and called in the consumer
void identify_funcs_outputting_to_channels(Stmt s, const map<string, Function>       &env,
                                           const map<string, vector<string>> &reverse_call_graph,
                                           const LoopBounds &global_bounds,
                                           map<string, Region> &channel2bounds,
                                           map<string, vector<Expr>> &channel2write_args,
                                           map<string, vector<Expr>> &channel2read_args) {
    vector<string> funcs_outputting_to_channels;
    for (auto itr : env) {
        const string   &func_name = itr.first;
        const Function &func = itr.second;
        if (func.place() == Place::Device) {
            // See if the function is used on the device.
            if (reverse_call_graph.find(func_name) != reverse_call_graph.end()) {
                const vector<string> &consumers = reverse_call_graph.at(func_name);
                if (consumers.size() == 1) {
                    const string &consumer = consumers[0];
                    internal_assert(env.find(consumer) != env.end());
                    const Function &consumer_func = env.at(consumer);
                    if (consumer_func.place() == Place::Device) {
                        funcs_outputting_to_channels.push_back(func_name);
                    }
                } else {
                    // So far, we restrict a channel can have only one write and only one read site.
                    // When there is more than one read site, the producer and consumer function can communicate only through memory.
                }
            }
        }
    }
    std::map<std::string, bool> used_in_producer;
    std::map<std::string, bool> used_in_consumer;
    std::map<std::string, bool> used_in_realize;
    ProducerConsumerChecker checker(used_in_producer, used_in_consumer, used_in_realize);
    s.accept(&checker);
    vector<string> funcs_to_channels;
    for (size_t i = 0; i < funcs_outputting_to_channels.size(); i++) {
        std::string name = funcs_outputting_to_channels[i];
        if ((used_in_producer.find(name) != used_in_producer.end())
                && (used_in_consumer.find(name) != used_in_consumer.end())) {
            funcs_to_channels.push_back(name);
        }
    }
    debug(4) << "Funcs to output to channels: " << to_string<string>(funcs_to_channels) << "\n";

    IdentifyChannelDimensions identify(funcs_to_channels, env, global_bounds);
    s.accept(&identify);
    channel2bounds = std::move(identify.channel2bounds);
    channel2write_args = std::move(identify.channel2write_args);
    channel2read_args = std::move(identify.channel2read_args);
}

// To replace a memory store (load) in a producer (consumer) with a memory channel write (read), we require that both
// the producer and the consumer have only 1 PE. That is, if the producer (consumer) has unrolled loops u1~un, the
// original memory store (load) should be guarded by a path condition that contains e.g. "u1=0 && u2==0 && ... && un=0".
// This class checks that the given func has a single PE.
class CheckIsSinglePE : public IRVisitor {
    using IRVisitor::visit;

public:
    CheckIsSinglePE(const string &func_name, const string &mem_name, const vector<string> &loops_to_serialize) :
        loops_to_serialize(loops_to_serialize), func_name(func_name), mem_name(mem_name) {
        in_function = false;
        func_def_already_visited = false;
        is_single_PE = false;
    }

private:
    const vector<string> &loops_to_serialize; // Loops that will be serialized but not yet.
    const string &func_name;                  // The function to check.
    const string &mem_name;                // The memory channel to transfer data.
    bool in_function;                         // The current IR is in the definition of the function.
    bool on_device;                           // THe current IR is on device.
    bool func_def_already_visited;            // The func is already visited.
    vector<string> unrolled_loops;            // Current unrolled loops
    Expr path_condition;                      // The path condition to a read/write_mem_channel

public:
    bool is_single_PE;             // The func is a single PE.

private:

public:
    void visit(const ProducerConsumer *op) override {
        if (func_def_already_visited) {
            return;
        }
        if (op->is_producer && op->name == func_name) {
            // Initial all the variables
            in_function = true;
            on_device = false;
            internal_assert(unrolled_loops.empty());
            path_condition = const_true();
            op->body.accept(this);
            func_def_already_visited = true;
        } else {
            in_function = false;
            op->body.accept(this);
        }
    }

    // For path condition: we track only IfThenElse, not Select, because channel writes do not happen inside Select,
    // and we assume channel reads have the same condition with channel writes.
    void visit(const IfThenElse *op) override {
        if (!in_function) {
            // We do not gather condition outside of a function.
            IRVisitor::visit(op);
            return;
        }
        Expr old_condition = path_condition;

        path_condition = equal(old_condition, const_true()) ? op->condition : old_condition && op->condition;
        op->then_case.accept(this);

        if (op->else_case.defined()) {
            path_condition = equal(old_condition, const_true()) ? !op->condition : old_condition && !op->condition;
            op->else_case.accept(this);
        }

        path_condition = old_condition;
        return;
    }

    void visit(const For *op) override {
        if (!in_function) {
            IRVisitor::visit(op);
            return;
        }

        bool prev_on_device = on_device;
        if (ends_with(op->name, ".run_on_device")) {
            on_device = true;
        }

        if ((op->for_type == ForType::Unrolled) || (op->for_type == ForType::PragmaUnrolled) || (op->for_type == ForType::DelayUnroll)) {
            unrolled_loops.push_back(op->name);
        }

        op->body.accept(this);

        if ((op->for_type == ForType::Unrolled) || (op->for_type == ForType::PragmaUnrolled) || (op->for_type == ForType::DelayUnroll)) {
            unrolled_loops.pop_back();
        }

        if (ends_with(op->name, ".run_on_device")) {
            on_device = prev_on_device;
        }
    }

    void visit(const Store *op) override {
        if (!in_function) {
            IRVisitor::visit(op);
        }
        if (op->name == mem_name) {
            Expr single_PE_condition;
            vector<string> unrolled_loops_without_terms;
            is_single_PE = check_is_single_PE(on_device, path_condition, unrolled_loops, loops_to_serialize, single_PE_condition, unrolled_loops_without_terms);
            if (!is_single_PE) {
                debug(4) << "Warning: Failed to serialize an output in function " << func_name << ": path condition to \n\t" << Stmt(op) << "\n"
                    << "is " << "\n\t" << path_condition << "\n"
                    << "The condition should contain terms like u==some constant (e.g. u==0) for every unrolled loop u. "
                    << "However, this condition misses such terms for unrolled loops "
                    << to_string<string>(unrolled_loops_without_terms) << ". Check the specifiction, or the IR.\n\n";
            }
            return;
        }
        IRVisitor::visit(op);
    }

    void visit(const Load *op) override {
        if (!in_function) {
            IRVisitor::visit(op);
        }
        if (op->name == mem_name) {
            Expr single_PE_condition;
            vector<string> unrolled_loops_without_terms;
            is_single_PE = check_is_single_PE(on_device, path_condition, unrolled_loops, loops_to_serialize, single_PE_condition, unrolled_loops_without_terms);
            if (!is_single_PE) {
                debug(4) << "Warning: Failed to serialize an input in function " << func_name << ": path condition to \n\t" << Expr(op) << "\n"
                    << "is " << "\n\t" << path_condition << "\n"
                    << "The condition should contain terms like u==some constant (e.g. u==0) for every unrolled loop u. "
                    << "However, this condition misses such terms for unrolled loops "
                    << to_string<string>(unrolled_loops_without_terms) << ". Check the specifiction, or the IR.\n\n";
            }
            return;
        }
        IRVisitor::visit(op);
    }
};

// Find the functions that write their outputs to virtual channels(actually communicate through memory).
// This happens when producer and consumer are on host and FPGA, respectively, and when both have single PEs.
set<string> identify_funcs_outputting_to_mem_channels(Stmt s, const map<string, Function> &env,
                                                      const map<string, vector<string>> &reverse_call_graph,
                                                      map<string, Place> &funcs_using_mem_channels,
                                                      const vector<string> &loops_to_serialize) {
    set<string> funcs_outputting_to_mem_channels;

    for (auto itr : env) {
        const string &func_name = itr.first;
        const Function &func = itr.second;
        if (func.place() == Place::Device) {
            if (reverse_call_graph.find(func_name) != reverse_call_graph.end()) {
                const vector<string> &consumers = reverse_call_graph.at(func_name);
                if (consumers.size() == 1) {
                    const string &consumer = consumers[0];
                    internal_assert(env.find(consumer) != env.end());
                    const Function &consumer_func = env.at(consumer);
                    if (consumer_func.place() == Place::Host && consumer_func.isolated_from_as_consumer() == func_name && func.isolated_from_as_consumer() != "") {
                        CheckIsSinglePE checker(func_name, func_name, loops_to_serialize);
                        s.accept(&checker);
                        if (checker.is_single_PE) {
                            funcs_outputting_to_mem_channels.insert(func_name);
                            funcs_using_mem_channels[func_name] = Place::Device;
                            funcs_using_mem_channels[consumer] = Place::Host;
                            debug(2) << "Find Func output to mem channel\n"
                                     << "Producer: " << func_name << "\n"
                                     << "Consumer: " << consumer << "\n";
                        }
                    }
                }
            }
        } else if(func.place() == Place::Host) {
            if (reverse_call_graph.find(func_name) != reverse_call_graph.end()) {
                const vector<string> &consumers = reverse_call_graph.at(func_name);
                if (consumers.size() == 1) {
                    const string &consumer = consumers[0];
                    internal_assert(env.find(consumer) != env.end());
                    const Function &consumer_func = env.at(consumer);
                    if (consumer_func.place() == Place::Device && func.isolated_from_as_producer() == consumer && consumer_func.isolated_from_as_producer() != "") {
                        CheckIsSinglePE checker(consumer, func_name, loops_to_serialize);
                        s.accept(&checker);
                        if (checker.is_single_PE) {
                            funcs_outputting_to_mem_channels.insert(func_name);
                            funcs_using_mem_channels[func_name] = Place::Host;
                            funcs_using_mem_channels[consumer] = Place::Device;
                            debug(2) << "Find Func output to mem channel\n"
                                     << "Producer: " << func_name << "\n"
                                     << "Consumer: " << consumer << "\n";
                        }
                    }
                }
            }
        }
    }
    // std::map<std::string, bool> used_in_producer;
    // std::map<std::string, bool> used_in_consumer;
    // std::map<std::string, bool> used_in_realize;
    // ProducerConsumerChecker checker(used_in_producer, used_in_consumer, used_in_realize);
    // s.accept(&checker);
    // set<string> funcs_to_mem_channels;
    // for (size_t i = 0; i < funcs_outputting_to_mem_channels.size(); i++) {
    //     std::string name = funcs_outputting_to_mem_channels[i];
    //     if ((used_in_producer.find(name) != used_in_producer.end())
    //             && (used_in_consumer.find(name) != used_in_consumer.end())) {
    //         funcs_to_mem_channels.insert(name);
    //         // debug(2) << name << "\n";
    //     }
    // }

    return funcs_outputting_to_mem_channels;
}

// The __fpga_reg function is useful to "Break the critical paths between spatially distant portions of a data path,
// such as between processing elements of a large systolic array." We check the arguments' differences in the nested
// write/read_shift_regs calls, which hints the data path across PEs:
// write_shift_regs("X.shreg", iii, jjj, read_shift_regs("X.shreg", iii-1, jjj)), iii and jjj are space loops
// The value of X is propogated along the iii dimension. After transformation, a fpga_reg call is inserted:
// write_shift_regs("X.shreg", iii, jjj, fpga_reg( read_shift_regs("X.shreg", iii-1, jjj) ))
class RegCallInserter : public IRMutator {
    using IRMutator::visit;
    vector<Expr> write_args;
    vector<string> space_loops;

    class FindSpaceVar : public IRVisitor {
        using IRVisitor::visit;
        string var;

    public:
        bool found = false;
        FindSpaceVar(string &_s)
            : var(_s) {}

        void visit(const Variable *v) override {
            if (extract_last_token(v->name) == var) {
                found = true;
            }
        }
    };
public:
    RegCallInserter(vector<string> &_s)
        : space_loops(_s) {}

    Expr visit(const Call *op) override {
        if (op->is_intrinsic(Call::write_shift_reg)) {
            write_args = op->args;
            vector<Expr> new_args;
            for (size_t i = 0; i < op->args.size(); i++) {
                new_args.push_back(mutate(op->args[i]));
            }
            write_args.clear();
            return Call::make(op->type, op->name, new_args, op->call_type);
        }
        if (op->is_intrinsic(Call::read_shift_reg) && write_args.size() > 0) {
            for (auto &v : space_loops) {
                FindSpaceVar spv(v);
                size_t iw = 1, end_w = write_args.size()-1;
                size_t ir = 1, end_r = op->args.size();
                spv.found = false;
                for (; iw < end_w; iw++) {
                    write_args[iw].accept(&spv);
                    if (spv.found) {
                        break;
                    }
                }
                spv.found = false;
                for (; ir < end_r; ir++) {
                    op->args[ir].accept(&spv);
                    if (spv.found) {
                        break;
                    }
                }
                if (iw < end_w && ir < end_r) {
                    // Both iw (write) and ir (read) refer to one of the space loop
                    // So we compare them and insert a fpga_reg if different
                    Expr diff = simplify(write_args[iw] - op->args[ir]);
                    if (!is_zero(diff)) {
                        return Call::make(op->type, "fpga_reg", { op }, Call::PureIntrinsic);
                    }
                }
            }
        }
        return IRMutator::visit(op);
    }
};

Stmt replace_references_with_channels(Stmt s, const map<string, Function> &env) {
    LoopBounds global_bounds;
    return replace_references_with_channels(s, env, global_bounds);
}

Stmt replace_references_with_channels(Stmt s, const std::map<std::string, Function> &env,
                                             const LoopBounds &global_bounds) {
    map<string, vector<string>> call_graph = build_call_graph(env); // a function to all the functions it calls.
    map<string, vector<string>> reverse_call_graph = build_reverse_call_graph(call_graph); // a function to the all the functions calling it.
    map<string, Region> channel2bounds;
    map<string, vector<Expr>> channel2write_args;
    map<string, vector<Expr>> channel2read_args;
    identify_funcs_outputting_to_channels(s, env, reverse_call_graph, global_bounds, channel2bounds, channel2write_args, channel2read_args);

    ReplaceReferencesWithChannels replacer(channel2bounds, channel2write_args, channel2read_args, env);
    s = replacer.mutate(s);
    return s;
}

Stmt replace_references_with_mem_channels(Stmt s, const std::map<std::string, Function> &env,
                                        map<string, Place> &funcs_using_mem_channels,
                                        vector<std::pair<string, Expr>> &letstmts_backup) {
    map<string, vector<string>> call_graph = build_call_graph(env); // a function to all the functions it calls.
    map<string, vector<string>> reverse_call_graph = build_reverse_call_graph(call_graph); // a function to the all the functions calling it.
    vector<string> loops_to_serialize;
    find_loops_to_serialize_by_scattering(env, loops_to_serialize);
    find_loops_to_serialize_by_gathering(env, loops_to_serialize);
    set<string> mem_channels = identify_funcs_outputting_to_mem_channels(s, env, reverse_call_graph,
                                                                         funcs_using_mem_channels, loops_to_serialize);
    ReplaceReferencesWithMemChannels replacer(mem_channels, env, letstmts_backup);
    s = replacer.mutate(s);
    return s;
}

Stmt replace_references_with_shift_registers(Stmt s, const map<string, Function> &env, const std::map<std::string, RegBound > &_reg_size_map) {
    // So far, the shift registers are still referenced as common memory like
    //    C(s.kkk, s.jj, s.ii, (15 - t), 0, 0, 0, 0, 0)
    // We would like to change it into something like
    //    read_shift_reg("C.shreg", s.kkk, s.jj, s.ii, (15 - t))
    // or write_shit_reg(...).
    // For now, we do this only for device funcs.
    // TODO: do the changes for host funcs as well. Or just remove write/read_shift_reg intrincs
    // and treat them as normal memory, for all targets.
    std::map<std::string, RegBound > reg_size_map;
    for (auto r : _reg_size_map) {
        Function func;
        if (function_is_in_environment(r.first, env, func) && func.place() == Place::Device) {
            reg_size_map[r.first] = r.second;
        }
    }

    ReplaceReferencesWithShiftRegisters replacer(reg_size_map);
    s = replacer.mutate(s);
    return s;
}

Stmt insert_fpga_reg(Stmt s, const map<string, Function> &env) {
    vector<string> space_loops;
    for (auto &kv : env) {
        if (kv.second.definition().schedule().transform_params().size() > 0) {
            // Only the space loops are needed to identify spatially distant PEs
            const auto &params = kv.second.definition().schedule().transform_params();
            const auto &dst_vars = params[0].dst_vars;
            std::copy(dst_vars.begin(), dst_vars.end()-1, std::back_inserter(space_loops));
            // Trick: in double buffer template, space loop is annotated with suffix "buf"
            space_loops.push_back("buf");
            break;
        }
    }
    RegCallInserter inserter(space_loops);
    s = inserter.mutate(s);
    return s;
}

}
}
