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
#include <set>
#include <string>
#include <queue>
#include <map>
#include "../../Halide/src/Definition.h"
#include "../../Halide/src/Expr.h"
#include "../../Halide/src/Func.h"
#include "../../Halide/src/Function.h"
#include "../../Halide/src/IREquality.h"
#include "../../Halide/src/IRMutator.h"
#include "../../Halide/src/IRVisitor.h"
#include "../../Halide/src/Schedule.h"
#include "../../Halide/src/Simplify.h"
#include "../../Halide/src/Substitute.h"
#include "../../Halide/src/Util.h"
#include "./BuildCallRelation.h"
#include "./DebugPrint.h"
#include "./ScatterAndBuffer.h"
#include "./SliceExprTree.h"
#include "./Utilities.h"

namespace Halide{
Func &Func::buffer(Func f, VarOrRVar loop, BufferStrategy buffer_strategy, BufferReadStrategy read_strategy){
    invalidate_cache();
    user_assert(this->defined()) <<"Func " << this->name() << " is undefined";
    user_assert(f.defined()) <<"Func " << f.name() << " is undefined";
    user_assert(buffer_strategy == BufferStrategy::Double) << "Only BufferStrategy::Double is supported so far.";

    std::vector<Internal::BufferItem> &buffer_params = func.definition().schedule().buffer_params();
    user_assert(buffer_params.empty())
        << "Inserting more than 1 buffer to Func " << func.name() << " is unexpected. We support only one buffer in a function so far\n";
    buffer_params.push_back(Internal::BufferItem(f.name(), loop.name(), buffer_strategy,read_strategy));
    return *this;
}

Func &Func::scatter(Func f, VarOrRVar loop, ScatterStrategy scatter_strategy) {
    invalidate_cache();
    user_assert(this->defined()) <<"Func " << this->name() << " is undefined";
    user_assert(f.defined()) <<"Func " << f.name() << " is undefined";

    std::vector<Internal::ScatterItem>& scatter_params = func.definition().schedule().scatter_params();
    user_assert(scatter_params.size() == 0)
        << "Scattering more than once in Func " << func.name() << " is unexpected. We support only scattering once in a function so far";
    scatter_params.push_back(Internal::ScatterItem(f.name(), loop.name(), scatter_strategy));
    return *this;
}

namespace Internal{

namespace{

using std::map;
using std::set;
using std::string;
using std::queue;

// Info in a function tha buffers and/or scattes data
class ScatterBufferArg{
public:
    string producer = "";          // The Func that sends data to buffer and/or scatter
    string buffer_loop = "";       // Loop under which a buffer is to be inserted. Undocorated name without func and stage prefix, e.g. "x"
    string scatter_loop = "";      // Loop along which data are to be scattered.  Undocorated name without func and stage prefix, e.g. "y"
    Expr read_node = nullptr;      // The expression that reads the data from the producer
    Expr read_condition = nullptr; // Path conditioin to read_node. It is the same condition the producer writes into its output channel.
    BufferStrategy buffer_strategy = BufferStrategy::Double;
    ScatterStrategy scatter_strategy = ScatterStrategy::Up; 

    // Full names, mins, extents and types of the loops around the read node.
    vector<tuple<string, Expr, Expr, ForType>> loops;
};

using ScatterBufferArgs = map<string,ScatterBufferArg>;

/**
 * detect the conditions of "read channel" stmt
 * Now one feeder can only read from loader once.
 * we will only keep all the conds related to the space loop variables.
 * (the reason for that:
 *    When inserting buffers, we need a counter to decide 
 *    which buffer or which position we should write or read in.
 *    We will inc the counter no matter whether reading or writing buffer happens.
 *    so if the cond is like:
 *      if(unroll_jj == constantx){...} or
 *      if(serial_jj == xxx){...}
 *    we can remove these conditions.
 * )
 * If we introduce an empty space_loops, we will keep all the conds
 *    
 */

class CondDetect:public IRVisitor{
    using IRVisitor::visit;
private:
    const vector<string>& space_loops; //we will keep the conds related to these variables
    const Expr& read_channel;
    bool found_space_loop = false;
    bool only_constant_vars = true;
    bool found_read_channel = false;
    Expr tmp_cond;
    bool remove;
    void visit(const IfThenElse *op) override{
        Expr condition = detect_condition(op->condition);
        op->then_case.accept(this);
        if(found_read_channel){
            if(cond.defined())
                cond = condition && cond;
            else
            {
                cond = condition;
            }
            
            if(op->else_case.defined())
                op->else_case.accept(this);
        }else{
            if(op->else_case.defined())
                op->else_case.accept(this);
            if(found_read_channel){
                if(cond.defined())
                    cond = !condition && cond;
                else
                {
                    cond = !condition;
                }
            }
        }
    }
    Expr detect_condition(Expr condition){ //for get the new condition from the IfThenElse*op
        found_space_loop = false;
        only_constant_vars = true;
        tmp_cond = Expr();
        condition.accept(this);
        if(!tmp_cond.defined()){
            tmp_cond = condition;
        }
        if(remove)
            if(!found_space_loop || only_constant_vars){
                tmp_cond = const_true();
            }
        return tmp_cond;
    }
    void visit(const Call* op) override{
        if(op->is_intrinsic(Call::read_channel)){
            if(equal(op, read_channel)){
                user_assert(!found_read_channel)
                    <<"If Afeeder buffer Aloader, Afeeder can only read once from Aloader through channel.\n";
                found_read_channel = true;
            }
        }
        IRVisitor::visit(op);
    }
    void visit(const And* op) override{ 
        Expr newa,newb;
        found_space_loop = false;
        only_constant_vars = true;
        op->a.accept(this);
        newa = op->a;
        if(remove)
            if(!(op->a.as<And>() || op->a.as<Or>())){
                if(!found_space_loop || only_constant_vars){
                    newa = const_true();
                }
            }
        found_space_loop = false;
        only_constant_vars = true;
        op->b.accept(this);
        newb = op->b;
        if(remove)
            if(!(op->b.as<And>() || op->b.as<Or>())){
                if(!found_space_loop|| only_constant_vars){
                    newb = const_true();
                }
            }
        found_space_loop = false;
        only_constant_vars = true;
        tmp_cond = And::make(newa, newb);
    }
    void visit(const Or *op) override{
        Expr newa,newb;
        found_space_loop = false;
        only_constant_vars = true;
        op->a.accept(this);
        newa = op->a;
        if(remove)
            if(!(op->a.as<And>() || op->a.as<Or>())){
                if(!found_space_loop || only_constant_vars){
                    newa = const_true();
                }
            }
        found_space_loop = false;
        only_constant_vars = true;
        newb = op->b;
        op->b.accept(this);
        if(remove)
            if(!(op->b.as<And>() || op->b.as<Or>())){
                if(!found_space_loop || only_constant_vars){
                    newb = const_true();
                }
            }
        found_space_loop = false;
        only_constant_vars = true;
        tmp_cond = Or::make(newa,newb); 
    }
    void visit(const Variable *op) override{
        for(const auto & space_loop : space_loops){
            if(space_loop == op->name){
                found_space_loop = true;
                return;
            }
        }
        only_constant_vars = false;
        return;
    }
public:
    Expr cond;
    CondDetect(const vector<string>& _space_loops, const Expr& _read_channel, bool _remove)
        :space_loops(_space_loops), read_channel(_read_channel), remove(_remove), cond(nullptr){}

};

/*
B = A(oi,oj,ok,kk,ii,jj); //jj is unrolled 
B.isolate_producer_chain(A,Aloader,Afeeder);
Afeeder.buffer(Aloader,ok, BufferStrategy::Double); //buffer loop is ok
Aloader.remove(kk); //the user should guarantee that
                   //the loops outside buffer loop should be the same between Aloader and Afeeder
                   //the loops inner buffer loop should be the same except some removal loops

the code generated is like the following:
 Aloader: 
       ....
 Afeeder:
     realize buffer[2][ii][jj];
     realize counter[jj]; //init is 0
     let read_buffer_num = (kk * ii); 
     let write_buffer_num = ii;
     let block_length = max(read_buffer_num,write_buffer_num);
     let block_num = oi * oj * ok;
     while(1):
        for jj:
            if(cond){ // this cond is the original cond in original Afeeder
                     // in this example, it's const_true
                bool idx = (counter[jj] / block_length)%2; // which buffer to write into
                int buffer_ii = (counter[jj] % block_length); //the pos in the buffer
                int counter_no = counter[jj] / counter_length;
                if(counter_no < block_num && buffer_ii < write_buffer_num){
                    Type tmp = read_channel();
                    write_buffer(tmp, buffer[cast<int>(idx)][buffer_ii][jj]);
                }
                if(counter_no > 0 && counter_no <= block_num && buffer_ii < read_buffer_num){
                    Type tmp = read_buffer(buffer[cast<int>(!idx)][buffer_ii][jj]);
                    write_channel(tmp);
                }
            }
            // we have to inc the counter outside the cond
            // but if there are some conds about space loop (here, jj), we'll keep those conds
            if(counter_no <= block_num)
                counter[j]++;
*/                  
class BufferInserter: public IRMutator{
    using IRMutator::visit;
    const map<string, Function>& envs;
    const ScatterBufferArgs& scatterbuffer_args;
    const string& caller_name;
    int loop_level = 0;
    vector<Expr> loop_mins;
    vector<Expr> loop_extents;
    vector<string> loop_names;
    vector<int> unroll_loop_indexes;
    vector<ForType> for_types;
    Stmt visit(const For *op) override{
        if(ends_with(op->name,".run_on_device")){
            return IRMutator::visit(op);
        }
        loop_names.push_back(op->name);
        for_types.push_back(op->for_type);
        loop_level += 1;
        int this_level = loop_level;
        if(op->for_type == ForType::Unrolled){
            unroll_loop_indexes.push_back(this_level - 1);
        }
        Expr new_min = IRMutator::mutate(op->min);
        loop_mins.push_back(new_min);
        Expr new_extent = IRMutator::mutate(op->extent);
        loop_extents.push_back(new_extent);
        Stmt new_body = IRMutator::mutate(op->body);
        auto iter = scatterbuffer_args.find(caller_name);
        const string& buffer_loop = iter->second.buffer_loop;
        const string& producer = iter->second.producer;
        Type data_type = iter->second.read_node.type();
        debug(4) << "inserting buffer: "<<op->name<<"\n";
        string cycle_name = caller_name + ".cycle.temp";
        if(this_level == loop_level){
            string buffer_name = caller_name + "_buffer_.ibuffer";
            Expr original_condition = Expr();
            {//find the original_condition and data type of the data
                const Call * read_channel = iter->second.read_node.as<Call>();
                CondDetect condDetect(vector<string>(), read_channel,false);
                new_body.accept(&condDetect);
                original_condition = condDetect.cond;
            }
            debug(4)<<"The original condition is "<<original_condition<<"\n";
            
            Expr new_condition = Expr(); 
                //this condition will remove all the conditions like:
                // 1. if(unroll_jj == constantx){...}
                // 2. if(serial_jj == xxx){...} 
            {// generate new condition for inc counter
                const Call * read_channel = iter->second.read_node.as<Call>();
                vector<string> space_loops;
                for(size_t i = 0;i < unroll_loop_indexes.size();++i){
                    int index = unroll_loop_indexes[i];
                    space_loops.push_back(loop_names[index]);
                }
                CondDetect condDetect(space_loops, read_channel,true);
                new_body.accept(&condDetect);
                new_condition = condDetect.cond;
            }
            internal_assert(!new_condition.defined() || equal(new_condition,const_true()))
                <<"condition = "<<new_condition
                <<", now the Afeeder cannot have some complicated conditions.\n";

            debug(4)<<"The new condition is "<<new_condition<<"\n";

            vector<Expr> cycle_args;
                //counter is an array, its same dimension is unroll loops
            {
                for(size_t i = 0;i < unroll_loop_indexes.size();++i){
                    int index = unroll_loop_indexes[i];
                    cycle_args.push_back(
                        Variable::make(Int(32), loop_names[index]) - loop_mins[index]
                    );
                }
            }

            
            

            Expr idx; 
                //which buffer to write for double buffer
            {
                idx = (Variable::make(Int(32),"period") & 1);
            }

            //calculate buffer size and buffer arg (read and write)
            vector<Expr> buffer_args;
            Region buffer_dims;

            //how many times to read or write in one period
            //writes means "write to buffer"
            //read means "read buffer"
            int writes = 1, reads = 1;
            { //calculate buffer_args, buffer_dims, reads, writes
                bool inBufferLoop = false;
                const auto& remove_params = 
                    envs.find(producer)->second.definition().schedule().remove_params();
                buffer_dims.push_back(Range(0,2));
                for(size_t i = 0;i < loop_names.size();++i){
                    if(inBufferLoop){
                        internal_assert(loop_extents[i].as<IntImm>())
                                    <<"the loop bounds inner buffer loop must be constant. "
                                    <<split_string(loop_names[i],".").back() <<" in func "
                                    <<caller_name << " is not.\n";
                        int loop_extent = loop_extents[i].as<IntImm>()->value;
                        bool isRemoved = false;
                        //whether this loop is removed in its producer
                        //if true, this loop is one of buffer dimension
                        for(size_t j = 0;j < remove_params.size();++j){
                            if(ends_with(loop_names[i],"." + remove_params[j])){
                                isRemoved = true;
                                break;
                            }
                        }
                        if(!isRemoved){
                            buffer_args.push_back(Variable::make(Int(32),loop_names[i]));
                            buffer_dims.push_back(Range(loop_mins[i],loop_extents[i]));
                            if(for_types[i] != ForType::Unrolled){
                                writes = writes * loop_extent;
                            }
                        }
                        if(for_types[i] != ForType::Unrolled){
                            reads = reads * loop_extent;
                        }
                    }
                    if(ends_with(loop_names[i],"." + buffer_loop)){
                        inBufferLoop = true;
                    }
                }

                int extent = buffer_dims.back().extent.as<IntImm>()->value;
                int new_extent = 1;
                while(new_extent < extent)
                    new_extent <<= 1;
                buffer_dims.back() = Range(buffer_dims.back().min,new_extent);

                user_assert(reads >= writes)
                    <<"When buffering, the product of removal loop dimensions "
                    <<"must exceed scatter loop dimension. If meeting this condition, "
                    <<"the delay of reading memory can be hiden well\n";
            }

            int init = reads - writes;
            

            Expr period, offset;
                //iter_no : which iter to read and write now
                //iter_pos : read (or write) how many times in this iter now
            {
                period = 
                    Call::make(Int(32),cycle_name, cycle_args, Call::PureIntrinsic) /
                    reads;
                offset =
                    Call::make(Int(32),cycle_name,cycle_args,Call::PureIntrinsic) %
                    reads;
            }

            // this part will change "new_body" stmt, take care
            {// replace reading channel with reading buffer
                vector<Expr> read_buffer_args(buffer_args);
                read_buffer_args.insert(
                    read_buffer_args.begin(),
                    Not::make(Cast::make(Bool(),Variable::make(Int(32),"idx")))
                );
                Expr read_buffer = Call::make(data_type,buffer_name,read_buffer_args,Call::PureIntrinsic);
                new_body = substitute(iter->second.read_node,read_buffer,new_body);

                Expr counter = 
                    Call::make(Int(32),cycle_name,cycle_args,Call::PureIntrinsic) - reads;
                if(!loop_names.empty()){
                    int beg = loop_names.size() - 1;
                    for(int i = beg;i >= 0; --i){
                        if(for_types[i] == ForType::Unrolled)
                            continue;
                        Expr var = (counter % loop_extents[i]) + loop_mins[i];
                        new_body = LetStmt::make(loop_names[i],var,new_body);
                        counter = counter / loop_extents[i];
                    }
                };
                Expr read_buffer_cond = (Variable::make(Int(32),"period") <= Variable::make(Int(32),"period_num")) && 
                                        (Variable::make(Int(32),"period") > 0);
                new_body = IfThenElse::make(read_buffer_cond,new_body);
            }

            // this part will change "new_body" stmt, take care
            {// add read channel and write buffer part
                vector<Expr> write_buffer_args(buffer_args);
                write_buffer_args.insert(write_buffer_args.begin(),Cast::make(Bool(),Variable::make(Int(32),"idx")));
                Stmt write_buffer = Provide::make(buffer_name,{iter->second.read_node},write_buffer_args);
                if(original_condition.defined()){
                    write_buffer = IfThenElse::make(original_condition,write_buffer);
                }
                Expr write_buffer_cond = (Variable::make(Int(32),"period") < Variable::make(Int(32),"period_num")) && 
                                        (Variable::make(Int(32),"offset") >= init);
                Expr counter = 
                    writes * Variable::make(Int(32),"period") + 
                    Variable::make(Int(32), "offset") - init;
                vector<Expr> new_loop_vars;
                vector<string> new_loop_names;
                if(!loop_names.empty()){
                    const auto &remove_params = envs.find(producer)->second.definition().schedule().remove_params();
                    int beg = loop_names.size() - 1;
                    for(int i = beg; i >= 0;--i){
                        if(for_types[i] == ForType::Unrolled)
                            continue;
                        bool isRemove = false;
                        for(size_t j = 0; j < remove_params.size();++j){
                            if(ends_with(loop_names[i], "." + remove_params[j])){
                                isRemove = true;
                                break;
                            }
                        }
                        new_loop_names.push_back(loop_names[i]);
                        if(!isRemove){
                            Expr var = (counter % loop_extents[i]) + loop_mins[i];
                            new_loop_vars.push_back(var);
                            counter = counter / loop_extents[i];
                        }else{
                            new_loop_vars.push_back(loop_mins[i]);
                        }
                        
                    }
                }
                
                for(size_t i = 0;i < new_loop_vars.size(); ++i){
                    write_buffer = LetStmt::make(
                        new_loop_names[i], new_loop_vars[i],write_buffer
                    );
                }
                write_buffer = IfThenElse::make(write_buffer_cond, write_buffer);
                new_body = Block::make(write_buffer, new_body);
            }

            // this part will change "new_body" stmt, take care
            {// add "inc counter" part in the end of new_body
                Stmt inc_counter = Provide::make(cycle_name,{Call::make(Int(32),cycle_name,cycle_args,Call::PureIntrinsic)+1},cycle_args);
                Expr inc_counter_cond = (Variable::make(Int(32),"period") <= Variable::make(Int(32),"period_num"));
                if(new_condition.defined())
                    inc_counter_cond = new_condition && inc_counter_cond;
                inc_counter = IfThenElse::make(inc_counter_cond, inc_counter);
                new_body = Block::make(new_body,inc_counter);
            }
        
            new_body = LetStmt::make("idx",idx,new_body);
            new_body = LetStmt::make("period",period,new_body);
            new_body = LetStmt::make("offset",offset,new_body);

            // remove the serial for loops, only keep the space loop
            for(size_t i = 0;i < unroll_loop_indexes.size();++i){
                int index = unroll_loop_indexes[i];
                new_body = For::make(
                    loop_names[index], loop_mins[index], loop_extents[index],ForType::Unrolled, op->device_api,new_body
                );
            }
                
            Expr period_num;
            // PERIOD in document
            {
                const auto& remove_params = envs.find(producer)->second.definition().schedule().remove_params();
                for(size_t i = 0;i < loop_names.size();++i){
                    for(size_t j = 0;j < remove_params.size();++j){
                        if(ends_with(loop_names[i],"." + remove_params[j])){
                            user_error
                                <<"Removed loop in func : "<<producer
                                <<", cannot outside buffer loop "<<buffer_loop
                                <<"( func "<<caller_name<<" buffer "<<producer
                                <<" at loop "<<buffer_loop<<" )\n";
                        }
                    }
                    if(period_num.defined()){
                        period_num = period_num * loop_extents[i];
                    }else{
                        period_num = loop_extents[i];
                    }
                    if(ends_with(loop_names[i],"."+buffer_loop)){
                        break;
                    }
                }
            }

            new_body = For::make(caller_name+".s0.outermost_loop.infinite",0,10,ForType::Serial,op->device_api,new_body);

            Region cycle_dims;
                //counter is an array, its same dimension is unroll loops
            {
                for(size_t i = 0;i < loop_names.size(); ++i){
                    if(for_types[i] == ForType::Unrolled){
                        cycle_dims.push_back(Range(loop_mins[i],loop_extents[i]));
                    }
                }
            }

            {//init the counters
                Stmt init_counter = Provide::make(cycle_name,{init},cycle_args);
                for(size_t i = 0;i < unroll_loop_indexes.size();++i){
                    int index = unroll_loop_indexes[i];
                    int min = loop_mins[index].as<IntImm>()->value;
                    int extent = loop_extents[index].as<IntImm>()->value;
                    Stmt tmp_stmt;
                    for(int j = min; j < extent;++j){
                        if(tmp_stmt.defined()){
                            tmp_stmt = Block::make(tmp_stmt,substitute(loop_names[index],j,init_counter));
                        }else{
                            tmp_stmt = substitute(loop_names[index],j,init_counter);
                        }
                    }
                    init_counter = tmp_stmt;
                }
                new_body = Block::make(init_counter, new_body);
            }

            // realize counters and buffers 
            new_body = Realize::make(
                buffer_name,{data_type},MemoryType::Auto,buffer_dims,const_true(),new_body
            );
            new_body = Realize::make(
                cycle_name, {Int(32)}, MemoryType::Auto,cycle_dims, const_true(),new_body
            );


            new_body = LetStmt::make(
                "period_num", period_num, new_body
            );
        }
        return new_body;
    }
        
public:
    BufferInserter(
        const map<string, Function>& _envs, 
        const ScatterBufferArgs& _scatterbuffer_args, 
        const string& _caller_name):
    envs(_envs), scatterbuffer_args(_scatterbuffer_args), caller_name(_caller_name){}
};

class ScatterInserter: public IRMutator{
    using IRMutator::visit;
    const map<string, Function>& envs;
    const ScatterBufferArgs& scatterbuffer_args;
    const string& caller_name;
    int loop_level = 0;
    vector<Expr> loop_mins;
    vector<Expr> loop_extents;
    vector<string> loop_names;
    vector<int> unroll_loop_indexes;
    vector<ForType> for_types;
    Stmt visit(const For *op) override{
        auto iter = scatterbuffer_args.find(caller_name);
        const string& producer = iter->second.producer;
        const string& scatter_loop = iter->second.scatter_loop;
        string shreg_name = caller_name + "_scatter_" + producer + ".shreg";
        Type data_type = iter->second.read_node.type();
        if(ends_with(op->name,".run_on_device")){
            Stmt new_body = IRMutator::mutate(op->body);
            new_body = For::make(
                shreg_name.substr(0,shreg_name.size() - string(".shreg").size()) +".run_on_device", 0, 1,
                ForType::Parallel, DeviceAPI::OpenCL, new_body
            );
            new_body = For::make(
                op->name,op->min,op->extent,op->for_type,op->device_api,new_body
            );
            Region range_args;
            for(auto index : unroll_loop_indexes){
                range_args.push_back(Range(loop_mins[index], loop_extents[index]));
            }
            new_body = Realize::make(shreg_name,
                    {data_type},
                    MemoryType::Auto, range_args,
                    const_true(), new_body);
            return new_body;
        }
        loop_level++;
        int loop_now = loop_level;
        loop_names.push_back(op->name);
        for_types.push_back(op->for_type);
        if(op->for_type == ForType::Unrolled){
            unroll_loop_indexes.push_back(loop_now - 1);
        }
        Expr new_min = mutate(op->min);
        loop_mins.push_back(new_min);
        Expr new_extent = mutate(op->extent);
        loop_extents.push_back(new_extent);
        Stmt new_body = mutate(op->body);
        

        if(loop_now == loop_level){ //inner most loop level
            debug(4) << "Scattering : enter innermost loop.\n";
            // Basic Exprs
            bool scatter_along_removed = false;
            {
                const auto &remove_params = envs.find(producer)->second.definition().schedule().remove_params();
                for(size_t i = 0;i < remove_params.size();++i){
                    if(remove_params[i] == scatter_loop){
                        scatter_along_removed = true;
                        break;
                    }
                }
            }
            string origin_loop_name = "";  
            int scatter_loop_extent = 0, scatter_loop_min = 0;
            for(size_t i = 0;i < loop_names.size(); ++i){
                if(ends_with(loop_names[i],"." + scatter_loop)){
                    origin_loop_name = loop_names[i];
                    scatter_loop_extent = loop_extents[i].as<IntImm>()->value;
                    scatter_loop_min = loop_mins[i].as<IntImm>()->value;
                    break;
                }
            }
            string  new_loop_name = origin_loop_name + ".scatter";
            Expr origin_loop_var = Variable::make(Int(32), origin_loop_name);
            Expr scatter_var =  Variable::make(Int(32), new_loop_name);

            
            vector<Expr> shreg_args;
            for(size_t i = 0; i < unroll_loop_indexes.size(); ++i){
                int index = unroll_loop_indexes[i];
                string unroll_loop = loop_names[index];
                shreg_args.push_back(Variable::make(Int(32), unroll_loop));
            }

            bool strategy_up = iter->second.scatter_strategy == ScatterStrategy::Up;

            // if (ii == 0)/(ii == I - 1)
            Expr cond_read = EQ::make(origin_loop_var, strategy_up ? scatter_loop_min : (scatter_loop_min + scatter_loop_extent - 1));

            // if(cond_A)
            //      if (ii == 0)/(ii == I - 1)
            //          feederA[ii] = read inputA(ii->t)
            //      else
            //          feederA[ii] = feederA[ii-1]/feederA[ii+1]
            // if (ii == t)
            //      consume feederA[ii] (typically, send it to the core systolic array via a channel.)
            // Basic Exprs
            Expr origin_call_node = iter->second.read_node;
            

            // record condition in slicer
            CondDetect slicer(vector<string>(),origin_call_node,false);
            new_body.accept(&slicer);
            Expr cond = slicer.cond;

            // read inputA(ii->t)
            Expr read_from_channel = Expr();
            if(strategy_up){
                if(origin_call_node.as<Call>()){
                    read_from_channel = substitute(origin_loop_var, scatter_loop_min, origin_call_node);
                }     
                else
                    read_from_channel = substitute(origin_loop_var, scatter_var, origin_call_node);
            }else{
                if(origin_call_node.as<Call>()){
                    read_from_channel = substitute(origin_loop_var, (scatter_loop_min + scatter_loop_extent - 1), origin_call_node);
                }else{
                    read_from_channel = substitute(origin_loop_var, scatter_var, origin_call_node);
                }
                
            }
            
            vector<Expr> tmp_data_args;
                //unroll loop dims except scatter loop
            for(size_t i = 0;i < unroll_loop_indexes.size();++i){
                int index = unroll_loop_indexes[i];
                string unroll_name = loop_names[index];
                if(unroll_name == origin_loop_name)
                    continue;
                tmp_data_args.push_back(Variable::make(Int(32),unroll_name));
            }

            Expr var_read_channel = read_from_channel;
            // we want to move the read channel stmt outside the scatter loop
            {

                read_from_channel = Call::make(data_type,producer+".scatter.temp",tmp_data_args,Call::PureIntrinsic);
            }
            
            // feederA[ii]
            vector<Expr> read_shreg_args(shreg_args);
            read_shreg_args.insert(read_shreg_args.begin(), shreg_name);
            Expr read_from_shreg_now = Call::make(iter->second.read_node.type(), Call::read_shift_reg, read_shreg_args, Call::Intrinsic);

            // feederA[ii-1]/feederA[ii+1]
            Expr read_from_shreg_before = substitute(origin_loop_var,
                                                strategy_up ? Sub::make(origin_loop_var, 1) : Add::make(origin_loop_var, 1),
                                                read_from_shreg_now);

            // feederA[ii] = read inputA(ii->t)
            vector<Expr> write_shreg_args(shreg_args);
            write_shreg_args.insert(write_shreg_args.begin(), shreg_name);
            write_shreg_args.push_back(read_from_channel);
            Stmt write_shreg_from_channel = Evaluate::make(
                                                Call::make(iter->second.read_node.type(),
                                                        Call::write_shift_reg,
                                                        write_shreg_args,
                                                        Call::Intrinsic));

            //  feederA[ii] = feederA[ii-1]/feederA[ii+1]
            Stmt write_shreg_from_shreg = substitute(read_from_channel, read_from_shreg_before, write_shreg_from_channel);
            // Stmt read_from_fifo = Evaluate::make(write_shreg_from_shreg);

            //  if (ii == 0)/(ii == I - 1)
            //      feederA[ii] = read inputA(ii->t)
            //  else
            //      feederA[ii] = feederA[ii-1]/feederA[ii+1]
            Stmt write_shreg = IfThenElse::make(cond_read, write_shreg_from_channel, write_shreg_from_shreg);

            

            if(cond.defined()){
                Expr new_cond = substitute(origin_loop_name,Variable::make(Int(32),new_loop_name),cond);
                write_shreg = IfThenElse::make(new_cond, write_shreg);
            }
                

            // rw_block = rw_block.defined() ? Block::make(rw_block, rw_stmt) : Block::make({rw_stmt});

            // replace call_node in op->body
            new_body = substitute(origin_call_node, read_from_shreg_now, new_body);

            // update body: if (condition) RWStmt; if t == i updated_body
            if(!scatter_along_removed)
                new_body = IfThenElse::make(EQ::make(scatter_var, origin_loop_var), new_body);
            
            new_body = Block::make(write_shreg, new_body);

            new_body = For::make(origin_loop_name,scatter_loop_min,scatter_loop_extent,ForType::Unrolled,op->device_api,new_body);
            
            Expr new_cond = substitute(
                    origin_loop_name,
                    Variable::make(Int(32),new_loop_name),
                    cond
                );
            
            // if(envs.find(producer) != envs.end()){
            //     auto remove_loops = envs.find(producer)->second.definition().schedule().remove_params();
            //     bool isRemoved = false;
            //     for(auto remove_loop : remove_loops){
            //         if(ends_with(op->name,"."+remove_loop)){
            //             isRemoved = true;
            //         }
            //     }
            //     if(isRemoved){
            //         if(new_cond.defined()){
            //             new_cond = new_cond && (Variable::make(Int(32),op->name+".scatter") == op->min);
            //         }else{
            //             new_cond = Variable::make(Int(32),op->name+".scatter") == op->min;
            //         }
            //     }
            // }
            Stmt new_stmt = Stmt();
            if(new_cond.defined()){
                new_stmt = IfThenElse::make(new_cond,Provide::make(producer + ".scatter.temp",{var_read_channel},tmp_data_args));
            }else{
                new_stmt = Provide::make(producer + ".scatter.temp",{var_read_channel},tmp_data_args);
            }
            new_body = Block::make(new_stmt,new_body);
            {
                int beg = unroll_loop_indexes.size() - 1;
                for(int i = beg;i >= 0; --i){
                    int index = unroll_loop_indexes[i];
                    if(loop_names[index] == origin_loop_name){
                        continue;
                    }
                    new_body = For::make(loop_names[index],loop_mins[index],loop_extents[index],ForType::Unrolled,op->device_api,new_body);
                }
            }
            if(!scatter_along_removed)
                new_body = For::make(new_loop_name,scatter_loop_min,scatter_loop_extent,ForType::Serial,op->device_api,new_body);
            {
                int beg = loop_names.size() - 1;
                for(int i = beg;i >= 0; --i){
                    if(for_types[i] == ForType::Unrolled){
                        continue;
                    }
                    new_body = For::make(loop_names[i],loop_mins[i],loop_extents[i],for_types[i],op->device_api,new_body);
                }
            }

            Region ranges;
            for(size_t i = 0;i < unroll_loop_indexes.size();++i){
                int index = unroll_loop_indexes[i];
                string unroll_name = loop_names[index];
                if(unroll_name == origin_loop_name)
                    continue;
                ranges.push_back(Range(loop_mins[index], loop_extents[index]));
            }
            new_body = Realize::make(producer + ".scatter.temp",{data_type},MemoryType::Auto,ranges,const_true(),new_body);
        }
        return new_body;
    }
public:
    ScatterInserter(
        const map<string, Function>& _envs, 
        const ScatterBufferArgs& _scatterbuffer_args, 
        const string& _caller_name):
    envs(_envs), scatterbuffer_args(_scatterbuffer_args), caller_name(_caller_name){}
};

// Transform IR to buffer and scatter incoming data. The code closely follows this document:
//   ../doc/compiler_design/buffer.md
// So read the document first in order to understand the code.
class ScatterAndBuffer: public IRMutator{
    using IRMutator::visit;
private:
    // Input info of the entire IR, not just the current Func.
    const map<string, Function>& envs;
    const vector<tuple<string, Expr, Expr, ForType>> &all_loops; // Full names, mins, extents and types of loops

    // Info of the current Func
    const string &func_name; // The function that buffers and scatters incoming data
    const string &producer;  // The Func that sends data to buffer and scatter
    const string &buffer_loop; // Loop under which a buffer is to be inserted. Undocorated name without func and stage prefix, e.g. "x"
    const string &scatter_loop; // Loop along which data are to be scattered.  Undocorated name without func and stage prefix, e.g. "y"
    const Expr &original_read_node; // The expression in the consumer that reads the data from the producer
    const Expr &original_read_condition; // Path condition to the original_read_node. It is the same condition the producer writes to its output channel.
    const BufferStrategy buffer_strategy;
    const ScatterStrategy scatter_strategy;
    const vector<tuple<string, Expr, Expr, ForType>> &loops; // Full names, mins, extents and types of the loops around the read node.

private:
    // Frequently used constants and variables used in transforming the IR.
    Type TYPE;               // Type of the incoming data
    vector<int> WRITE_LOOPS; // Serial loops in the producer below the buffer insertion point. Elements: indices to loops
    vector<int> READ_LOOPS;  // Serial loops in the consumer below the buffer insertion point. Elements: indices to loops
    uint32_t WRITES;         // Product of the extents of WRITE_LOOPS
    uint32_t READS;          // Product of the extents of READ_LOOPS
    int32_t BUFFERS;         // Number of double buffers (i.e. extent of the scatter loop)
    int32_t BANKS;           // The last dimension of a buffer, which usually equals BUFFERS, rounded up to a power of two
    uint32_t CYCLES_PER_PERIOD;
    uint32_t INIT;           // The cycle in a period when buffer writing should start.
    Expr PERIODS;            // Total periods
    Expr cycle;              // Current cycle
    Expr in_v;               // Incoming value read from the input channel in the current cycle
    Expr value;              // Incoming value stored in shift registers for scattering
    Expr time_stamp;         // Cycle stored in shift registers for scattering
    Expr buf_loop_var;
    Expr period;
    Expr offset;
    Expr time_to_write_buffer;

    int original_scatter_loop;             // Index to loops for the full name of the scatter loop in the consumer
    bool scatter_loop_removed_in_producer; // The scatter loop is removed in the producer?
    Expr original_scatter_loop_var;

    // As an extension to our design document, besides the scatter loop, we allow other unroll loops to exist.
    // Every iteration of these other unroll loops has its own n double buffers that store and scatter its own
    // data. All the iterations of these other unroll loops are independent from each other.
    // See the comments for visit_innermost_loop() for details.
    vector<int> unroll_loops;                 // All unroll loops. Elements are indices to loops.
    vector<Expr> unroll_loop_vars;            // Variables of unroll_loops
    Region unroll_loop_dims;                  // Bounds of unroll_loops
    vector<int> nonscatter_unroll_loops;      // All unroll loops except the scatter loop. Elements are indices to loops.
    vector<Expr> nonscatter_unroll_loop_vars; // Variables of nonscatter_unroll_loops
    Region nonscatter_unroll_loop_dims;       // Bounds of nonscatter_unroll_loops

    // Variables for a single buffer:
    Expr _cycle;
    Expr _period;
    Expr _offset;
    Expr _time_to_write_buffer;
    Expr _owner;
    Expr _idx;
    Expr _time_to_read;

    // The incoming data type might be a compiler-generated struct, which contains
    // multiple fields, and each field might differ in their degrees of reuse. Therefore,
    // we can allocate separate buffers for them, with sizes decided by their degrees
    // of reuse separately. This will minimize the total sizes of memory allocation.
    typedef struct {
        string name;             // Name of this buffer (for one field)
        Type   type;             // Element type
        Region dims;             // [2][extents of NonScatter_NonReuse_Write_Loops][extents of nonscatter_unroll_loops][BANKS],
                                 // where NonScatter_NonReuse_Write_Loops = (WRITE_LOOPS - REUSE_WRITE_LOOPS - scatter loop),
                                 // where REUSE_WRITE_LOOPS are subset of WRITE_LOOPS that are not referred by the field.
        vector<Expr> write_args; // [_idx][WRITE_TO(_offset)][nonscatter_unroll_loops][buf], where WRITE_TO(_offset) is the address determined by
                                 // the NonScatter_NonReuse_Write_Loops out of the WRITE_LOOPS
        vector<Expr> read_args;  // DB[!_idx][READ_FROM(_offset)][nonscatter_unroll_loops][buf], where READ_FROM(_offset) is the address determined by
                                 // the NonScatter_NonReuse_Write_Loops out of the READ_LOOPS
    } buffer_info;
    vector<buffer_info> buffers_info; // Buffer info for all fields

public:
    ScatterAndBuffer(
        const map<string, Function>& envs,
        const vector<tuple<string, Expr, Expr, ForType>> &all_loops,
        const string &func_name,
        const string &producer,
        const string &buffer_loop,
        const string &scatter_loop,
        const Expr &original_read_node,
        const Expr &original_read_condition,
        const BufferStrategy buffer_strategy,
        const ScatterStrategy scatter_strategy,
        const vector<tuple<string, Expr, Expr, ForType>> &loops) :
            envs(envs), all_loops(all_loops), func_name(func_name), producer(producer),
            buffer_loop(buffer_loop), scatter_loop(scatter_loop),
            original_read_node(original_read_node), original_read_condition(original_read_condition),
            buffer_strategy(buffer_strategy), scatter_strategy(scatter_strategy), loops(loops) {
                initialize_common_constants_vars();
    }

private:
    const string &var_name(const Expr &e) const {
        const Variable *v = e.as<Variable>();
        internal_assert(v);
        return v->name;
    }

    string replace_postfix(const string &str, const string &postfix, const string &replacement) {
        internal_assert(ends_with(str, postfix));
        return str.substr(0, str.size() - postfix.size()) + replacement;
    }

    // This class does not really mutate anything. It inherits IRMutator to take advantage of mutate() functions.
    class VarReferred : public IRMutator {
        using IRMutator::visit;
    private:
        const string &var;
    public:
        bool referred;
        VarReferred(const string &var) : var(var) {
            referred = false;
        }
    public:
        Expr mutate(const Expr &e) override {
            if (referred) {
                return e;
            }
            const Variable *v = e.as<Variable>();
            if (v) {
                if (v->name == var) {
                    referred = true;
                    return e;
                }
            }
            return IRMutator::mutate(e);
        }
        Stmt mutate(const Stmt &s) override {
            if (referred) {
                return s;
            }
            return IRMutator::mutate(s);
        }
    };

    // See if a loop variable is used in the expression
    bool loop_referred(const string &loop, const Expr &e) {
        VarReferred ref(loop);
        ref.mutate(e);
        return ref.referred;
    }

    // The correponding loop name in the producer.
    string producer_loop_name(const string &loop_name) {
        const Function &func = envs.find(producer)->second;
        string ploop = func.name() + ".s0." + extract_after_tokens(loop_name, 2);
        return ploop;
    }

    // The loop is removed?
    bool loop_is_removed(const string &loop_name) {
        for (auto &l : all_loops) {
            if (loop_name == std::get<0>(l)) {
                return false;
            }
        }
        return true;
    }

    void intialize_common_constants() {
        TYPE = original_read_node.type();
        WRITES = 1, READS = 1, PERIODS = 1;
        scatter_loop_removed_in_producer = false;
        bool inside_buffer_loop = false;
        for(size_t i = 0; i < loops.size(); i++) {
            auto &l = loops[i];
            string loop_name = std::get<0>(l);
            Expr loop_extent = std::get<2>(l);
            ForType for_type = std::get<3>(l);
            if (inside_buffer_loop) {
                string producer_loop = producer_loop_name(loop_name); // Corresponding loop in producer
                if(!loop_is_removed(producer_loop)) {
                    if (for_type != ForType::Unrolled) {
                        // NOTE: we check if a loop is serial based on its current for_type in the consumer.
                        // So WRITE_LOOPS are those that are not removed in the producer, and currently
                        // serial in the consumer. Scatter loop is an exception: it is excluded here,
                        // but may be added in the end.
                        string undecorated_loop_name = extract_after_tokens(loop_name, 2);
                        user_assert(loop_extent.as<IntImm>()) << "Loop " << undecorated_loop_name
                                <<" in func " << envs.find(producer)->second.name()
                                << " is below the buffer loop, and is expected to have a constant extent.";
                        int loop_extent_val = loop_extent.as<IntImm>()->value;
                        WRITES = WRITES * loop_extent_val;
                        WRITE_LOOPS.push_back(i);
                    }
                } else {
                    if (ends_with(loop_name, "." + scatter_loop)) {
                        scatter_loop_removed_in_producer = true;
                    }
                }
                if (for_type != ForType::Unrolled) {
                    // NOTE: we check if a loop is serial based on its current for_type in the consumer.
                    // So READ_LOOPS are those that are currently serial in the consumer.
                    string undecorated_loop_name = extract_after_tokens(loop_name, 2);
                    user_assert(loop_extent.as<IntImm>()) << "Loop " << undecorated_loop_name
                            <<" in func " << func_name
                            << " is below the buffer loop, and is expected to have a constant extent.";
                    int loop_extent_val = loop_extent.as<IntImm>()->value;
                    READS = READS * loop_extent_val;
                    READ_LOOPS.push_back(i);
                }
            } else {
                PERIODS = PERIODS * loop_extent;
            }
            if(ends_with(loop_name, "." + buffer_loop)) {
                inside_buffer_loop = true;
            }
            if(ends_with(loop_name, "." + scatter_loop)){
                original_scatter_loop = i;
                internal_assert(loop_extent.as<IntImm>());
                BUFFERS = loop_extent.as<IntImm>()->value;
            }
        }

        if (!scatter_loop_removed_in_producer) {
            WRITES = WRITES * BUFFERS;
            WRITE_LOOPS.push_back(original_scatter_loop);
        }

        debug(4) << "Buffer WRITES: " << WRITES << ", WRITE_LOOPS: ";
        for (auto W : WRITE_LOOPS) {
            debug(4) << std::get<0>(loops[W]) << " ";
        }
        debug(4) << "\nBuffer READS: " << READS << ",  READ_LOOPS: ";
        for (auto R : READ_LOOPS) {
            debug(4) << std::get<0>(loops[R]) << " ";
        }
        debug(4) << "\nPERIODS: " << PERIODS << "\n";

        if (READS < WRITES) {
            user_warning << "Buffering in func " << func_name << ": READS (" << READS << ")" << " < WRITES("
                    << WRITES << "). This buffer might not be abel to hide the latency of reading from the main memory\n";
        }

        CYCLES_PER_PERIOD = std::max(READS, WRITES);
        INIT = (READS >= WRITES) ? (READS - WRITES) : 0;

        for(size_t i = 0; i < loops.size(); i++){
            auto &l = loops[i];
            ForType for_type = std::get<3>(l);
            if (for_type == ForType::Unrolled) {
                string loop_name = std::get<0>(l);
                Expr loop_min = std::get<1>(l);
                Expr loop_extent = std::get<2>(l);
                unroll_loops.push_back(i);
                unroll_loop_vars.push_back(Variable::make(Int(32), loop_name));
                unroll_loop_dims.push_back(Range(loop_min, loop_extent));
                if(!ends_with(loop_name,"." + scatter_loop)){
                    nonscatter_unroll_loops.push_back(i);
                    nonscatter_unroll_loop_vars.push_back(Variable::make(Int(32), loop_name));
                    nonscatter_unroll_loop_dims.push_back(Range(loop_min, loop_extent));
                }
            }
        }
    }

    // Loop variables referred in an isolated operand are the original loop variables
    // before space time transform. Replace them with those after the transform
    Expr isolated_operand_with_loop_vars_after_stt(const Expr &isolated_opnd) {
        debug(4) << "Isolated operand: " << to_string(isolated_opnd) << "\n";
        const Function &f = envs.at(func_name);
        auto &params = f.definition().schedule().transform_params();
        internal_assert(params.size() <= 1); // Currently only 1 STT is allowed in a function
        Expr opnd = isolated_opnd;
        if (!params.empty()) {
            const auto &reverse = params[0].reverse;
            for (const auto &r : reverse) {
                opnd = substitute(r.first, r.second, opnd);
                debug(4) << "  Replace " << r.first << " with " << r.second << " and get " << opnd << "\n";
            }
        }
        return opnd;
    }

    void calculate_buffer_dims_args(const Expr &isolated_opnd, buffer_info &buf) {
        // Loop vars referred in the isolated operand have no prefix. Thus we search
        // undecorated name in the isolated operand.
        Expr new_isolated_opnd = isolated_operand_with_loop_vars_after_stt(isolated_opnd);
        debug(4) << "calculate_buffer_dims_args with isolated operand " << to_string(new_isolated_opnd) << "\n";

        buf.dims.push_back(Range(0, 2));
        buf.write_args.push_back(_idx);
        buf.read_args.push_back(!_idx);

        vector<int> NonScatter_NonReuse_Write_Loops;
        for(size_t i = 0; i < WRITE_LOOPS.size(); i++){
            auto &l = loops[WRITE_LOOPS[i]];
            string loop_name = std::get<0>(l);
            string undecorated_loop_name = extract_after_tokens(loop_name, 2);
            if (undecorated_loop_name == scatter_loop) {
                continue;
            }
            if (!loop_referred(undecorated_loop_name, new_isolated_opnd)) {
                debug(4) << "Found a reuse loop: " << loop_name << ". Not adding it to buffer dims.\n";
                continue;
            }
            Expr value;
            if (loop_var_is_constant_in_condition(loop_name, original_read_condition, value)) {
                debug(4) << "Found a loop that has only 1 iteration: " << loop_name << ". Not adding it to buffer dims\n";
                continue;
            }

            debug(4) << "Found a NonScatter_NonReuse_Write_Loop: " << loop_name << "\n";
            NonScatter_NonReuse_Write_Loops.push_back(i);

            Expr loop_extent = std::get<2>(l);
            internal_assert(loop_extent.as<IntImm>());
            int loop_extent_val = loop_extent.as<IntImm>()->value;
            buf.dims.push_back(Range(0, loop_extent_val));
            buf.write_args.push_back(Variable::make(Int(32),loop_name));
            buf.read_args.push_back(Variable::make(Int(32),loop_name));
        }

        for (size_t i = 0; i < nonscatter_unroll_loops.size(); i++) {
            const string &loop_name = nonscatter_unroll_loop_vars[i].as<Variable>()->name;
            Expr value;
            if (loop_var_is_constant_in_condition(loop_name, original_read_condition, value)) {
                debug(4) << "Unrolled loop " << loop_name << " has only 1 iteration. Not adding it to buffer dims\n";
                continue;
            }
            Expr loop_extent = nonscatter_unroll_loop_dims[i].extent;
            internal_assert(loop_extent.as<IntImm>());
            int loop_extent_val = loop_extent.as<IntImm>()->value;
            buf.dims.push_back(Range(0, loop_extent_val));
            buf.write_args.push_back(nonscatter_unroll_loop_vars[i]);
            buf.read_args.push_back(nonscatter_unroll_loop_vars[i]);
        }

        if (!scatter_loop_removed_in_producer) {
            BANKS = (int32_t)closest_power_of_two((uint32_t)BUFFERS);
            buf.dims.push_back(Range(0, Expr(BANKS)));
            buf.write_args.push_back(buf_loop_var);
            buf.read_args.push_back(buf_loop_var);
        } else {
            auto dim = buf.dims.back();
            internal_assert(dim.extent.as<IntImm>());
            int extent = dim.extent.as<IntImm>()->value;
            BANKS = (int32_t)closest_power_of_two((uint32_t)extent);
            buf.dims.back() = Range(dim.min, BANKS);
        }
    }

    void calculate_buffer_dims_args() {
        // TODO: so far, we assume that the buffer is inserted to a Func that was isolated
        // out of another Func as that Func's producer. Should remove this limitation in future.
        const vector<Expr> &isolated_operands = envs.at(func_name).isolated_operands_as_producer();
        if (TYPE.is_generated_struct()) {
            int type_id = TYPE.bits();
            const std::pair<string, vector<Type>> &entry = GeneratedStructType::structs[type_id];
            internal_assert(entry.second.size() == isolated_operands.size());
            for (size_t i = 0; i < isolated_operands.size(); i++) {
                auto opnd = isolated_operands[i];
                // The following assertion might not hold: After isolation, we might have done
                // vectorization, so the original isolated operand's type might not be current.
                // internal_assert(opnd.type() == entry.second[i]);
                buffer_info buf;
                buf.type = entry.second[i];
                buf.name = func_name + "_DB_f" + std::to_string(i) + ".ibuffer";
                calculate_buffer_dims_args(opnd, buf);
                buffers_info.push_back(buf);
            }
        } else {
            internal_assert(isolated_operands.size() == 1);
            buffer_info buf;
            buf.type = original_read_node.type();
            buf.name = func_name + "_DB.ibuffer";
            calculate_buffer_dims_args(isolated_operands[0], buf);
            buffers_info.push_back(buf);
        }
    }

    void initialize_common_constants_vars() {
        intialize_common_constants();

        value = Variable::make(TYPE, func_name + "_value.shreg");
        time_stamp = Variable::make(UInt(32), func_name + "_time_stamp.shreg");
        cycle = Variable::make(UInt(32), func_name + "_cycle.temp");
        in_v = Variable::make(TYPE, func_name + "_in_v.temp");
        buf_loop_var = Variable::make(Int(32), func_name + ".s0.buf");
        const string &original_scatter_loop_name = std::get<0>(loops[original_scatter_loop]);
        original_scatter_loop_var = Variable::make(Int(32), original_scatter_loop_name);
        period = Variable::make(UInt(32), "period");
        offset = Variable::make(UInt(32), "offset");
        time_to_write_buffer = Variable::make(Bool(1), "time_to_write_buffer");

        _cycle = Variable::make(UInt(32), "_cycle");
        _period = Variable::make(UInt(32), "_period");
        _offset = Variable::make(UInt(32), "_offset");
        _time_to_write_buffer = Variable::make(Bool(1), "_time_to_write_buffer");
        _owner = Variable::make(UInt(32), "_owner");
        _idx = Variable::make(Bool(1), "_idx");
        _time_to_read = Variable::make(Bool(1), "_time_to_read");

        calculate_buffer_dims_args();
    }

public:
    Stmt visit(const For *op) override{
        if(ends_with(op->name,".run_on_device")){
            return visit_kernel(op);
        }
        Stmt new_body = mutate(op->body);
        if(op->name == std::get<0>(loops.back())) { //innermost loop
            visit_innermost_loop(op, new_body);
        }
        return new_body;
    }

    /* Make IR for the following declarations:
     *   TYPE value[unroll_loop_dims];
         int  time_stamp[unroll_loop_dims];
         TYPE in_v[nonscatter_unroll_loop_dims];
         int  cycle[nonscatter_unroll_loop_dims];
       Note this code pattern is an extension to the design doc to allow unroll loops
       besides the scatter loop.

       The corresponding IR for the code pattern looks like this:
         realize value.shreg, time_stamp.shreg
           parallel<OpenCL> (loop name, 0, 1) // Dummy loop
             parallel<OpenCL> value.run_on_device // Dummy loop
               parallel<OpenCL> time_stamp.run_on_device // Dummy loop
                 realize cycle.temp, in_v.temp, DB.ibuffer
                   body
       The last two dummy loops are needed for the OpenCL code generator
       to declare value.shreg and time_stamp.shreg
    */
    Stmt visit_kernel(const For *op) {
        internal_assert(ends_with(op->name,".run_on_device"));
        Stmt new_body = IRMutator::mutate(op->body);
        for (auto b : buffers_info) {
            new_body = Realize::make(b.name, {b.type}, MemoryType::Auto, b.dims, const_true(), new_body);
        }
        new_body = Realize::make(var_name(cycle), {UInt(32)}, MemoryType::Auto, nonscatter_unroll_loop_dims, const_true(), new_body);
        new_body = Realize::make(var_name(in_v), {TYPE}, MemoryType::Auto, nonscatter_unroll_loop_dims, const_true(), new_body);
        new_body = For::make(replace_postfix(var_name(time_stamp), ".shreg", ".run_on_device"), 0, 1, ForType::Parallel, DeviceAPI::OpenCL, new_body);
        new_body = For::make(replace_postfix(var_name(value), ".shreg", ".run_on_device"), 0, 1, ForType::Parallel, DeviceAPI::OpenCL, new_body);
        new_body = For::make(op->name, op->min, op->extent, op->for_type, op->device_api, new_body);
        new_body = Realize::make(var_name(time_stamp), {UInt(32)}, MemoryType::Auto, unroll_loop_dims, const_true(), new_body);
        new_body = Realize::make(var_name(value), {TYPE}, MemoryType::Auto, unroll_loop_dims, const_true(), new_body);
        return new_body;
    }

    /* Generate IR like this:
     *   initialize cycle
     *   while (1)
     *      all unroll loops except the scatter loop // NEW to the design doc
     *          get input // All variables in this step needs an instance for
     *                    // each iteration of the above unroll loops
     *          unroll for buf = 0 : BUFFERS
     *                   original_scatter_loop = buf + scatter loop's min
     *                   broadcast input by scattering
     *                   write buffer
     *                   read buffer
     *          cycle++
     * A difference from the document is that we allow other unroll loops to exist
     * besides the scatter loop in the consumer. So all the variables used between
     * the above "all unrolled loops except the scatter loop" and "unroll for buf" loop,
     * i.e. "get input", need an instance for every iteration of the other unroll loops,
     * and need indices of these unroll loops. Such variables include cycle and in_v
     * (The other 3 variables: period, offset, and time_to_write_buffer can be made as
     * the target variables of LetStmts and thus avoid using the indices explicitly).
     */
    void visit_innermost_loop(const For *op, Stmt &new_body) {
        internal_assert(!ends_with(op->name,".run_on_device")); // not kernel loop
        internal_assert(op->name == std::get<0>(loops.back())); // is innermost loop

        // Build new loop body bottom up
        read_from_buffer(new_body);
        write_to_buffer(new_body);
        broadcast_input_by_scattering(new_body);

        // Add scatter loop
        const Expr &original_scatter_loop_min = std::get<1>(loops[original_scatter_loop]);
        const Expr &original_scatter_loop_extent = std::get<2>(loops[original_scatter_loop]);
        new_body = LetStmt::make(var_name(original_scatter_loop_var), buf_loop_var + original_scatter_loop_min, new_body);
        new_body = For::make(var_name(buf_loop_var), 0, original_scatter_loop_extent, ForType::Unrolled,op->device_api,new_body);

        // Read input value before the scatterring
        get_input(new_body);

        Expr single_PE_cond;
        vector<string> unrolled_loops_without_terms;
        vector<string> unrolled_loops_name;
        for (auto &v : nonscatter_unroll_loop_vars) {
            unrolled_loops_name.push_back(v.as<Variable>()->name);
        }
        Stmt inc_cycle = Provide::make(var_name(cycle), {Call::make(UInt(32), var_name(cycle), nonscatter_unroll_loop_vars, Call::PureIntrinsic) + 1}, nonscatter_unroll_loop_vars);
        if (check_is_single_PE(true, original_read_condition, unrolled_loops_name, {}, single_PE_cond, unrolled_loops_without_terms)) {
            inc_cycle = IfThenElse::make(single_PE_cond, inc_cycle);
        }
        new_body = Block::make(new_body, inc_cycle);

        add_nonscatter_unroll_loops(op->device_api, new_body);

        // TODO: change here into while(1)
        new_body = For::make(func_name + ".s0.outermost_loop", 0, Expr(PERIODS + 1) * Expr(CYCLES_PER_PERIOD), ForType::Serial, op->device_api, new_body);

        initialize(op->device_api, new_body);
    }

    /* Make IR like this:
     *    unroll for nonscatter_unroll_loop_vars
     *                cycle[nonscatter_unroll_loop_vars] = INIT;
     */
    void initialize(const DeviceAPI device_api, Stmt &new_body) {
        vector<Expr> init_args;
        for (auto v : nonscatter_unroll_loop_vars) {
            init_args.push_back(Variable::make(Int(32), var_name(v) + "_init"));
        }

        Expr single_PE_cond, original_cond = original_read_condition;
        vector<string> unrolled_loops_without_terms;
        vector<string> unrolled_loops_name;
        for (auto &v : nonscatter_unroll_loop_vars) {
            string name = v.as<Variable>()->name;
            unrolled_loops_name.push_back(name + "_init");
            original_cond = substitute(name, Variable::make(Int(32), name + "_init"), original_cond);
        }
        Stmt init_cycle = Provide::make(var_name(cycle), {Expr(INIT)}, init_args);
        if (check_is_single_PE(true, original_cond, unrolled_loops_name, {}, single_PE_cond, unrolled_loops_without_terms)) {
            init_cycle = IfThenElse::make(single_PE_cond, init_cycle);
        }

        for (int i = nonscatter_unroll_loops.size() - 1; i >= 0; i--) {
            auto &l = loops[nonscatter_unroll_loops[i]];
            string name = std::get<0>(l);
            int min = std::get<1>(l).as<IntImm>()->value;
            int extent = std::get<2>(l).as<IntImm>()->value;
            init_cycle = For::make(name + "_init", min, extent, ForType::Unrolled, device_api, init_cycle);
        }
        new_body = Block::make(init_cycle, new_body);
    }

    // Wrap the new body with all unroll loops except the scatter loop
    void add_nonscatter_unroll_loops(const DeviceAPI device_api, Stmt &new_body) {
        for (int i = nonscatter_unroll_loops.size() - 1; i >= 0; i--) {
            auto &l = loops[nonscatter_unroll_loops[i]];
            string loop_name = std::get<0>(l);
            Expr loop_min = std::get<1>(l);
            Expr loop_extent = std::get<2>(l);
            new_body = For::make(loop_name, loop_min, loop_extent, ForType::Unrolled, device_api,new_body);
        }
    }

    /* Make IR as:
     *   int period = cycle[nonscatter_unroll_loop_vars] / CYCLES_PER_PERIOD; // current period
         int offset = cycle[nonscatter_unroll_loop_vars] % CYCLES_PER_PERIOD; // relative position of the current cycle in the current period
         bool time_to_write_buffer = (offset >= INIT);
         if ((period < PERIODS) && time_to_write_buffer) {
             in_v[nonscatter_unroll_loop_vars] = read_channel_intel(IN_CHANNEL);
         }
     */
    void get_input(Stmt &new_body) {
        // The original scatter loop is unroll loop in the consumer, and thus every
        // iteration of this loop reads from the producer with a seperate channel.
        // With scattering, however, we let only the first iteration reads from
        // the producer.
        const string &original_scatter_loop_name = std::get<0>(loops[original_scatter_loop]);
        Expr scatter_loop_min = std::get<1>(loops[original_scatter_loop]);
        Expr read_node = substitute(original_scatter_loop_name, scatter_loop_min, original_read_node);
        Stmt read_input = Provide::make(var_name(in_v), {read_node}, nonscatter_unroll_loop_vars);

        if (!equal(original_read_condition, const_true())) {
            // This piece of "get_input" IR will be wrapped around by all the unrolled loops, except
            // the scatter loop, which has been serialized in the producer, and thus in the consumer
            // side, we can only know the scatter loop is in which iteration by decoding the cycle.
            // This is necessary when the scatter loop var is used in the original condition.
            const string &original_scatter_loop_name = std::get<0>(loops[original_scatter_loop]);
            Expr scatter_loop_tmp = Variable::make(Int(32), original_scatter_loop_name + "_tmp");
            Expr new_condition = substitute(original_scatter_loop_name, scatter_loop_tmp, original_read_condition);
            read_input = IfThenElse::make(new_condition, read_input);
            Expr total_writes_so_far = Variable::make(UInt(32), "total_writes_so_far");
            Expr scatter_loop_val = (total_writes_so_far  % Expr((uint32_t)BUFFERS)) + scatter_loop_min;
            read_input = LetStmt::make(var_name(scatter_loop_tmp), scatter_loop_val, read_input);
            read_input = LetStmt::make(var_name(total_writes_so_far), period * Expr(WRITES) + offset - Expr(INIT), read_input);
        }

        Expr condition = (period < PERIODS) && time_to_write_buffer;
        read_input = IfThenElse::make(condition, read_input);

        read_input = LetStmt::make(var_name(time_to_write_buffer), (offset >= Expr(INIT)), read_input);

        Expr offset_val = Call::make(UInt(32), var_name(cycle), nonscatter_unroll_loop_vars, Call::PureIntrinsic) % Expr(CYCLES_PER_PERIOD);
        read_input = LetStmt::make(var_name(offset), offset_val, read_input);

        Expr period_val = Call::make(UInt(32), var_name(cycle), nonscatter_unroll_loop_vars, Call::PureIntrinsic) / Expr(CYCLES_PER_PERIOD);
        read_input = LetStmt::make(var_name(period), period_val, read_input);

        new_body = Block::make(read_input, new_body);
    }

    /**
     * if(buf == 0) {
     *     value     [unroll_loop_vars] = in_v [nonscatter_unroll_loop_vars];
     *     time_stamp[unroll_loop_vars] = cycle[nonscatter_unroll_loop_vars];
     * } else {
     *     value     [unroll_loop_vars] = value     [unroll_loop_vars with buf - 1];
     *     time_stamp[unroll_loop_vars] = time_stamp[unroll_loop_vars with buf - 1];
     * }
     */
    void broadcast_input_by_scattering(Stmt &new_body) {
        bool strategy_up = (scatter_strategy == ScatterStrategy::Up);

        // If branch
        vector<Expr> write_value0_args(unroll_loop_vars);
        write_value0_args.insert(write_value0_args.begin(), var_name(value));
        write_value0_args.push_back(Call::make(TYPE, var_name(in_v), nonscatter_unroll_loop_vars, Call::PureIntrinsic));
        Stmt write_value0 = Evaluate::make(Call::make(TYPE, Call::write_shift_reg, write_value0_args, Call::Intrinsic));

        vector<Expr> write_time_stamp0_args(unroll_loop_vars);
        write_time_stamp0_args.insert(write_time_stamp0_args.begin(), var_name(time_stamp));
        write_time_stamp0_args.push_back( Call::make(UInt(32), var_name(cycle), nonscatter_unroll_loop_vars, Call::PureIntrinsic));
        Stmt write_time_stamp0 = Evaluate::make(Call::make(UInt(32), Call::write_shift_reg, write_time_stamp0_args, Call::Intrinsic));

        Stmt base = Block::make(write_value0, write_time_stamp0);

        // Else branch
        Expr prev_buf =  strategy_up ? Sub::make(buf_loop_var, 1) : Add::make(buf_loop_var, 1);
        const string &original_scatter_loop_name = std::get<0>(loops[original_scatter_loop]);
        vector<Expr> prev_args(unroll_loop_vars);
        for (auto &a : prev_args) {
            if (var_name(a) == original_scatter_loop_name) {
                a = prev_buf;
                break;
            }
        }
        vector<Expr> read_value_args(prev_args);
        read_value_args.insert(read_value_args.begin(), var_name(value));
        Expr read_value = Call::make(TYPE, Call::read_shift_reg, read_value_args, Call::PureIntrinsic);

        vector<Expr> shift_value_args(unroll_loop_vars);
        shift_value_args.insert(shift_value_args.begin(), var_name(value));
        shift_value_args.push_back(read_value);
        Stmt shift_value =  Evaluate::make(Call::make(TYPE, Call::write_shift_reg, shift_value_args, Call::Intrinsic));

        vector<Expr> read_time_stamp_args(prev_args);
        read_time_stamp_args.insert(read_time_stamp_args.begin(), var_name(time_stamp));
        Expr read_time_stamp = Call::make(UInt(32), Call::read_shift_reg, read_time_stamp_args, Call::PureIntrinsic);

        vector<Expr> shift_time_stamp_args(unroll_loop_vars);
        shift_time_stamp_args.insert(shift_time_stamp_args.begin(), var_name(time_stamp));
        shift_time_stamp_args.push_back(read_time_stamp);
        Stmt shift_time_stamp =  Evaluate::make(Call::make(UInt(32), Call::write_shift_reg, shift_time_stamp_args, Call::Intrinsic));

        Stmt recursion = Block::make(shift_value, shift_time_stamp);

        // Condition
        Expr condition = strategy_up ? (buf_loop_var == 0) : (buf_loop_var == Expr(BUFFERS - 1));

        // Compose together
        Stmt broadcast_incoming_value = IfThenElse::make(condition, base, recursion);
        if (!equal(original_read_condition, const_true())) {
            broadcast_incoming_value = IfThenElse::make(original_read_condition, broadcast_incoming_value);
        }
        new_body = Block::make(broadcast_incoming_value, new_body);
    }

    /* Make IR as:
        int  _cycle = time_stamp[unroll_loop_vars];
        int  _period = _cycle / CYCLES_PER_PERIODS;
        int  _offset = _cycle % CYCLES_PER_PERIODS;
        bool _time_to_write_buffer = (_offset >= INIT);
        int  _owner = _cycle % BUFFERS;
        bool _idx = _period & 1;
        if (buf == _owner) // Note: needed only when the scatter loop is not removed in the producer
          if (_time_to_write_buffer)
            TYPE _tmp =  = value[unroll_loop_vars];
            DB_f0[_idx][WRITE_TO(_offset)][buf] = _tmp.f0;
            DB_f1[_idx][WRITE_TO(_offset)][buf] = _tmp.f1;
            ...
            // Or simply: DB[_idx][WRITE_TO(_offset)][buf] = value[unroll_loop_vars]; if there is only 1 type of buffer.
    */
    void write_to_buffer(Stmt &new_body) {
        Stmt write_buffer;
        vector<Expr> read_value_args(unroll_loop_vars);
        read_value_args.insert(read_value_args.begin(), var_name(value));
        Expr read_value = Call::make(TYPE, Call::read_shift_reg, read_value_args, Call::PureIntrinsic);

        int num_types_of_buffers = buffers_info.size();
        internal_assert(num_types_of_buffers >= 1);
        if (num_types_of_buffers == 1) {
            // DB[_idx][WRITE_TO(_offset)][buf] = value[unroll_loop_vars]
            write_buffer = Provide::make(buffers_info[0].name, {read_value}, buffers_info[0].write_args);
        } else {
            // TYPE _tmp = value[unroll_loop_vars];
            // DB_f0[_idx][WRITE_TO(_offset)][buf] = _tmp.f0;
            // DB_f1[_idx][WRITE_TO(_offset)][buf] = _tmp.f1;
            // ...
            Expr _tmp = Variable::make(TYPE, "_tmp");
            vector<Stmt> writes(buffers_info.size());
            for (size_t i = 0; i < buffers_info.size(); i++) {
                Expr field = Call::make(buffers_info[i].type, Call::read_field, {_tmp, IntImm::make(Int(32), i)}, Call::PureIntrinsic);
                writes[i] = Provide::make(buffers_info[i].name, {field}, buffers_info[i].write_args);
            }
            write_buffer = Block::make(writes);
            write_buffer = LetStmt::make("_tmp", read_value, write_buffer);
        }
        write_buffer = IfThenElse::make(original_read_condition, write_buffer);

        // Calculate the write loop vars for write buffer.
        // These loops will be serial in the producer. Some of them might be unrolled
        // originally, and such a loop is added separately in two places:
        // (1) as the new innermost loop if it is the scatter loop, (2) as the loops
        // right below the outermost loop (see add_nonscatter_unroll_loops).
        Expr writes = _offset - Expr(INIT); // Total writes in the current period
        for(int i = WRITE_LOOPS.size() - 1; i >= 0; i--){
            int loop_index = WRITE_LOOPS[i];
            auto &l = loops[loop_index];
            Expr loop_extent = std::get<2>(l);
            if(loop_index == original_scatter_loop && !scatter_loop_removed_in_producer){
                writes = writes / loop_extent;
                continue;
            }
            if (std::get<3>(l) == ForType::Unrolled) {
                continue;
            }
            string loop_name = std::get<0>(l);
            Expr loop_min = std::get<1>(l);
            Expr var;
            if (i == 0) {
                // Optimization: writes must already be within loop_extent. No need to %.
                var = writes + loop_min;
            } else {
                var = (writes % loop_extent) + loop_min;
            }
            writes = writes / loop_extent;
            write_buffer = LetStmt::make(loop_name, var, write_buffer);
        }

        Expr _owner_value;
        // If scatter loop is not removed in producer, BUFFERS must be a factor of WRITES(i.e. _period * WRITES % BUFFERS == 0)
        // We can optimize the calculation of owner.  
        if (!scatter_loop_removed_in_producer) {
            write_buffer = IfThenElse::make(buf_loop_var == _owner, write_buffer);
            _owner_value = (_offset - Expr((uint32_t)INIT)) % Expr((uint32_t)BUFFERS);
        } else {
            _owner_value = (_period * Expr((uint32_t)WRITES) + _offset - Expr((uint32_t)INIT)) % Expr((uint32_t)BUFFERS);
        }
        write_buffer = IfThenElse::make(_time_to_write_buffer, write_buffer);
        new_body = Block::make(write_buffer, new_body);
        new_body = LetStmt::make(var_name(_idx), Cast::make(Bool(), _period & 0x1), new_body);
        new_body = LetStmt::make(var_name(_owner), _owner_value, new_body);
        new_body = LetStmt::make(var_name(_time_to_write_buffer), _offset >= Expr(INIT), new_body);
        new_body = LetStmt::make(var_name(_offset), _cycle % Expr(CYCLES_PER_PERIOD), new_body);
        new_body = LetStmt::make(var_name(_period), _cycle / Expr(CYCLES_PER_PERIOD), new_body);
        vector<Expr> read_time_stamp_args(unroll_loop_vars);
        read_time_stamp_args.insert(read_time_stamp_args.begin(), var_name(time_stamp));
        new_body = LetStmt::make(var_name(_cycle), Call::make(UInt(32), Call::read_shift_reg, read_time_stamp_args, Call::PureIntrinsic), new_body);
    }

    /* Make IR as:
     *       bool _time_to_read = (READS >= WRITES) ? (_period > 0 && _period <= PERIODS) :
     *                                                (_period > 0 && _period <= PERIODS && _offset < READS)
             if (_time_to_read) {
                write_channel_intel(OUT_CHANNEL[buf], _tmp) where
                    _tmp = { DB_f0[!_idx][READ_FROM(_offset)][buf], DB_f1[!_idx][READ_FROM(_offset)][buf], ...}
                    or simply _tmp = DB[!_idx][READ_FROM(_offset)][buf] if there is only 1 type of buffer.
             }
     */
    void read_from_buffer(Stmt &new_body) {
        Expr buf_value;
        int num_types_of_buffers = buffers_info.size();
        internal_assert(num_types_of_buffers >= 1);
        if (num_types_of_buffers == 1) {
            // DB[!_idx][READ_FROM(_offset)][buf]
            buf_value = Call::make(TYPE, buffers_info[0].name, buffers_info[0].read_args, Call::PureIntrinsic);
        } else {
            // { DB_f0[!_idx][READ_FROM(_offset)][buf], DB_f1[!_idx][READ_FROM(_offset)][buf], ...}
            vector<Expr> fields;
            for (size_t i = 0; i < buffers_info.size(); i++) {
                Expr field = Call::make(buffers_info[i].type, buffers_info[i].name, buffers_info[i].read_args, Call::PureIntrinsic);
                fields.push_back(field);
            }
            buf_value = Call::make(TYPE, Call::make_struct, fields, Call::PureIntrinsic);
        }
        new_body = substitute(original_read_node, buf_value, new_body);

        // Recover the variables of the sequential loops
        Expr reads = _offset; // Total reads in the current period
        for(int i = READ_LOOPS.size() - 1; i >= 0; --i){
            auto &l = loops[READ_LOOPS[i]];
            string loop_name = std::get<0>(l);
            Expr loop_min = std::get<1>(l);
            Expr loop_extent = std::get<2>(l);
            Expr var;
            if (i == 0) {
                // Optimization: reads must already be within loop_extent. No need to %.
                var = reads + loop_min;
            } else {
                var = (reads % loop_extent) + loop_min;
            }
            new_body = LetStmt::make(loop_name, var, new_body);
            reads = reads / loop_extent;
        }

        new_body = IfThenElse::make(_time_to_read, new_body);

        Expr periods_unfinished = (_period <= PERIODS);
        Expr val = (READS >= WRITES) ? ((_period > 0) && periods_unfinished) :
                   ((_period > 0) && periods_unfinished && (_offset < Expr(READS)));
        new_body = LetStmt::make("_time_to_read", val, new_body);
    }
};

// Do scatter, or buffer, or both
class ScatterBufferInserter: public IRMutator{
    using IRMutator::visit;
private:
    const map<string, Function>& envs;
    const ScatterBufferArgs &scatterbuffer_args;
    const vector<tuple<string, Expr, Expr, ForType>> &all_loops;
public:
    ScatterBufferInserter(const map<string, Function>& _envs,
                          const ScatterBufferArgs & _scatterbuffer_args,
                          const vector<tuple<string, Expr, Expr, ForType>> &_all_loops):
        envs(_envs), scatterbuffer_args(_scatterbuffer_args),
        all_loops(_all_loops) {}

    Stmt visit(const ProducerConsumer *op) override{
        if(op->is_producer && scatterbuffer_args.find(op->name) != scatterbuffer_args.end()){
            auto iter = scatterbuffer_args.find(op->name);
            int cases = (iter->second.buffer_loop != "") * 2 + (iter->second.scatter_loop != "");
            Stmt new_body;
            switch(cases){
                case 1:{
                    debug(4)<<"Func "<< op->name <<": scatters data from "
                            << iter->second.producer <<" along loop "
                            << iter->second.scatter_loop << "\n";
                    ScatterInserter scatterInserter(envs, scatterbuffer_args,op->name);
                    new_body = scatterInserter.mutate(op->body);
                    break;
                }
                case 2:{
                    debug(4)<<"Func "<< op->name <<": buffers data from "
                            << iter->second.producer <<" at loop "
                            << iter->second.buffer_loop << "\n";
                    BufferInserter bufferInserter(envs, scatterbuffer_args,op->name);
                    new_body = bufferInserter.mutate(op->body);
                    break;
                }
                case 3:{
                    debug(4)<<"Func "<< op->name <<": buffers and scatters data from "
                            << iter->second.producer <<", buffers at loop "
                            << iter->second.buffer_loop << ", scatters along loop "
                            << iter->second.scatter_loop << "\n";
                    ScatterAndBuffer scatterAndBuffer(envs, all_loops, op->name,
                                                      iter->second.producer,
                                                      iter->second.buffer_loop,
                                                      iter->second.scatter_loop,
                                                      iter->second.read_node,
                                                      iter->second.read_condition,
                                                      iter->second.buffer_strategy,
                                                      iter->second.scatter_strategy,
                                                      iter->second.loops);
                    new_body =  scatterAndBuffer.mutate(op->body);
                    break;
                }
            }
            return ProducerConsumer::make(op->name,true,new_body);
        }
        return IRMutator::visit(op);
    }
};


// Check assumptions and collect info to be used for buffering and scattering
class ScatterBufferChecker: public IRVisitor{
    using IRVisitor::visit;

private:
    string func_name; // A function where a buffer is to be inserted
    ScatterBufferArgs& scatterbuffer_args;
    const map<string,Function>& env;
    vector<tuple<string, Expr, Expr, ForType>> &all_loops; // Full name, min, extent and type of all loops in the IR

    // Temporaries for the current Func
    vector<tuple<string, Expr, Expr, ForType>> loops; // Full name, min, extent and type of the loops
                                                      // around the read node in the current Func
    bool buffer_loop_seen; // True after a buffer loop has been seen for the current Func
    Expr path_condition; // Path condition to the current IR.

public:
    ScatterBufferChecker(ScatterBufferArgs& _scatterbuffer_args,const map<string, Function>& _env,
                         vector<tuple<string, Expr, Expr, ForType>> &_all_loops):
        scatterbuffer_args(_scatterbuffer_args), env(_env), all_loops(_all_loops) { func_name = ""; }
public:
    void visit(const ProducerConsumer *op) override{
        if(op->is_producer && scatterbuffer_args.find(op->name) != scatterbuffer_args.end()){
            func_name = op->name;
            loops.clear();
            buffer_loop_seen = false;
            path_condition = const_true();
            IRVisitor::visit(op);
            func_name = "";
            return;
        }
        IRVisitor::visit(op);
        return;
    }
    void visit(const Call *op) override{
        if(func_name != ""){
            auto iter = scatterbuffer_args.find(func_name);
            string producer = iter->second.producer;
            if(!op->args.empty() && op->args[0].as<StringImm>()){
                if(producer + ".channel" == op->args[0].as<StringImm>()->value){
                    internal_assert(!iter->second.read_node.defined());
                    iter->second.read_node = op;
                    iter->second.read_condition = path_condition;
                    iter->second.loops = loops;
                }
            }

        }
        IRVisitor::visit(op);
        return;
    }

    void visit(const For*op) override{
        // The scattering/buffering phase is placed after vectorization and vectorized loops are out of concern.
        internal_assert(op->for_type != ForType::Vectorized);

        IRMutator mutator;
        Expr min = mutator.mutate(op->min);
        Expr extent = mutator.mutate(op->extent);
        if (!ends_with(op->name, ".run_on_device")) {
            all_loops.push_back(tuple<string, Expr, Expr, ForType>(op->name, min, extent, op->for_type));
        }

        if(func_name != ""){
            auto iter = scatterbuffer_args.find(func_name);
            string scatter_loop = iter->second.scatter_loop;
            string buffer_loop = iter->second.buffer_loop;
            auto remove_iter = env.find(iter->second.producer);
            if(remove_iter != env.end()){
                auto remove_params = remove_iter->second.definition().schedule().remove_params();
                for(auto remove_param : remove_params){
                    if(ends_with(op->name,"." + remove_param)){
                        // This loop is removed in the producer side. In the consumer side, data should
                        // be buffered first, and then this loop READS the buffer repetitively so that
                        // the removal of the loop in the producer does not affect the data read.
                        // In other words, the buffer loop should be before this loop.
                        user_assert(buffer_loop != "")
                            << "Loop " << remove_param << " is removed in Func " << iter->second.producer
                            << " (producer), and a buffer is expected to insert in Func " << func_name
                            << " (consumer).\n";
                        user_assert(buffer_loop_seen)
                            << "Loop " << remove_param << " is removed in Func " << iter->second.producer
                            << " (producer), and a buffer is expected to insert in Func " << func_name
                            << " (consumer) at an outer loop level. However, the current loop to insert the buffer"
                            << ", " << buffer_loop << ", is not at an outer loop of loop " << remove_param << "\n";
                    }

                }
            }
            if(scatter_loop != "" && ends_with(op->name,"." + scatter_loop)){
                user_assert(op->for_type == ForType::Unrolled)
                    <<func_name<<" scatters data from "<<iter->second.producer <<" along loop "<<scatter_loop
                    <<". The loop is expected to be unrolled.\n";
                user_assert(op->min.as<IntImm>() && op->extent.as<IntImm>())
                    <<func_name<<" scatters data from "<<iter->second.producer <<" along loop "<<scatter_loop
                    <<". The loop is expected to have constant bounds.\n";
            }
            if (!ends_with(op->name, ".run_on_device")) {
                loops.push_back(tuple<string, Expr, Expr, ForType>(op->name, min, extent, op->for_type));
                if(buffer_loop != "" && ends_with(op->name,"." + buffer_loop)){
                    buffer_loop_seen = true;
                }
            }

            IRVisitor::visit(op);

            if (!ends_with(op->name, ".run_on_device")) {
                loops.pop_back();
            }
        } else {
            IRVisitor::visit(op);
        }
    }

    void visit(const Load *op) override{
        if(func_name != ""){
            auto iter = scatterbuffer_args.find(func_name);
            if(iter->second.buffer_loop != ""){
                string producer = iter->second.producer;
                if(producer == op->name + "_im" || producer == op->name){
                    user_assert(false)
                        << iter->second.producer << " sends data to " << func_name << " to buffer "
                        << " via memory, which is unexpected. The data should be transferred via a channel "
                        << " instead. To avoid this error, make sure both " << iter->second.producer
                        << " and " << func_name << " are declared with Place::Device.";
                }
            }
            if(iter->second.scatter_loop != ""){
                string producer = iter->second.producer;
                if(producer == op->name + "_im" || producer == op->name){
                    internal_assert(!iter->second.read_node.defined());
                    iter->second.read_node = op;
                    iter->second.read_condition = path_condition;
                    iter->second.loops = loops;
                }
            }
        }
        IRVisitor::visit(op);
        return;
    }

    void visit(const IfThenElse *op) override {
        if(func_name != ""){
            Expr old_condition = path_condition;

            path_condition = equal(old_condition, const_true()) ? op->condition : old_condition && op->condition;
            op->then_case.accept(this);

            if (op->else_case.defined()) {
                path_condition = equal(old_condition, const_true()) ? !op->condition : old_condition && !op->condition;
                op->else_case.accept(this);
            }

            path_condition = old_condition;
        } else {
            IRVisitor::visit(op);
        }
    }

    void visit(const Select *op) override {
        if(func_name != ""){
            Expr old_condition = path_condition;

            path_condition = equal(old_condition, const_true()) ? op->condition : old_condition && op->condition;
            op->true_value.accept(this);

            if (op->false_value.defined()) {
                path_condition = equal(old_condition, const_true()) ? !op->condition : old_condition && !op->condition;
                op->false_value.accept(this);
            }

            path_condition = old_condition;
        } else {
            IRVisitor::visit(op);
        }
    }
};

class ModifyScatterLoop : public IRMutator{
    using IRMutator::visit;
    const vector<string> &all_loops_to_serialize;
    map<string, Expr> loop2min; // Loop to serialize--> its min
public:
    ModifyScatterLoop(const vector<string> &all_loops_to_serialize) :
        all_loops_to_serialize(all_loops_to_serialize) {}

    Stmt visit(const For *op) override{
        if (std::find(all_loops_to_serialize.begin(), all_loops_to_serialize.end(), op->name) != all_loops_to_serialize.end()) {
            loop2min[op->name] = op->min;
            return For::make(op->name, op->min, op->extent, ForType::Serial, op->device_api, mutate(op->body));
        }
        return For::make(op->name, op->min, op->extent, op->for_type, op->device_api, mutate(op->body));
    }

    Expr visit(const Call *op) override{
        if(op->is_intrinsic(Call::write_channel) || op->is_intrinsic(Call::read_channel)) {
            vector<Expr> new_args;
            new_args.push_back(op->args[0]); // channel name
            size_t offset = 1;
            if (op->is_intrinsic(Call::write_channel)) {
                new_args.push_back(mutate(op->args[1])); // data
                offset = 2;
            }
            for (size_t i = offset; i < op->args.size(); i++) {
                Expr arg = op->args[i];
                if (arg.as<Variable>()) {
                    const string &loop = arg.as<Variable>()->name;
                    if (loop2min.find(loop) != loop2min.end()) {
                        Expr min = loop2min.at(loop);
                        arg = substitute(loop, min, arg);
                    }
                }
                new_args.push_back(arg);
            }
            return Call::make(op->type, op->name, new_args, op->call_type, op->func, op->value_index, op->image, op->param);
        }
        return IRMutator::visit(op);
    }
};

/*
// Send one more period of dummy data from a producer to its consumer to buffer
class OneMorePeriod : public IRMutator {
    using IRMutator::visit;
private:
    bool in_producer;     // In the definition of a func that produces data for its consumer to buffer
    string producer_name; // The name of the func that produces data for its consumer to buffer
    string buffer_loop; // The loop under which a buffer is inserted in the consumer side
    string innermost_loop; // The innermost loop name
    vector<string> loops; // All the loops
    vector<Expr> extents; // The original extents of the loops

    const ScatterBufferArgs     &scatterbuffer_args;
    const map<string, Function> &env;

public:
    Stmt visit(const ProducerConsumer *op) override {
        bool old_in_producer = in_producer;
        if(op->is_producer) {
            for (auto a : scatterbuffer_args) {
                if (a.second.producer == op->name) {
                    in_producer = true;
                    producer_name = op->name;
                    buffer_loop = a.second.buffer_loop;
                }
            }
        }
        if (in_producer) {
            Stmt s= ProducerConsumer::make(op->name, op->is_producer, mutate(op->body));
            in_producer = old_in_producer;
            return s;
        } else {
            return IRMutator::visit(op);
        }
    }

    Stmt visit(const For *op) override {
        if (!in_producer) {
            return IRMutator::visit(op);
        }

        // Skip the dummy loops that only tell the compiler this is a device function
        bool dummy_loop = ends_with(op->name, ".run_on_device");
        if (!dummy_loop) {
            loops.push_back(op->name);
        }

        Stmt body = IRMutator::mutate(op->body);
        Stmt s;
        if (!dummy_loop) {
            if (innermost_loop == op->name) {
                // if (first loop < its extent || (second until buffer loop all equal 0) body
                Expr cond1 = Expr(loops[0]) < extents[0];
                Expr cond2;
                for (size_t i = 0; i < loops.size(); i++) {
                    cond2 = (i == 0) ? (Expr(loops[i]) < extents[i]) : (cond2 && (Expr(loops[i]) < extents[i]));
                    if (loops[i] == buffer_loop) {
                        break;
                    }
                }
                body = IfThenElse::make(cond1 || cond2, body, Stmt());
            }
            if (loops.size() == 1) {
                // The outermost loop. Increase its extent by 1
                s = For::make(op->name, op->min, op->extent + 1, op->for_type, op->device_api, body);
            }
            loops.pop_back();
            return s;
        } else {
            s = For::make(op->name, op->min, op->extent, op->for_type, op->device_api, body);
            return s;
        }
    }

    Expr visit(const Call *op) override {
        // TODO: here we should catch calls with side effects, or memory reads, and change them into
        // something like select(cond, op, dummy) instead. No need to change write_channel.
        // Also put the cond at the beginning of the innermost loop.
        innermost_loop = loops.back();
        if(op->is_intrinsic(Call::write_channel) && op->args[0].as<StringImm>()->value == (producer_name + ".channel")){
            // Change the value to write to the channel as
            //      select(first loop < its extent, current value, a dummy value);
            Expr value = op->args[1];
            Expr condition = Expr(loops[0]) < extents[0];
            Expr dummy =
            op->args[1] = Select::make(condition, value, )
        } else {
            return IRMutator::visit(op);
        }
    }

public:
    OneMorePeriod(ScatterBufferArgs& _scatterbuffer_args,const map<string, Function>& _env):
        scatterbuffer_args(_scatterbuffer_args), env(_env){ in_producer = false; }
};
*/

void get_ScatterBufferArgs(const map<string, Function> &envs, ScatterBufferArgs& scatterbuffer_args){
    for(const auto &e : envs){
        Function func = e.second;
        auto buffer_params = func.definition().schedule().buffer_params();
        auto scatter_params = func.definition().schedule().scatter_params();
        if(buffer_params.empty() && scatter_params.empty()) {
            continue;
        }
        internal_assert(buffer_params.size() <= 1 && scatter_params.size() <= 1);
        
        if(!buffer_params.empty()){
            if(!scatter_params.empty()){
                user_assert(buffer_params[0].func_name == scatter_params[0].func_name)
                    <<"Func " << func.name() << " buffers data from Func " << buffer_params[0].func_name
                    << ", but scatters data from another Func " << scatter_params[0].func_name
                    << ". We require the data to buffer and scatter are from the same producer.";
            }
        }

        ScatterBufferArg tmp;
        if(!buffer_params.empty()){
            tmp.producer = buffer_params[0].func_name;
            tmp.buffer_loop = buffer_params[0].loop_name;
            tmp.buffer_strategy = buffer_params[0].strategy;
        }

        if(!scatter_params.empty()){
            tmp.producer = scatter_params[0].func_name;
            tmp.scatter_loop = scatter_params[0].loop_name;
            tmp.scatter_strategy = scatter_params[0].strategy;
        }

        debug(4) <<  tmp.producer << " --> " << func.name() << ": "
                 << "buffer at loop ("<< tmp.buffer_loop << "), "
                 << "scatter along loop (" << tmp.scatter_loop << ")\n";
        scatterbuffer_args.insert({func.name(),tmp});
    }
    return;
}

// Suppose func p is isolated out of func c, and both funcs are on device. Both funcs have the same
// loop structure, because of isolation. Suppose there is an unrolled loop l with the extent of L,
// and here we want to scatter along dimension l in func c. Without scattering, func p and c
// both have L PEs; func p is writing into L channels, and func c reads from these L channels.
// After scattering, all the PEs of func p are collapsed into a single PE; this single PE of func p
// writes into 1 channel, and the first PE of func c reads that channel and distributes the data
// to the other PEs.
// If another func p1 is isolated out of func p, dimension l in func p1 should also be "collapsed",
// since our principle of isolation is that producer and consumer have the same loop structure. In
// general, we need collapse dimension l for the producer chain isolated out of func c.
// Note: below we do not check if a Func is on device or not. So loops on host can be serializd as well.
void find_loops_to_serialize_by_scattering(const Function &func, const string &scatter_loop,
                                           const map<string, Function> &env,
                                           vector<string> &all_loops_to_serialize) {
    // Find the scatter loop in the producer chain of the func
    for (auto &e : env) {
        const Function &p = e.second;
        if (/*p.place() == Place::Device &&*/ p.isolated_from_as_producer() == func.name()) {
            // p is on the device and isolated from func. Make the scatter_loop as serial in p.
            all_loops_to_serialize.push_back(p.name() + ".s0." + scatter_loop);
            find_loops_to_serialize_by_scattering(p, scatter_loop, env, all_loops_to_serialize);
            return;
        }
    }
}

}

void find_loops_to_serialize_by_scattering(const std::map<std::string, Function> &env,
                                           std::vector<std::string> &all_loops_to_serialize) {
    for (auto e : env) {
        auto &func = e.second;
        const std::vector<ScatterItem>& scatter_params = func.definition().schedule().scatter_params();
        internal_assert(scatter_params.size() < 2); // Currently a func can only have zero or one time of scatter
        if (scatter_params.size() > 0) {
            string scatter_loop = scatter_params[0].loop_name;
            // Find the scatter loop in the  entire producer chain of func
            find_loops_to_serialize_by_scattering(func, scatter_loop, env, all_loops_to_serialize);
        }
    }
}


// Scatter, buffer, or both.
Stmt scatter_buffer(Stmt s, const std::map<std::string, Function> &env){
    // Get arguments for scattering and buffering
    ScatterBufferArgs scatterbuffer_args;
    get_ScatterBufferArgs(env, scatterbuffer_args);
    if(scatterbuffer_args.empty()) {
        return s;
    }

    // Check for assumptions, as well as getting the info of all the loops
    // in the entire IR (Full names, mins, extents and types), read nodes and
    // path conditions to the read nodes.
    vector<tuple<string, Expr, Expr, ForType>> all_loops;
    ScatterBufferChecker sbc(scatterbuffer_args,env, all_loops);
    s.accept(&sbc);

    // Implement scattering, buffering, or both
    ScatterBufferInserter sbi(env, scatterbuffer_args, all_loops);
    s = sbi.mutate(s);

/*
    // For double buffering, a producer needs send one more period of trash data to the consumer
    OneMorePeriod omp(env, scatterbuffer_args);
    s = omp.mutate(s);
*/
    // Finalize scatter loops.
    // First, find all the loops to be serialized in the entire IR.
    vector<string> all_loops_to_serialize;
    find_loops_to_serialize_by_scattering(env, all_loops_to_serialize);
    ModifyScatterLoop msl(all_loops_to_serialize);
    s = msl.mutate(s);

    return s;
}

}
}