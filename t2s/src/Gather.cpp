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
#include <algorithm>
#include <set>
#include <string>
#include <queue>

#include "Gather.h"
#include "DebugPrint.h"
#include "../../Halide/src/Func.h"
#include "../../Halide/src/Function.h"
#include "../../Halide/src/Definition.h"
#include "../../Halide/src/Expr.h"
#include "../../Halide/src/IREquality.h"
#include "../../Halide/src/IRMutator.h"
#include "../../Halide/src/IRVisitor.h"
#include "../../Halide/src/Schedule.h"
#include "../../Halide/src/Simplify.h"
#include "../../Halide/src/Substitute.h"
#include "../../Halide/src/Util.h"
#include "./BuildCallRelation.h"

namespace Halide{
using namespace Internal;
Func &Func::gather(Func f, VarOrRVar loop, GatherStrategy strategy) {
    user_assert(this->defined() && !this->has_update_definition())
        << GATHER_ERROR_MESSAGE(f.name(), name(), loop.name())
        << "can only support gathering defined Func without updated definition.\n";
    user_assert(f.defined())
        << GATHER_ERROR_MESSAGE(f.name(), name(), loop.name())
        << "cannot gather undefined Func.\n";
    user_assert(func.definition().schedule().gather_params().empty())
        << GATHER_ERROR_MESSAGE(f.name(), name(), loop.name())
        << "can only support gathering once for each Func.\n";
    // just record args, gather in lower
    vector<GatherItem>& gather_params = func.definition().schedule().gather_params();
    gather_params.push_back(GatherItem(f.name(), loop.name(), strategy));
    return *this;
};
struct VarOrRVar;
namespace Internal{

using std::string;
using std::map;
using std::pair;
using std::vector;
using std::queue;

class TestGathering : public IRVisitor{
    using IRVisitor::visit;
    const std::map<std::string, Function> &env;
    string caller_name;
    string func_name;
    string gather_loop_name;
    GatherStrategy strategy;

    // we are in producer of A
    bool in_gather_func;

    void visit(const For *op) override{
        // find gather loop
        debug(4) << "Gathering : test assumption in For " << op->name << ".\n";
        if(in_gather_func && ends_with(op->name, "." + gather_loop_name)) {
            user_assert(op->for_type == ForType::Unrolled)
                << GATHER_ERROR_MESSAGE(func_name, caller_name, gather_loop_name)
                << "we can only gather unloop or vectorized loop.\n";
            assert(found_loop == nullptr);
            found_loop = op;
        }
        if(in_gather_func && op->for_type == ForType::Unrolled){
            unroll_names.push_back(op->name);
            const IntImm* _min = op->min.as<IntImm>();
            const IntImm* _extent = op->extent.as<IntImm>();
            user_assert(_min && _extent)
                << GATHER_ERROR_MESSAGE(func_name, caller_name, gather_loop_name)
                << "can't get unroll loop min and extend\n";
            unroll_min_extents.push_back(pair<int, int>(_min->value, _extent->value));
        }
        IRVisitor::visit(op);
        return ;
    }

    void visit(const IfThenElse *op) override{
        if(in_gather_func){
            Expr origin_found_call = found_call;
            IRVisitor::visit(op);
            // if not, found_call should have recorded the read_channel
            user_assert(equal(origin_found_call, found_call))
                << GATHER_ERROR_MESSAGE(func_name, caller_name, gather_loop_name)
                << func_name + " is constrained by a IfThenElse stmt, "
                << "which is unsupported in gather()\n";
        }
        IRVisitor::visit(op);
        return ;
    }

    void visit(const Select *op) override{
        if(in_gather_func){
            Expr origin_found_call = found_call;
            IRVisitor::visit(op);
            // if not, found_call should have recorded the read_channel
            user_assert(equal(origin_found_call, found_call))
                << GATHER_ERROR_MESSAGE(func_name, caller_name, gather_loop_name)
                << func_name + " is constrained by a Select stmt, "
                << "which is unsupported in gather()\n";
        }
        IRVisitor::visit(op);
        return ;
    }

    void visit(const Call *op) override{
        debug(4) << "Gathering : test assumption in Call " << op->name << ".\n";
        if (in_gather_func && op->is_intrinsic(Call::read_channel)){
            const StringImm* channel_name = op->args[0].as<StringImm>();
            assert(channel_name);
            if(ends_with(channel_name->value, func_name + ".channel")){
                user_assert(!found_call.defined())
                    << GATHER_ERROR_MESSAGE(func_name, caller_name, gather_loop_name)
                    << func_name + " appears multi times in " + caller_name
                    << ".(which haven't been supported by now) \n";
                found_call = op;
            }
        }else if(in_gather_func && op->is_intrinsic(Call::write_channel)){
            const StringImm* channel_name = op->args[0].as<StringImm>();
            assert(channel_name);
            bool not_found = !(found_call.defined());
            if(ends_with(channel_name->value,caller_name+".channel")){
                IRVisitor::visit(op);
                if(not_found && found_call.defined()){
                    write_channel_stmt = Evaluate::make(op);
                }
                return;
            }
        }
        IRVisitor::visit(op);
        return ;
    }

    void visit(const Load *op) override{
        debug(4) << "Gathering : test assumption in load " << op->name << ".\n";
        if (in_gather_func && op->name == func_name){
            user_assert(!found_call.defined())
                << GATHER_ERROR_MESSAGE(func_name, caller_name, gather_loop_name)
                << func_name + " appears multi times in " + caller_name
                << ".(which haven't been supported by now) \n";
            found_call = op;
        }
        IRVisitor::visit(op);
        return ;
    }

    void visit(const Store *op) override{
        if (in_gather_func && op->name == caller_name){
            bool not_found = !(found_call.defined());
            IRVisitor::visit(op);
            if(not_found && found_call.defined()){
                write_channel_stmt = op;
            }
            return;
        }
        IRVisitor::visit(op);
    }

    void visit(const Shuffle *op) override{
        if (in_gather_func && !op->vectors.empty()){
            const Call* arg0 = op->vectors[0].as<Call>();
            if(arg0->is_intrinsic(Call::read_channel)){
                const StringImm* channel_name = arg0->args[0].as<StringImm>();
                assert(channel_name);
                if(ends_with(channel_name->value, func_name + ".channel.0")){
                    user_assert(!found_call.defined())
                        << GATHER_ERROR_MESSAGE(func_name, caller_name, gather_loop_name)
                        << func_name + " appears multi times in " + caller_name
                        << ".(which haven't been supported by now) \n";
                    found_call = op;
                }
            }
        }
        IRVisitor::visit(op);
        return ;
    }

    // locate the caller, also, if there is a vectorized loop?
    void visit(const ProducerConsumer *op) override{
        debug(4) << "Gathering : test assumption in ProducerConsumer " << op->name << ".\n";
        if (op->is_producer && op->name == caller_name){
            debug(4) << "Gathering : enter producer body\n";
            in_gather_func = true;
            IRVisitor::visit(op);
            const vector<Dim> &dims = env.find(op->name)->second.definition().schedule().dims();
            for (size_t i = 0; i < dims.size(); i++) {
                // found vectorized loop
                if ((dims[i].var == gather_loop_name || ends_with(dims[i].var, "." + gather_loop_name)) &&
                    dims[i].for_type == ForType::Vectorized){
                    assert(found_call.defined() && found_loop == nullptr);
                    vec_loop_name = dims[i].var;
                }
            }
            in_gather_func = false;
        } else {
            IRVisitor::visit(op);
        }
        return ;
    }

public:
    Expr found_call;
    // Expr write_channel_node;
    Stmt write_channel_stmt;
    const For* found_loop;
    string vec_loop_name;
    vector<string> unroll_names;
    vector<pair<int, int>> unroll_min_extents;
    TestGathering(string _caller_name, string _loop_name, string _func_name, GatherStrategy _strategy, const std::map<std::string, Function> &_env)
        :env(_env), caller_name(_caller_name), func_name(_func_name), gather_loop_name(_loop_name), strategy(_strategy),
         in_gather_func(false), found_call(Expr()), found_loop(nullptr), vec_loop_name(""){
             unroll_names.clear();
             unroll_min_extents.clear();
    }
};

// Intermediate struct to record all the information needed in gathering.
class GatherArgInfo{
public:
    string func_name;
    Expr call_node;
    Stmt write_channel_node;
    const For* gather_loop;
    string vec_loop_name;
    const vector<string> unroll_names;
    const vector<pair<int, int>> unroll_min_extents;
    GatherStrategy strategy;
    GatherArgInfo(string _func_name, Expr node, Stmt _write_channel_node, const For* loop, string _vec_loop_name,vector<string> &_unroll_names,
                  vector<pair<int, int>>& _unroll_min_extents, GatherStrategy _strategy)
        :func_name(_func_name), call_node(node), write_channel_node(_write_channel_node),gather_loop(loop), vec_loop_name(_vec_loop_name),
         unroll_names(_unroll_names), unroll_min_extents(_unroll_min_extents), strategy(_strategy){}
};

using GatherArgs=map<string,GatherArgInfo>;

class DataGathering : public IRMutator{
    using IRMutator::visit;
    // key is caller_name
    const map<string, GatherArgInfo> gather_arg_info;
    string wait_insert_device_loop;
    // producer name
    string producer_name;
    int loop_level;
    Stmt write_node;
    Stmt visit(const For *op) override{
        debug(4) << "Gathering : enter For " << op->name << ".\n";
        loop_level++;
        int loop_now = loop_level;
        auto iter = gather_arg_info.find(producer_name);
        if(iter == gather_arg_info.end() || iter->second.vec_loop_name != "")
            return Stmt(op);
        Stmt updated_body = mutate(op->body);

        if(loop_now == loop_level){
            bool if_gathering_vec_loop = iter->second.vec_loop_name != "";

            // Basic Exprs
            Expr origin_loop_var = Variable::make(Int(32), if_gathering_vec_loop ?
                                                           producer_name + ".s0.gather." + iter->second.vec_loop_name :
                                                           iter->second.gather_loop->name);
            Expr origin_read_shreg = iter->second.call_node;
            string channel_name = producer_name + "_gather_" + iter->second.func_name + ".shreg";
            vector<Expr> channel_args(1, Expr(channel_name));
            for(auto t : iter->second.unroll_names){
                channel_args.push_back(Variable::make(Int(32), t));
            }
            bool strategy_up = iter->second.strategy == GatherStrategy::Up || iter->second.strategy == GatherStrategy::FPGAReg;
            int origin_loop_extent = if_gathering_vec_loop ?
                                     iter->second.call_node.type().lanes() :
                                     iter->second.gather_loop->extent.as<IntImm>()->value;
            int origin_loop_min = if_gathering_vec_loop?
                                    0:
                                    iter->second.gather_loop->min.as<IntImm>()->value;

            // if (t == ii)
            //     A[ii] = origin_read_shreg(ii, ...) from main kernel
            // else
            //  if(ii > 0)/(ii < II - 1) for GatherStrategy::Down
            //     A[ii] = read_shreg(FIFO_A, ii - 1, ...)/(read_shreg(FIFO_A, ii + 1, ...)
            // if (ii == II-1)/(ii == 0) for GatherStrategy::Down
            //      write A[ii] to output[t]

            Expr gather_var =  Variable::make(Int(32), iter->second.gather_loop->name + ".gather");

            // read_shreg(FIFO_A, ii - 1, ...)/read_shreg(FIFO_A, ii + 1, ...) for GatherStrategy::Down
            vector<Expr> read_shreg_args(channel_args);
            Expr read_shreg = Call::make(iter->second.call_node.type(), Call::read_shift_reg, read_shreg_args, Call::Intrinsic);
            Expr read_shreg_modify = Expr();
            if(strategy_up)
                read_shreg_modify = substitute(origin_loop_var, Sub::make(origin_loop_var, 1), read_shreg);
            else
                read_shreg_modify = substitute(origin_loop_var, Add::make(origin_loop_var, 1), read_shreg);


            //  if(ii > 0)/(ii < II - 1) for GatherStrategy::Down
            //     A[ii] = read_shreg(FIFO_A, ii - 1, ...)/(read_shreg(FIFO_A, ii + 1, ...)
            Expr cond_read = strategy_up ? GT::make(origin_loop_var, 0) : LT::make(origin_loop_var, origin_loop_extent - 1);
            vector<Expr> write_channel_args(channel_args);
            write_channel_args.push_back(read_shreg_modify);
            Stmt write_channel = Evaluate::make(Call::make(iter->second.call_node.type(),
                                                Call::write_shift_reg,
                                                write_channel_args,
                                                Call::Intrinsic));
            Stmt read_from_fifo = IfThenElse::make(cond_read, write_channel);

            // if (t == ii)
            //     A[ii] = origin_read_shreg(ii, ...) from main kernel
            // else
            //  if(ii > t)/(ii < t) for GatherStrategy::Down
            //     A[ii] = read_shreg(FIFO_A, ii - 1, ...)/(read_shreg(FIFO_A, ii + 1, ...)
            if (iter->second.strategy == GatherStrategy::FPGAReg) {
                Stmt pipeline_reg = Provide::make(iter->second.func_name + ".gather.temp", {Call::make(iter->second.call_node.type(), Call::fpga_reg, {Call::make(iter->second.call_node.type(),iter->second.func_name+".gather.temp",{},Call::PureIntrinsic)}, Call::Intrinsic)}, {});
                read_from_fifo = IfThenElse::make(EQ::make(gather_var, origin_loop_var),
                                                  Provide::make(iter->second.func_name + ".gather.temp",{iter->second.call_node},{}),
                                                  pipeline_reg);
            } else {
                read_from_fifo = IfThenElse::make(EQ::make(gather_var, origin_loop_var),
                                                  substitute(read_shreg_modify, origin_read_shreg, write_channel),
                                                  read_from_fifo);
            }


            // if (ii == II-1)/(ii == 0) for GatherStrategy::Down
            //      write A[ii] to output[t]
            Expr origin_read_shreg_t = substitute(origin_loop_var, gather_var, origin_read_shreg);
            // Stmt write_consumer_t = substitute(origin_loop_var, gather_var, updated_body);
            // Expr cond_write = EQ::make(origin_loop_var, strategy_up ? origin_loop_extent - 1 : 0);
            // Stmt write_to_consumer = IfThenElse::make(cond_write,
            //                                           substitute(origin_read_shreg_t, read_shreg, write_consumer_t));
            
            write_node = iter->second.write_channel_node;
            Stmt write_to_consumer = Stmt();
            if(iter->second.unroll_names.back() == iter->second.gather_loop->name){
                // write_to_consumer = substitute(write_node, const_true(), updated_body);
                write_node = substitute(origin_loop_var,gather_var,write_node);
                if (iter->second.strategy == GatherStrategy::FPGAReg) {
                    write_node = substitute(origin_read_shreg_t,Call::make(iter->second.call_node.type(),iter->second.func_name+".gather.temp",{},Call::PureIntrinsic),write_node);
                } else {
                    write_node = substitute(origin_read_shreg_t,read_shreg,write_node);
                }
                if(strategy_up)
                    write_node = substitute(origin_loop_var,op->min + op->extent - 1,write_node);
                else
                    write_node = substitute(origin_loop_var,op->min, write_node);
                // if(write_node.as<Store>()){
                //     const Store* wn = write_node.as<Store>();
                //     Expr new_index = substitute(op->name,gather_var,wn->index);
                //     write_node = Store::make(wn->name,wn->value,new_index,wn->param,wn->predicate,wn->alignment);
                // }
                updated_body = read_from_fifo;
            }else{
                Stmt write_consumer_t = substitute(origin_loop_var, gather_var, updated_body);
                Expr cond_write = EQ::make(origin_loop_var, strategy_up ? origin_loop_min + origin_loop_extent - 1 : origin_loop_min);
                write_to_consumer = IfThenElse::make(cond_write,
                                                        substitute(origin_read_shreg_t, read_shreg, write_consumer_t));
                updated_body = Block::make(read_from_fifo, write_to_consumer);
            }

            // updated_body = Block::make(read_from_fifo, write_to_consumer);
        }

        if (ends_with(op->name, ".run_on_device") && iter->second.strategy == GatherStrategy::FPGAReg) {
            Region ranges;
            updated_body = Realize::make(iter->second.func_name + ".gather.temp",{iter->second.call_node.type()},MemoryType::Auto,ranges,const_true(),updated_body);
        }

        updated_body = For::make(op->name, op->min, op->extent, op->for_type, op->device_api, updated_body);

        // find gather loop
        if(iter != gather_arg_info.end() && iter->second.gather_loop == op){
            debug(4) << "Gathering : insert gather loop " + op->name + ".gather.\n";
            if(iter->second.unroll_names.back() == op->name){
                updated_body = Block::make(updated_body,write_node);
            }
            updated_body =  For::make(op->name + ".gather", op->min, op->extent, ForType::Serial, op->device_api, updated_body);
        // insert buffer
        } else if (ends_with(op->name, ".run_on_device") && wait_insert_device_loop != ""){
            // updated_body = For::make(wait_insert_device_loop, 0, 1, ForType::Parallel, DeviceAPI::OpenCL, updated_body);
            updated_body = For::make(wait_insert_device_loop, 0, 1, ForType::Parallel, op->device_api, updated_body);
            wait_insert_device_loop = "";
        }
        return updated_body;
    }

    // just to locate the caller
    Stmt visit(const ProducerConsumer *op) override{
        debug(4) << "Gathering : enter ProducerConsumer " << op->name << ".\n";
        if (op->is_producer){
            producer_name = op->name;
            Stmt updated_body = op->body;
            debug(4) << "insert Realize Node.\n";
            for(auto item : gather_arg_info){
                if(item.first == producer_name){
                    string channel_name = item.first + "_gather_" + item.second.func_name;
                    vector<Range> range_args;
                    for(auto t : item.second.unroll_min_extents){
                        range_args.push_back(Range(t.first, t.second));
                    }
                    if(item.second.vec_loop_name != ""){
                        range_args.push_back(Range(0, item.second.call_node.type().lanes()));
                    }
                    wait_insert_device_loop = channel_name + ".run_on_device";
                    if (item.second.strategy != GatherStrategy::FPGAReg) {
                        updated_body = Realize::make(channel_name + ".shreg",
                                                    {item.second.vec_loop_name != "" ?
                                                        item.second.call_node.type().element_of() :
                                                        item.second.call_node.type()},
                                                    MemoryType::Auto,
                                                    range_args,
                                                    const_true(),
                                                    updated_body);
                    }
                }
            }
            updated_body = mutate(updated_body);
            producer_name = "";
            return ProducerConsumer::make(op->name, op->is_producer, updated_body);
        }
        Stmt updated_body = mutate(op->body);
        return ProducerConsumer::make(op->name, op->is_producer, updated_body);
    }

public:
    DataGathering(map<string, GatherArgInfo>& _gather_arg_info)
        :gather_arg_info(_gather_arg_info), wait_insert_device_loop(""), producer_name(""), loop_level(0){}
};

class GatherLoopGroup{
public:
    vector<string> unroll_loops;
    set<int> gather_loops_index;
    set<string> gather_loops;
    unsigned int gather_index;
    GatherLoopGroup(vector<string>& _unroll_loops): gather_index(1 << 20){
        unroll_loops.assign(_unroll_loops.begin(), _unroll_loops.end());
        gather_loops.clear();
        gather_loops_index.clear();
    }
};

// to find which unroll loop of which producer should be convert to serial loop
class CollectGatherLoop : public IRVisitor{
    using IRVisitor::visit;
    map<string, GatherArgInfo> gather_arg_groups;
    vector<string> unroll_loop;
    set<string> gather_loop;
    set<string> producer_name;

    void visit(const For *op) override{
        if(op->for_type == ForType::Unrolled && !producer_name.empty())
            unroll_loop.push_back(split_string(op->name, ".").back());
        IRVisitor::visit(op);
        return ;
    }

    void visit(const ProducerConsumer *op) override{
        if(op->is_producer){

            producer_name.insert(op->name);
            IRVisitor::visit(op);

            GatherLoopGroup group(unroll_loop);
            group.gather_loops.insert(gather_loop.begin(), gather_loop.end());
            for(string item : group.gather_loops){
                auto index_iter = std::find(group.unroll_loops.begin(), group.unroll_loops.end(), item);
                if(index_iter != group.unroll_loops.end())
                    group.gather_loops_index.insert(index_iter - group.unroll_loops.begin());
            }
            auto iter = gather_arg_groups.find(op->name);
            if(iter != gather_arg_groups.end() && iter->second.vec_loop_name == ""){
                string temp_name = split_string(iter->second.gather_loop->name, ".").back();
                auto index_iter = std::find(group.unroll_loops.begin(), group.unroll_loops.end(), temp_name);
                assert(index_iter != group.unroll_loops.end());
                group.gather_index = index_iter - group.unroll_loops.begin();
            }
            loop_info.insert(make_pair(op->name, group));

            producer_name.erase(op->name);
            if(producer_name.empty())
                unroll_loop.clear();

        } else {
            auto iter = gather_arg_groups.find(op->name);
            if(iter != gather_arg_groups.end()){
                if(iter->second.vec_loop_name == "")
                    gather_loop.insert(split_string(iter->second.gather_loop->name, ".").back());
                else
                    gather_loop.insert(split_string(iter->second.vec_loop_name, ".").back());
            }

            IRVisitor::visit(op);

            if(iter != gather_arg_groups.end()){
                if(iter->second.vec_loop_name == "")
                    gather_loop.erase(split_string(iter->second.gather_loop->name, ".").back());
                else
                    gather_loop.erase(split_string(iter->second.vec_loop_name, ".").back());
            }
        }
    }

public:
    map<string, GatherLoopGroup> loop_info;
    CollectGatherLoop(map<string, GatherArgInfo>& _gather_arg_groups)
    : gather_arg_groups(_gather_arg_groups){
        unroll_loop.clear();
        gather_loop.clear();
        producer_name.clear();
        loop_info.clear();
    }
};

// convert unroll loop to serial loop as well as simplify the relevant channels
class ModifyGatherLoop : public IRMutator{
    using IRMutator::visit;
    map<string, GatherLoopGroup> loop_info;
    string producer_name;

    Stmt visit(const For *op) override{
        auto iter = loop_info.find(producer_name);
        if(iter != loop_info.end() && iter->second.gather_loops.count(split_string(op->name, ".").back()) > 0)
            return For::make(op->name, op->min, op->extent, ForType::Serial, op->device_api, mutate(op->body));
        return For::make(op->name, op->min, op->extent, op->for_type, op->device_api, mutate(op->body));
    }

    Expr visit(const Call *op) override{
        if(op->is_intrinsic(Call::read_channel) || op->is_intrinsic(Call::write_channel) ||
           op->is_intrinsic(Call::read_shift_reg) || op->is_intrinsic(Call::write_shift_reg)){
            const StringImm* stringImm = op->args[0].as<StringImm>();
            assert(stringImm);
            string channel_name;
            bool if_channel = op->is_intrinsic(Call::read_channel) || op->is_intrinsic(Call::write_channel);
            if(if_channel){
                channel_name = split_string(stringImm->value, ".")[0];
            } else {
                channel_name = split_string(stringImm->value, "_gather_")[0];
            }
            auto iter = loop_info.find(channel_name);
            if(iter != loop_info.end() && (!iter->second.gather_loops_index.empty() || iter->second.gather_index != 1 << 20)){
                vector<Expr> args(1, op->args[0]);
                unsigned int index = 1;
                unsigned int shift = 1;
                if(op->is_intrinsic(Call::write_channel)){
                    args.push_back(mutate(op->args[index++]));
                    shift = 2;
                }
                while(index < op->args.size()){
                    if(iter->second.gather_loops_index.count(index - shift)
                    ||(if_channel && iter->second.gather_index == index - shift)){
                        index++;
                    } else {
                        args.push_back(mutate(op->args[index++]));
                    }
                }
                return Call::make(op->type, op->name, args, op->call_type, op->func, op->value_index, op->image, op->param);
            }
            return IRMutator::visit(op);
        }
        return IRMutator::visit(op);
    }

    Stmt visit(const Realize *op) override{
        string channel_name = "";
        if(ends_with(op->name, ".channel")){
            channel_name = split_string(op->name, ".")[0];
        } else if (ends_with(op->name, ".shreg")){
            channel_name = split_string(op->name, "_gather_")[0];
        }
        auto iter = loop_info.find(channel_name);
        if(iter != loop_info.end() && (!iter->second.gather_loops_index.empty() || iter->second.gather_index != 1 << 20)){
            unsigned int index = 0;
            vector<Range> ranges;
            if(ends_with(op->name, ".channel")){
                assert(op->bounds.size() == iter->second.unroll_loops.size() + 1 ||
                       iter->second.gather_index == 1 << 20);
            } else {
                assert(op->bounds.size() == iter->second.unroll_loops.size());
            }
            while(index < op->bounds.size()){
                if(iter->second.gather_loops_index.count(index)
                    || (ends_with(op->name, ".channel") && iter->second.gather_index == index)){
                    index++;
                } else {
                    ranges.push_back(op->bounds[index++]);
                }
            }
            return Realize::make(op->name, op->types, op->memory_type, ranges, mutate(op->condition), mutate(op->body));
        }
        return IRMutator::visit(op);
    }

    Stmt visit(const ProducerConsumer *op) override{
        if(op->is_producer){
            producer_name = op->name;
            Stmt updated_body = mutate(op->body);
            producer_name = "";
            return ProducerConsumer::make(op->name, op->is_producer, updated_body);
        }
        return ProducerConsumer::make(op->name, op->is_producer, mutate(op->body));
    }

public:
    ModifyGatherLoop(map<string, GatherLoopGroup>& _loop_info)
    :loop_info(_loop_info), producer_name(""){}
};

class ModifyGatherLoop_new : public IRMutator{
    using IRMutator::visit;
    const map<string, set<string>>& loop_info;
    const set<string>& channel_names;
    string producer_name;
    vector<string> for_names;
    vector<Expr> for_mins;
    // vector<Expr> for_extents;

    Stmt visit(const For *op) override{
        auto iter = loop_info.find(producer_name);
        string  for_name = split_string(op->name, ".").back();
        if(iter != loop_info.end() && iter->second.find(for_name) != iter->second.end()){
            for_names.push_back(op->name);
            for_mins.push_back(op->min);
            return For::make(op->name, op->min, op->extent, ForType::Serial, op->device_api, mutate(op->body));
        }
        return For::make(op->name, op->min, op->extent, op->for_type, op->device_api, mutate(op->body));
    }
    Expr visit(const Call *op) override{
        if(producer_name == "")
            return IRMutator::visit(op);
        if(op->is_intrinsic(Call::read_channel) || op->is_intrinsic(Call::write_channel) ||
            op->is_intrinsic(Call::read_channel_nb) || op->is_intrinsic(Call::write_channel_nb)){         
            const StringImm* stringImm = op->args[0].as<StringImm>();
            assert(stringImm);
            string channel_name = stringImm->value;
            auto iter = channel_names.find(channel_name);

            //we don't need to change the parameters of this channel 
            if(iter == channel_names.end()){
                return IRMutator::visit(op);
            }
            int size = for_names.size();
            vector<Expr> args(1, op->args[0]);
            unsigned int index = 1;
            if(op->is_intrinsic(Call::write_channel) || op->is_intrinsic(Call::read_channel_nb)){
                args.push_back(mutate(op->args[index++]));
            }
            while(index < op->args.size()){
                auto arg = op->args[index++];
                for(int i = 0;i < size;++i){
                    arg = substitute(for_names[i],for_mins[i],arg);
                }
                args.push_back(arg);
            }
            return Call::make(op->type, op->name, args, op->call_type, op->func, op->value_index, op->image, op->param);
        }
        return IRMutator::visit(op);
    }

    Stmt visit(const ProducerConsumer *op) override{
        if(op->is_producer){
            for_names.clear();
            for_mins.clear();
            producer_name = op->name;
            Stmt updated_body = mutate(op->body);
            producer_name = "";
            return ProducerConsumer::make(op->name, op->is_producer, updated_body);
        }
        return ProducerConsumer::make(op->name, op->is_producer, mutate(op->body));
    }



public:
    ModifyGatherLoop_new(map<string, set<string>>& _loop_info, const set<string>& _channel_names)
    :loop_info(_loop_info), channel_names(_channel_names), producer_name(""){}
};

void find_gather_loops(
    const GatherArgs& gather_arg_groups,
    const map<string, vector<string>>& reverse_call_graph,
    map<string, set<string>>& gather_loops,
    set<string>& channel_names){
    for(auto gather_arg : gather_arg_groups){
        string loop_name  = split_string(gather_arg.second.gather_loop->name, ".").back();
        string caller_name = gather_arg.first;
        queue<string> callers;
        {
            auto iter = reverse_call_graph.find(caller_name);
            assert(iter != reverse_call_graph.end());
            for(auto caller : iter->second){
                callers.push(caller);
            }
        }
        while(!callers.empty()){
            string caller = callers.front();
            channel_names.insert(caller);
            callers.pop();
            auto iter = reverse_call_graph.find(caller);
            for(auto new_caller_name : iter->second){
                callers.push(new_caller_name);
            }
            if(gather_loops.find(caller) == gather_loops.end()){
                gather_loops.insert(make_pair(caller,set<string>()));
            }
            gather_loops[caller].insert(loop_name);
        } 
    }
    return;
}

// Suppose func c is isolated out of func p as a consumer, and both funcs are on device. Both funcs have the same
// loop structure, because of isolation. Suppose there is an unrolled loop l with the extent of L,
// and here we want to gather along dimension l in func p. Without gathering, func p and c
// both have L PEs; func p is writing into L channels, and func c reads from these L channels.
// After gathering, all the PEs of func c are collapsed into a single PE; this single PE of func c
// reads from 1 channel, and the last PE of func p writes that channel.
// If another func c1 is isolated out of func c as a consumer, dimension l in func c1 should also be "collapsed",
// since our principle of isolation is that producer and consumer have the same loop structure. In
// general, we need collapse dimension l for the consumer chain isolated out of func p.
// Note: below we do not check if a Func is on device or not. So loops on host can be serializd as well.
void find_loops_to_serialize_by_gathering(const Function &func, const string &gather_loop,
                                          const map<string, Function> &env,
                                          vector<string> &loops_to_serialize) {
    // Find the gather loop in the consumer chain of the func
    for (auto &e : env) {
        const Function &c = e.second;
        if (/*c.place() == Place::Device &&*/ c.isolated_from_as_consumer() == func.name()) {
            loops_to_serialize.push_back(c.name() + ".s0." + gather_loop);
            find_loops_to_serialize_by_gathering(c, gather_loop, env, loops_to_serialize);
            return;
        }
    }
}

void find_loops_to_serialize_by_gathering(const std::map<std::string, Function> &env,
                                          std::vector<std::string> &loops_to_serialize) {
    for (auto e : env) {
        auto &func = e.second;
        const std::vector<GatherItem>& gather_params = func.definition().schedule().gather_params();
        internal_assert(gather_params.size() < 2); // Currently a func can only have zero or one time of gather
        if (gather_params.size() > 0) {
            string gather_loop = gather_params[0].loop_name;
            // Find the gather loop in the  entire consumer chain of func
            find_loops_to_serialize_by_gathering(func, gather_loop, env, loops_to_serialize);
        }
    }
}

Stmt gather_data(Stmt s, const std::map<std::string, Function> &env){
    // get gather arguments and test
    map<string, GatherArgInfo> gather_arg_info;
    for (auto e : env) {
        string caller_name = e.second.name();
        vector<GatherItem>& gather_params = e.second.definition().schedule().gather_params();
        if (!gather_params.empty()){
            auto t = gather_params[0];
            debug(3) << " test for GatherItem " << t.func_name << " "
                     << t.loop_name << " " << ((t.strategy == GatherStrategy::Up || t.strategy == GatherStrategy::FPGAReg) ? "Up" : "Down") << "\n";
            TestGathering ts(caller_name, t.loop_name, t.func_name, t.strategy, env);
            s.accept(&ts);
            user_assert(ts.found_loop != nullptr || ts.vec_loop_name != "")
                << GATHER_ERROR_MESSAGE(t.func_name, caller_name, t.loop_name)
                << "cannot find loop " + t.loop_name + " in " + caller_name
                << ".(which may have been reduced in optimization) \n";
            user_assert(ts.found_call.defined())
                << GATHER_ERROR_MESSAGE(t.func_name, caller_name, t.loop_name)
                << t.func_name + " is not found in " + caller_name + ".\n";

            // record arguments
            assert(gather_arg_info.find(caller_name) == gather_arg_info.end());
            if(ts.vec_loop_name == "")
                gather_arg_info.insert(std::make_pair(caller_name, GatherArgInfo(t.func_name,
                                                                            ts.found_call,
                                                                            ts.write_channel_stmt,
                                                                            ts.found_loop,
                                                                            ts.vec_loop_name,
                                                                            ts.unroll_names,
                                                                            ts.unroll_min_extents,
                                                                            t.strategy)));
        }
    }

    // gather processing
    if (!gather_arg_info.empty()){
        DataGathering ds(gather_arg_info);
        s = ds.mutate(s);
        debug(3) << "Lower after modification innermost loop\n" << s << "\n\n";
        // map<string, vector<string>> call_graph = build_call_graph(env);
        // map<string, vector<string>> reverse_call_graph = build_reverse_call_graph(call_graph);
        // map<string, set<string>> gather_loops;
        // set<string> channel_names;
        // find_gather_loops(gather_arg_info,reverse_call_graph,gather_loops,channel_names);
        // for(auto tmp : gather_loops){
        //     std::cout<<tmp.first<<" : \n";
        //     for(auto tmp2 : tmp.second){
        //         std::cout<<tmp2<<" ";
        //     }
        //     std::cout<<"\n";
        // }
        // ModifyGatherLoop_new mgl(gather_loops, channel_names);
        // s = mgl.mutate(s);
        CollectGatherLoop cs(gather_arg_info);
        s.accept(&cs);
        ModifyGatherLoop ms(cs.loop_info);
        s = ms.mutate(s);
    }


    return s;
}

}
}
