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
#include "../../Halide/src/Var.h"
#include "../../Halide/src/IR.h"
#include "../../Halide/src/Func.h"
#include "../../Halide/src/IREquality.h"
#include "../../Halide/src/IRVisitor.h"
#include "../../Halide/src/IRMutator.h"
#include "../../Halide/src/IRPrinter.h"
#include "../../Halide/src/Simplify.h"
#include "../../Halide/src/Substitute.h"
#include "../../Halide/src/FindCalls.h"
#include "../../Halide/src/CSE.h"
#include "Simplify.h"
#include "InjectHostDevBufferCopies.h"
#include "DebugPrint.h"
#include "Utilities.h"
#include <algorithm>
#include <math.h>

namespace Halide {
namespace Internal {

typedef struct ForLoopContainer {
    std::string name;
    int64_t extent;
} ForLoopContainer_t;


class SimplifyBody : public IRMutator {
 private:
    std::vector<ForLoopContainer_t> &flattened_loops;
    std::string fuse_var;
 protected:
    using IRMutator::visit;
    Expr visit(const Add *op) override {
        Expr new_a = mutate(op->a);
        Expr new_b = mutate(op->b);
        std::vector<ForLoopContainer_t> scope;
        Expr ret = Add::make(new_a, new_b);
        Expr expr = ret;
        while (expr.as<Add>() != nullptr) {
            debug(4) << "in " << expr << "\n";
            const Add *cur = expr.as<Add>();
            const Variable *a_as_var = cur->a.as<Variable>();
            const Mul *a_as_mul = cur->a.as<Mul>();
            if (a_as_var != nullptr) {
                ForLoopContainer_t f = {a_as_var->name, 1};
                scope.push_back(f);
            } else if (a_as_mul != nullptr) {
                const Variable *aa_as_var = a_as_mul->a.as<Variable>();
                const IntImm *ab_as_int = a_as_mul->b.as<IntImm>();
                if (aa_as_var != nullptr && ab_as_int != nullptr) {
                    ForLoopContainer_t f = {aa_as_var->name, ab_as_int->value};
                    scope.push_back(f);
                } else {
                    // left is not a var * int
                    return ret;
                }
            } else {
                // left is not var or mul
                return ret;
            }

            expr = cur->b;
        }

        const Variable *as_var = expr.as<Variable>();
        if (as_var != nullptr) {
            ForLoopContainer_t f = {as_var->name, 1};
            scope.push_back(f);
        } else {
            return ret;
        }

        if ((int)scope.size() == (int)flattened_loops.size()) {
            int num_loop = (int)flattened_loops.size();
            bool match = true;
            for (int i = 0; i < num_loop; ++i) {
                int factor = 1;
                for (int j = i + 1; j < num_loop; ++j) {
                    factor *= flattened_loops[j].extent;
                }
                if ((flattened_loops[i].name != scope[i].name) ||
                    (factor != scope[i].extent)) {
                    match = false;
                    break;
                }
            }
            if (match) {
                debug(4) << "Flatten simplify " << ret << " to " << fuse_var << "\n";
                return Variable::make(Int(32), fuse_var);
            }
        }
        return ret;
    }

 public:
    SimplifyBody(std::vector<ForLoopContainer_t> &_flattened_loops, std::string _fuse_var) :
        flattened_loops(_flattened_loops), fuse_var(_fuse_var) {
            internal_assert((int)flattened_loops.size() > 0);
        }
};


class FlattenConstLoops : public IRMutator {
    using IRMutator::visit;

    Stmt visit(const For* op) override {
        // std::vector<std::string> non_pow2_loops;
        // std::vector<int> non_pow2_loops_extents;
        // std::vector<int> non_pow2_loops_level;
        if (op->body.as<For>() && op->for_type == ForType::Serial && !ends_with(op->name, ".infinite") && is_const(op->extent) && is_const(op->min)) {
            // bool some_non_pow2 = false;
            std::string name = op->name;
            int  final_extent = op->extent.as<IntImm>()->value;
            // if (!isPowerOfTwo(final_extent)) {
            //     some_non_pow2 = true;
            //     non_pow2_loops.push_back(op->name);
            //     non_pow2_loops_extents.push_back(op->extent.as<IntImm>()->value);
            //     non_pow2_loops_level.push_back(0);
            //     final_extent = static_cast<int>(nextPowerOf2(static_cast<unsigned int>(final_extent)));
            //     debug(2) << "Converted " << op->extent.as<IntImm>()->value << " to " << final_extent << "\n";
            // }
            const For* cop = op->body.as<For>();
            std::vector<ForLoopContainer_t> flattened_loops;
            // std::vector<int64_t> extents_prod_top_to_bot;
            Stmt body = op->body;
            if (cop != nullptr && !is_const(cop->extent)) {
                cop = nullptr;
            } else {
                // extents_prod_top_to_bot.push_back(final_extent);
                // ForLoopContainer_t f = {op->name, static_cast<int>(nextPowerOf2(static_cast<unsigned int>(final_extent)))};
                // ForLoopContainer_t f = {op->name, final_extent};
                // flattened_loops.push_back(f);
            }
            ForLoopContainer_t f = {op->name, final_extent};
            flattened_loops.push_back(f);
            std::string postfix = "";

            // if (cop!=nullptr && is_const(cop->extent)) {
            //    name = name + "." + undecorated_loop_var(op->name);
            //    postfix = ".f";
            // }

            debug(4) << "Final extent: " << final_extent << " loop: " << op->name << "\n";
            while (cop != nullptr && is_const(cop->extent) && is_const(cop->min) && cop->for_type == ForType::Serial) {
                // user_assert(is_const(cop->extent) && is_const(cop->min)) << "For loop: " << cop->name << " does not have a const extent\n";
                // if (!isPowerOfTwo(cop->extent.as<IntImm>()->value)) {
                //     some_non_pow2 = true;
                //     non_pow2_loops.push_back(cop->name);
                //     non_pow2_loops_extents.push_back(cop->extent.as<IntImm>()->value);
                //     non_pow2_loops_level.push_back(flattened_loops.size());
                //     final_extent = final_extent * static_cast<int>(nextPowerOf2(static_cast<unsigned int>(cop->extent.as<IntImm>()->value)));
                // } else {
                //     final_extent = final_extent * (cop->extent.as<IntImm>()->value);
                // }
                final_extent = final_extent * (cop->extent.as<IntImm>()->value);
                debug(4) << "Final extent: " << final_extent << " loop: " << cop->name << "\n";
                // extents_prod_top_to_bot.push_back(final_extent);
                name = name + "." + extract_after_tokens(cop->name, 2);
                body = cop->body;
                ForLoopContainer_t f = {cop->name, static_cast<int>(cop->extent.as<IntImm>()->value)};
                flattened_loops.push_back(f);
                cop = cop->body.as<For>();
                if (cop != nullptr && !is_const(cop->extent)) {
                    cop = nullptr;
                }
            }
            name = name + postfix;

            int num_loops = (int)flattened_loops.size();
            if (num_loops == 1) {
                // Not flattened with any inner loop.
                return op;
            }

            // At this point the body is not a For loop, but however, it can consist of for loops, so mutate the body
            // simplify the body
            SimplifyBody sb(flattened_loops, name);
            debug(4) << "Flatten simplify body...\n";
            body = sb.mutate(body);
            debug(4) << body << "\n";
            body = mutate(body);

            // if (some_non_pow2) {
            //     name = name + "._no_loop_counter";
            //     // Inserting provide loop_name._no_loop_counter.temp(0) = call loop_name._no_loop_counter.temp(0) + 1
            //     // at the end of the loop body
            //     Expr update = Call::make(Int(32), name + ".temp", {IntImm::make(Int(32), 0)}, Call::Intrinsic);
            //     update = Add::make(update, IntImm::make(Int(32), 1));
            //     Stmt s = Provide::make(name + ".temp", {update}, {IntImm::make(Int(32), 0)});
            //     body = Block::make(body, s);
            // }
            // Traverse the loops with non power of 2 bounds inside out and insert conditional bumps to the loop counter like this:
            // if there are 4 nested levels of loops with bounds 2, 3, 4, 5, 2 then loop extent will be 2*4*4*8*2 = 512 and each loop will
            // be assigned 1, 2, 2, 3, 1 bits respectively in the loop counter. whenever loop_counter[3:1] becomes 101 we need to add
            // (8-5)*2 to the loop counter. Then we need to check if the bits loop_counter[7:6] become 11, at that time we need to add
            // (4-3)*4*8*2 to the loop counter
            // for (int i = non_pow2_loops.size()-1; i >= 0; i--) {
            //     Expr update = Call::make(Int(32), name + ".temp", {IntImm::make(Int(32), 0)}, Call::Intrinsic);
            //     int mul_factor = final_extent/extents_prod_top_to_bot[non_pow2_loops_level[i]];
            //     int offset = flattened_loops[non_pow2_loops_level[i]].extent - non_pow2_loops_extents[i];
            //     debug(2) << "name: " << name << ".temp mul_factor: " << mul_factor << " offset: " << offset << " ex prod: "
            //             << extents_prod_top_to_bot[non_pow2_loops_level[i]] << " loop level: " << non_pow2_loops_level[i] << "\n";
            //     update = Add::make(update, IntImm::make(Int(32), offset*mul_factor));
            //     Stmt s = Provide::make(name + ".temp", {update}, {IntImm::make(Int(32), 0)});
            //     Expr mask;
            //     if (non_pow2_loops_level[i] != 0)
            //         mask = IntImm::make(Int(32), (final_extent/extents_prod_top_to_bot[non_pow2_loops_level[i]-1])-1);
            //     else
            //         mask = IntImm::make(Int(32), final_extent-1);
            //     Expr cond = Call::make(Int(32), name + ".temp", {IntImm::make(Int(32), 0)}, Call::Intrinsic);
            //     cond = Call::make(Int(32), Call::bitwise_and, {cond, mask}, Call::Intrinsic);
            //     //cond = Div::make(cond, IntImm::make(Int(32), final_extent/extents_prod_top_to_bot[non_pow2_loops_level[i]]));
            //     cond = EQ::make(cond, IntImm::make(Int(32), mul_factor*non_pow2_loops_extents[i]));
            //     s = IfThenElse::make(cond, s, Stmt());
            //     body = Block::make(body, s);
            // }
            for (int i = num_loops-1; i >= 0; i--) {
                Expr value = Variable::make(Int(32), name);
                int mod_part = 1;
                for (int j = i; j < num_loops; ++j) {
                    mod_part *= flattened_loops[j].extent;
                }
                int div_part = mod_part/flattened_loops[i].extent;
                debug(4) << "mod part = " << mod_part << ", div part = " << div_part << "\n"; 
                // value = Sub::make(IntImm::make(Int(32), (i != 0) ? (final_extent/extents_prod_top_to_bot[i-1]) : final_extent), IntImm::make(Int(32),1));
                // value = Call::make(Int(32), Call::bitwise_and, {Variable::make(Int(32), name), value}, Call::Intrinsic);
                value = Mod::make(value, mod_part);
                // value = Div::make(value, IntImm::make(Int(32), final_extent/extents_prod_top_to_bot[i]));
                value = Div::make(value, div_part);
                debug(4) << "check body=\n" << body << "\n";
                body = LetStmt::make(flattened_loops[i].name, value, body);
            }
            Stmt stmt = For::make(name, op->min, final_extent, op->for_type, op->device_api, body);
            // if (some_non_pow2) {
            //     temps.push_back(name + ".temp");
            //     //stmt = Realize::make(name + ".temp", {Int(32)}, {Range(0,1)}, const_true(), stmt);
            // }
            return stmt;
        } else {
            return IRMutator::visit(op);
        }
    }
    
public:
    std::vector<std::string> temps;
    FlattenConstLoops() { temps.clear(); }
};

class ConstLoopFlattening : public IRMutator {
    using IRMutator::visit;
    bool is_open_cl;

    Stmt visit(const ProducerConsumer* op) override {
        is_open_cl = false;
        Stmt stmt = IRMutator::visit(op);
        is_open_cl = false;
        return stmt;
    }

    Stmt visit(const For* op) override {
        if ((op->for_type != ForType::Serial || !is_open_cl) && op->device_api == DeviceAPI::OpenCL) {
            is_open_cl = true;
            return IRMutator::visit(op);
        } else {
            FlattenConstLoops fl;
            Stmt stmt = fl.mutate(op);
            for (auto temp : fl.temps) {
                stmt = Realize::make(temp, {Int(32)}, MemoryType::Auto, {Range(0,1)}, const_true(), stmt);
            }
            return stmt;
        }
    }
};

typedef struct DynamicForLoopContainer {
    std::string name;
    Expr extent;
} DynamicForLoopContainer_t;

class FlattenDynamicLoops : public IRMutator {
    using IRMutator::visit;

    Stmt visit(const For* op) override {
        if (op->body.as<For>() && op->for_type == ForType::Serial && !ends_with(op->name, ".infinite") && !is_const(op->extent) && is_const(op->min)) {
            std::string name = op->name;
            const For* cop = op->body.as<For>();
            Stmt body;
            body = op->body;
            std::vector<DynamicForLoopContainer_t> flattened_loops;
            Expr final_extent = op->extent;
            std::vector<Expr> extents_prod_top_to_bot = {final_extent};
            DynamicForLoopContainer_t f = {op->name, op->extent};
            flattened_loops.push_back(f);
            temps.push_back(op->name);
            std::vector<Stmt> loop_inits;
            loop_inits.push_back(Provide::make(op->name + ".temp", {IntImm::make(Int(32), 0)}, {IntImm::make(Int(32), 0)}));
            std::string postfix = "";
            if (cop!=nullptr && !is_const(cop->extent)) {
                name = name + "." + extract_after_tokens(op->name, 2);
                postfix = ".f";
            }
            while (cop != nullptr && !is_const(cop->extent)) {
                user_assert(is_const(cop->min) && !is_const(op->extent)) << "For loop: " << cop->name << " has a const extent\n";
                    final_extent = Mul::make(final_extent, cop->extent);
                extents_prod_top_to_bot.push_back(final_extent);
                temps.push_back(cop->name);
                loop_inits.push_back(Provide::make(cop->name + ".temp", {IntImm::make(Int(32), 0)}, {IntImm::make(Int(32), 0)}));
                name = name + "." + extract_after_tokens(cop->name, 2);
                body = cop->body;
                DynamicForLoopContainer_t f = {cop->name, cop->extent};
                flattened_loops.push_back(f);
                cop = cop->body.as<For>();
            }
            name = name + postfix;
            // At this point the body is not a For loop, but however, it can consist of for loops, so mutate the body
            body = mutate(body);
            Stmt s;
            for (size_t i = 0; i < flattened_loops.size(); i++) {
                Expr cond = EQ::make(Call::make(Int(32), flattened_loops[i].name + ".temp", {IntImm::make(Int(32), 0)}, Call::Intrinsic),
                        flattened_loops[i].extent);
                Stmt reinit = Provide::make(flattened_loops[i].name + ".temp", {IntImm::make(Int(32),0)}, {IntImm::make(Int(32), 0)});
                if (i!=0) {
                    s = Block::make(reinit, s);
                } else {
                    s = reinit;
                }
                s = IfThenElse::make(cond, s, Stmt());
                Stmt increment = Provide::make(flattened_loops[i].name + ".temp",  {Add::make(
                                        Call::make(Int(32), flattened_loops[i].name + ".temp", {IntImm::make(Int(32), 0)}, Call::Intrinsic),
                                        IntImm::make(Int(32),1))}, {IntImm::make(Int(32), 0)});
                s = Block::make(increment, s);
            }
            body = Block::make(body, s);
            Stmt stmt = For::make(name, op->min, final_extent, op->for_type, op->device_api, body);
            return stmt;
        } else {
            return IRMutator::visit(op);
        }
    }
    
public:
    std::vector<std::string> temps;
    FlattenDynamicLoops() { temps.clear(); }
};


class DynamicLoopFlattening : public IRMutator {
    using IRMutator::visit;
    bool is_open_cl;

    Stmt visit(const ProducerConsumer* op) override {
        is_open_cl = false;
        Stmt stmt = IRMutator::visit(op);
        is_open_cl = false;
        return stmt;
    }
    Stmt visit(const For* op) override {
        if (op->for_type != ForType::Serial || !is_open_cl) {
            if (op->device_api == DeviceAPI::OpenCL)
                is_open_cl = true;
            return IRMutator::visit(op);
        } else {
            FlattenDynamicLoops fl;
            Stmt stmt = fl.mutate(op);
            // replace each use of the loop variables by the new temp variables
            for (auto temp : fl.temps) {
                stmt = substitute(temp, Call::make(Int(32), temp + ".temp", {IntImm::make(Int(32), 0)}, Call::Intrinsic), stmt);
            }
            for (auto temp : fl.temps) {
                stmt = Realize::make(temp + ".temp", {Int(32)}, MemoryType::Auto, {Range(0,1)}, const_true(), stmt);
            }
            return stmt;
        }
    }
};
/* For a loop like this
 * 
 * for i = 0..I
 *   s0
 *   s1
 *   for x = 0...X
 *      A
 *   s2
 *   s3
 *   for y = 0...Y
 *      B
 *   s4
 *   for z = 0...Z
 *      C
 *   s5
 *   
 * We want to generate a code like this:
 * 
 * for i = 0...I
 *   for xyz = 0...X+Y+Z
 *      x = xyz;
 *      if (xyz < X) {
 *         if (xyz == 0) {
 *            s0;
 *            s1;
 *         }
 *         A
 *         if (xyz == X-1) {
 *            s2;
 *            s3;
 *            y = 0;
 *         }
 *      }
 *      else {
 *        if (xyz < X+Y) {
 *           B
 *           y++;
 *           if (y == Y) {
 *             s4;
 *             z = 0;
 *           }
 *        }
 *        else {
 *           C
 *           z++;
 *           if (z == Z) {
 *             s5;
 *           } 
 *        }
 *      }
 *        
 */
class MergeLoops : public IRMutator {
    using IRMutator::visit;

    Stmt visit(const For* op) override {
        if (op->body.as<Block>()) {
            Expr current_iter = IntImm::make(Int(32), 0);
            const Block* blk = op->body.as<Block>();
            std::vector<Expr> loop_mins;
            std::vector<Expr> loop_maxs;
            std::vector<Expr> loop_extents;
            std::vector<std::string> loop_names;
            std::vector<Stmt> loop_bodies;
            std::vector<std::vector<Stmt>> non_for_stmts_after_a_loop;
            const Block* old_blk = blk;
            std::string name = "";
            int num_loops = 0;
            while (blk != nullptr) {
                if (!blk->first.as<For>()) {//(blk->first.as<Provide>() || blk->first.as<IfThenElse>() || blk->first.as<Evaluate>()) {
                    non_for_stmts_after_a_loop.resize(num_loops+1);
                    non_for_stmts_after_a_loop[num_loops].push_back(blk->first);
                } else if (blk->first.as<For>()) {
                    const For* cop = blk->first.as<For>();
                    loop_mins.push_back(current_iter);
                    current_iter = Add::make(current_iter, cop->extent);
                    loop_maxs.push_back(current_iter);
                    loop_extents.push_back(cop->extent);
                    if (num_loops!=0)
                        name = name + extract_after_tokens(cop->name, 2);
                    else
                        name = cop->name;
                    loop_names.push_back(cop->name);
                    // mutate the loop bodies before inserting
                    Stmt temp_body = mutate(cop->body);
                    // replace the accesses to loop variables with the call nodes
                    if (num_loops!=0) {
                        temp_body = substitute(cop->name, Call::make(Int(32), cop->name + ".temp", {IntImm::make(Int(32), 0)}, Call::Intrinsic), temp_body);
                        temps.push_back(cop->name);
                    }
                    loop_bodies.push_back(temp_body);
                    num_loops++;
                    non_for_stmts_after_a_loop.resize(num_loops+1);
                } else {
                    user_assert(false) << "Should not end up here\n";
                }
                old_blk = blk;
                blk = blk->rest.as<Block>();
                if (blk == nullptr) {
                    if (old_blk->rest.as<Provide>() || old_blk->rest.as<IfThenElse>() || old_blk->rest.as<Evaluate>()) {
                        non_for_stmts_after_a_loop.resize(num_loops+1);
                        non_for_stmts_after_a_loop[num_loops].push_back(old_blk->rest);
                    } else if (old_blk->rest.as<For>()) {
                        const For* cop = old_blk->rest.as<For>();
                        loop_mins.push_back(current_iter);
                        current_iter = Add::make(current_iter, cop->extent);
                        loop_maxs.push_back(current_iter);
                        loop_extents.push_back(cop->extent);
                        if (num_loops!=0)
                            name = name + extract_after_tokens(cop->name, 2);
                        else
                            name = cop->name;
                        loop_names.push_back(cop->name);
                        // mutate the loop bodies before inserting
                        Stmt temp_body = mutate(cop->body);
                        // replace the accesses to loop variables with the call nodes
                        if (num_loops!=0) {
                            temp_body = substitute(cop->name, Call::make(Int(32), cop->name + ".temp", {IntImm::make(Int(32), 0)}, Call::Intrinsic), temp_body);
                            temps.push_back(cop->name);
                        }
                        loop_bodies.push_back(temp_body);
                        num_loops++;
                        non_for_stmts_after_a_loop.resize(num_loops+1);
                    }
                }
            }
            // If there was no loop in the block then just return the body as it is
            if (loop_names.size()==0) {
                Stmt stmt = op;
                return stmt;
            } else { // replace the use of the first loop variable in the loop body as the fused variable
                loop_bodies[0] = substitute(loop_names[0], Variable::make(Int(32), name), loop_bodies[0]);
            }
            // If there were at least two for loops at same level in the block
            Stmt s;
            if (loop_names.size() >= 2) {
                for (int i = loop_names.size()-2; i >= 0; i--) {
                    // (xyz < X+Y) for i = 1 and (xyz < X) for i = 0
                    Expr cond = LT::make(Variable::make(Int(32), name), loop_maxs[i]);
                    // last two for loops need special handling as they are the base case of the if-else tree being produced
                    // int this for loop. The recursive implementation of if-else tree breaks here.
                    if (i == (static_cast<int>(loop_names.size())-2)) {
                        Stmt increment0;
                        if (i!=0) {
                             // generate y++
                             increment0 =  Provide::make(loop_names[i] + ".temp",  {Add::make(
                                Call::make(Int(32), loop_names[i] + ".temp", {IntImm::make(Int(32), 0)}, Call::Intrinsic),
                                IntImm::make(Int(32),1))}, {IntImm::make(Int(32), 0)});
                        }
                        // generate z++
                        Stmt increment1 =  Provide::make(loop_names[i+1] + ".temp",  {Add::make(
                                Call::make(Int(32), loop_names[i+1] + ".temp", {IntImm::make(Int(32), 0)}, Call::Intrinsic),
                                IntImm::make(Int(32),1))}, {IntImm::make(Int(32), 0)});
                        // {B; y++}
                        Stmt blk1;
                        if (increment0.defined())
                            blk1 = Block::make(loop_bodies[i], increment0);
                        else
                            blk1 = loop_bodies[i];
                        // z = 0
                        Stmt init_next_loop = Provide::make(loop_names[i+1] + ".temp", {IntImm::make(Int(32),0)}, {IntImm::make(Int(32), 0)});
                        // {s4; z=0;}
                        if (non_for_stmts_after_a_loop[i+1].size()!=0)
                            init_next_loop = Block::make(Block::make(non_for_stmts_after_a_loop[i+1]), init_next_loop);
                        Expr init_next_cond;
                        if (i!=0) {
                            // (y == Y)
                            init_next_cond = EQ::make(Call::make(Int(32), loop_names[i] + ".temp", {IntImm::make(Int(32), 0)}, Call::Intrinsic), loop_extents[i]);
                        }
                        else {
                            init_next_cond = EQ::make(Variable::make(Int(32), name), loop_maxs[i]);
                        }
                        // if (y==Y) { s4; z=0; }
                        init_next_loop = IfThenElse::make(init_next_cond, init_next_loop, Stmt());
                        // {B; y++; if (y==Y) {s4; z=0;} }
                        blk1 = Block::make(blk1, init_next_loop);
                        // {C; z++;}
                        Stmt blk2 = Block::make(loop_bodies[i+1], increment1);
                        if (non_for_stmts_after_a_loop[i+2].size()!=0) {
                            // {C; z++, if (z == Z) {s5;} }
                            blk2 = Block::make(blk2, IfThenElse::make(
                                    EQ::make(Call::make(Int(32), loop_names[i+1] + ".temp", {IntImm::make(Int(32), 0)}, Call::Intrinsic), loop_extents[i+1]),
                                    Block::make(non_for_stmts_after_a_loop[i+2]), Stmt()
                                    ));
                        }
                        // if (xyz < X+Y) {B; y++; z=0;} else {C; z++; if (z==Z) {s5;} }
                        s = IfThenElse::make(cond, blk1, blk2);
                    } else {
                        // Add the statements before the first for loop
                        Stmt start_block;
                        if (i==0 && non_for_stmts_after_a_loop[0].size()!=0) {
                            // {s0; s1;}
                            start_block = Block::make(non_for_stmts_after_a_loop[0]);
                            // (xyz == 0)
                            Expr start_cond = EQ::make(Variable::make(Int(32), name), IntImm::make(Int(32), 0));
                            // if (xyz == 0) {s0; s1;}
                            start_block = IfThenElse::make(start_cond, start_block, Stmt());
                        }
                        // Add the increment condition for the for loops
                        Stmt increment0;
                        if (i!=0) {
                            increment0 = Provide::make(loop_names[i] + ".temp",  {Add::make(
                                Call::make(Int(32), loop_names[i] + ".temp", {IntImm::make(Int(32), 0)}, Call::Intrinsic),
                                IntImm::make(Int(32),1))}, {IntImm::make(Int(32), 0)});
                        }
                        // Init the loop variable of next loop in the last iteration of current loop
                        // y = 0;
                        Stmt init_next_loop = Provide::make(loop_names[i+1] + ".temp", {IntImm::make(Int(32),0)}, {IntImm::make(Int(32), 0)});
                        // Add the provide/if else statements after this loop to be executed in last iteration as well.
                        // {s2; s3; y=0;}
                        if (non_for_stmts_after_a_loop[i+1].size()!=0)
                            init_next_loop = Block::make(Block::make(non_for_stmts_after_a_loop[i+1]), init_next_loop);
                        Stmt blk1;
                        if (i!=0) {
                            Expr init_next_cond = EQ::make(Call::make(Int(32), loop_names[i] + ".temp", {IntImm::make(Int(32), 0)}, Call::Intrinsic), loop_maxs[i]);
                            init_next_loop = IfThenElse::make(init_next_cond, init_next_loop, Stmt());
                            blk1 = Block::make({loop_bodies[i], increment0, init_next_loop});
                        } else {
                            // (xyz == X-1)
                            Expr init_next_cond = EQ::make(Variable::make(Int(32), name), Sub::make(loop_maxs[i], IntImm::make(Int(32),1)));
                            // if (yyz == X-1) { s2; s3; y=0; }
                            init_next_loop = IfThenElse::make(init_next_cond, init_next_loop, Stmt());
                            // { if (xyz == 0) {s0; s1;} A; if (xyz == X-1) { s2; s3; y=0; } }
                            if (start_block.defined())
                                blk1 = Block::make({start_block, loop_bodies[i], init_next_loop});
                            else
                                blk1 = Block::make({loop_bodies[i], init_next_loop});
                        }
                        // if (xyz < X) {
                        //    if (xyz == 0) {s0; s1;} A; if (xyz == X-1) { s2; s3; y=0; }
                        // } else
                        // { ... }
                        s = IfThenElse::make(cond, blk1, s);
                    }
                }
            } else { // Only one loop inside the loop body block.
                Stmt start_block, end_block;
                if (non_for_stmts_after_a_loop[0].size()!=0) {
                    Expr start_cond = EQ::make(Variable::make(Int(32), name), IntImm::make(Int(32),0));
                    start_block = Block::make(non_for_stmts_after_a_loop[0]);
                    start_block = IfThenElse::make(start_cond, start_block, Stmt());
                }
                if (non_for_stmts_after_a_loop[1].size()!=0) {
                    Expr end_cond = EQ::make(Variable::make(Int(32), name), Sub::make(loop_maxs.back(), IntImm::make(Int(32),1)));
                    end_block = Block::make(non_for_stmts_after_a_loop[1]);
                    end_block = IfThenElse::make(end_cond, end_block, Stmt());
                }
                if (start_block.defined()) {
                    s = Block::make(start_block, loop_bodies[0]);
                } else {
                    s = loop_bodies[0];
                }
                if (end_block.defined()) {
                    s = Block::make(s,end_block);
                }
            }
            Stmt stmt = For::make(name, IntImm::make(Int(32),0), loop_maxs.back(), op->for_type, op->device_api, s);
            stmt = For::make(op->name, op->min, op->extent, op->for_type, op->device_api, stmt);
            return stmt;
        }
        else {
            return IRMutator::visit(op);
        }
    }
public:
    std::vector<std::string> temps;
    MergeLoops() { temps.clear(); }
};

class LoopMerging : public IRMutator {
    using IRMutator::visit;
    bool is_open_cl;

    Stmt visit(const ProducerConsumer* op) override {
        is_open_cl = false;
        Stmt stmt = IRMutator::visit(op);
        is_open_cl = false;
        return stmt;
    }
    Stmt visit(const For* op) override {
        if (op->for_type != ForType::Serial || !is_open_cl) {
            if (op->device_api == DeviceAPI::OpenCL)
                is_open_cl = true;
            return IRMutator::visit(op);
        } else {
            MergeLoops ml;
            Stmt stmt = ml.mutate(op);
            for (auto temp : ml.temps) {
                stmt = Realize::make(temp + ".temp", {Int(32)}, MemoryType::Auto, {Range(0,1)}, const_true(), stmt);
            }
            return stmt;
        }
    }
public:
    LoopMerging() {
        is_open_cl = false;
    }
};

// A memory channel is a FIFO, but since it is implemented in memory, and thus its data
// are kept there, its data can be repeatedly read. For example, a memory channel
// may have the following data, a b c d, in order. Unlike a normal FIFO whose output
// sequence has to be a, b, c, d, the output sequence of the memory channel can be e.g.
// a, b, a, b, c, a, b, a, b, c, d. The sequence is still essentially sequential, but
// the reading point can loop back and the same data can be read again. Below in class
// FindAddressesOfMemChannels, we find out the address for a read of the memory channel.
// Then in class ReplaceMemChannel, we use the found address to replace the memory channel
// read with a Load; In addition, we also replace the memory channel write with a Store.
// For example:
//      Producer:
//            for i = 0; i < I; i++
//              for k = 0; k < K; k++
//                write_mem_channel(ch, ...)
//      Consumer:
//            for i = 0; i < I; i++
//              for j = 0; j < J; j++  // A reuse loop
//                for k = 0; k < K; k++
//                  read_mem_channel(ch, ...)
// The loop nests of the producer and consumer are the same, except the reuse loop.
// We will generate the following code:
//      Producer:
//            addr = 0
//            for i = 0; i < I; i++
//              for k = 0; k < K; k++
//                Store ch[addr], ...
//                addr++
//      Consumer:
//            addr = 0
//            for i = 0; i < I; i++
//              for j = 0; j < J; j++  // A reuse loop
//                for k = 0; k < K; k++
//                  Load ch[addr%K + (addr/(J*K))*K]
//                  addr++;
// Here we explain how the index for Load is calculated.
// The algorithm is to compare the loops of the producer and consumer of the memory channel,
// and remove the reuse loops from the consumer to get the reading address.
// The index (address expression) of the load is calculated by throwing away the part of reuse loop j
// from addr. In general, let index be the addression expression of the load. Index can
// be caculated this way: (1) let index = addr, (2) scan loops of the consumer from the
// innermost loop toward the outermost loop, (3) at a reuse loop level j, index is composed of
// three parts: outer loops part, reuse loop j part, inner loops part. In the inner loops part, any
// reuse loop part has already been removed. Now we want to remove the reuse loop j part, and make
// the index composed of two parts only: outer loops part, inner loops part. This can be done by
// letting K = product of the extents of all non-reuse inner loops of loop j
//         J = extent of loop j
// and     index = (index/(J*K))*K  // the outer loops part
//                 + index%K.       // plus the inner loops part.
// Note that (index/(J*K))*K is not equal to index/J in general, as / here is integer division.
// Also note that as an optimization, we can treat several contiguous reuse loops as 1 reuse loop, and
// remove their parts together, instead of one by one. That might simplify the index expression.

// We assume that in serialization, there is only 1 producer PE and only 1 consumer PE. This is
// similar to a normal channel. Therefore, if there are unrolled loops, we expect a mem channel
// access to be guarded with a condition like below:
//    Producer or Consumer:
//      for loops
//        unroll for loops u1, u2, ... un
//           if (some condition && u1==0 && u2==0 && ... && un==0) // in general, ui=a constant.
//                write/read_mem_channel(ch, ...)
// And the generated code should be like this:
//      addr = 0
//      for loops
//        unroll for loops u1, u2, ... un
//           if (some condition && u1==0 && u2==0 && ... && un==0)
//                store/ld ...
//           if (u1==0 && u2==0 && ... && un==0)
//              addr++
//
// In summary, in serialization, we have the following principles:
// 1. There is a single producer and consumer PE for a mem channel.
// 2. Address is incremented every iteration.

class FindAddressesOfMemChannels : public IRVisitor {
    using IRVisitor::visit;

public:
    FindAddressesOfMemChannels(map<string, Expr> &_mem_addr, const map<string, Function> &_env)
        : mem_addr(_mem_addr), env(_env) {};

private:
    vector<string> current_loops;
    vector<Expr> current_extents;
    map<string, vector<string>> producer_loops;
    map<string, Expr> &mem_addr;
    const map<string, Function> &env;

    void visit(const For *op) override {
        if (op->for_type == ForType::Serial) {
            std::string undecorated_loop_name = extract_after_tokens(op->name, 2); // remove func and stage from the name
            current_loops.push_back(undecorated_loop_name);
            current_extents.push_back(op->extent);
        }

        IRVisitor::visit(op);

        if (op->for_type == ForType::Serial) {
            current_loops.pop_back();
            current_extents.pop_back();
        }
    }

    void visit(const Call *op) override {
        if (op->is_intrinsic(Call::read_mem_channel)) {
            vector<Expr> args = op->args;
            internal_assert(args[0].as<StringImm>());
            string name = args[0].as<StringImm>()->value;
            internal_assert(producer_loops.find(name) != producer_loops.end());
            auto prod_loops = producer_loops[name];
            debug(2) << name << "\n";
            // internal_assert(current_loops.size() >= prod_loops.size());

            Expr temp;
            temp = Call::make(Int(32), "addr.temp", {}, Call::PureIntrinsic);
            const Function &func = env.at(name);
            Expr inner_loops_extents_prod = Expr(1); // product of the extents of the non-reuse inner loops of the current loop level
            Expr reuse_loops_extents_prod = Expr(1); // product of the extents of contiguous reuse loops immediately enclosed by the current loop level
            Expr index = temp;
            for (int j = current_loops.size() - 1; j >= 0; j--) {
                bool is_reuse_loop = true;
                for (auto loop_name : prod_loops) {
                    if (loop_name == current_loops[j]) {
                        is_reuse_loop = false;
                        break;
                    }
                }

                if (j == (int)(current_loops.size() - 1) && is_reuse_loop) {
                    const vector<Dim> &dims = func.definition().schedule().dims();
                    for (size_t i = 0; i < dims.size(); i++) {
                        if (dims[i].for_type == ForType::Vectorized && dims[i].var == current_loops[j]) {
                            is_reuse_loop = false;
                        }
                    }
                }
                
                if (is_reuse_loop) {
                    reuse_loops_extents_prod *= current_extents[j];
                    debug(2) << "Reuse loop: " << current_loops[j]
                             << ", extent: " << current_extents[j] << "\n";
                } else {
                    if (!equal(reuse_loops_extents_prod, 1)) {
                        // Remove the part of the index for the contiguous reuse loops under this non-reuse loop together
                        index = (index/(reuse_loops_extents_prod*inner_loops_extents_prod)) * inner_loops_extents_prod
                                + index % inner_loops_extents_prod;
                        reuse_loops_extents_prod = Expr(1);
                    }
                    inner_loops_extents_prod *= current_extents[j];
                    debug(2) << "Non-reuse loop: " << current_loops[j]
                             << ", extent: " << current_extents[j] << "\n";
                }
            }
	    if (!equal(reuse_loops_extents_prod, 1)) {
                // Remove the part of the index for the contiguous reuse loops starting from the outermost level 
                index = index % inner_loops_extents_prod;
            }
            index = simplify(index);
            debug(2) << index << "\n";
            mem_addr[name] = index;
        } else if (op->is_intrinsic(Call::write_mem_channel)) {
            vector<Expr> args = op->args;
            internal_assert(args[0].as<StringImm>());
            string name = args[0].as<StringImm>()->value;
            producer_loops[name] = current_loops;
        }
        IRVisitor::visit(op);
    }
};

class ReplaceMemChannel : public IRMutator {
    using IRMutator::visit;

public:
    ReplaceMemChannel(const map<string, Expr> &_mem_addr)
        : mem_addr(_mem_addr) { in_function = false; }

private:
    const map<string, Expr> &mem_addr;                       // Memory channal name -> index of Load
    bool in_function;                                        // The current IR is in a function definition.
    bool on_device;                                          // THe current IR is on device.
    vector<string> serial_loops;                             // Current serial loop names
    vector<string> unrolled_loops;                           // Current unrolled loops
    string current_loop;                                     // Current serial or unroll loop
    string loop_enclosing_mem_channel_access;                // The loop that immediately encloses a read/write_mem_channel
    int num_mem_channel_accesses;                            // We assume there can only be 1 read or write of mem channel for 1 func.
                                                             // Then with loop unrolling, there may appear multiple duplicates of the access.
    Expr path_condition;                                     // The path condition to a read/write_mem_channel
    Expr single_PE_condition;                                // The part of the path condition that checks for a single PE.
                                                             // E.g. given path condition "some cond && u0=0 && u1=0", where u0 and u1
                                                             // are unrolled loops, the single_PE_condition = "u0=0 && u1=0".

public:
    Stmt visit(const ProducerConsumer *op) override {
        if (op->is_producer) {
            // Initial all the variables
            in_function = true;
            on_device = false;
            internal_assert(serial_loops.empty());
            internal_assert(unrolled_loops.empty());
            current_loop.clear();
            loop_enclosing_mem_channel_access.clear();
            num_mem_channel_accesses = 0;
            path_condition = const_true();
            single_PE_condition = const_true();
        } else {
            in_function = false;
        }
        return IRMutator::visit(op);
    }

    // For path condition: we track only IfThenElse, not Select, because channel writes do not happen inside Select,
    // and we assume channel reads have the same condition with channel writes.
    Stmt visit(const IfThenElse *op) override {
        if (!in_function) {
            // We do not gather condition outside of a function.
            return IRMutator::visit(op);
        }
        Expr old_condition = path_condition;

        path_condition = equal(old_condition, const_true()) ? op->condition : old_condition && op->condition;
        Stmt then_case = mutate(op->then_case);

        Stmt else_case;
        if (op->else_case.defined()) {
            path_condition = equal(old_condition, const_true()) ? !op->condition : old_condition && !op->condition;
            else_case = mutate(op->else_case);
        }

        path_condition = old_condition;
        return IfThenElse::make(op->condition, then_case, else_case);
    }

    Stmt visit(const For *op) override {
        if (!in_function) {
            return IRMutator::visit(op);
        }

        bool prev_on_device = on_device;
        if (ends_with(op->name, ".run_on_device")) {
            on_device = true;
        }

        string prev_loop;
        int prev_num_mem_channel_accesses = 0;
        if ((op->for_type == ForType::Serial) || (op->for_type == ForType::Unrolled) ||
            (op->for_type == ForType::PragmaUnrolled) || (op->for_type == ForType::DelayUnroll)) {
            if (op->for_type == ForType::Serial) {
                serial_loops.push_back(op->name);
            } else {
                unrolled_loops.push_back(op->name);
            }
            prev_loop = current_loop;
            current_loop = op->name;
            prev_num_mem_channel_accesses = num_mem_channel_accesses;
            num_mem_channel_accesses = 0;
        }
        
        Stmt body = mutate(op->body);

        if ((op->for_type == ForType::Serial) || (op->for_type == ForType::Unrolled) ||
            (op->for_type == ForType::PragmaUnrolled) || (op->for_type == ForType::DelayUnroll)) {
            // If the current loop immediately encloses a read/write_mem_channel, increment the address temp
            // at the end of the loop
            if (loop_enclosing_mem_channel_access == current_loop) {
                Stmt inc = Provide::make("addr.temp", {Call::make(Int(32), "addr.temp", {}, Call::Intrinsic) + num_mem_channel_accesses}, {});
                if (!equal(single_PE_condition, const_true())) {
                    inc = IfThenElse::make(single_PE_condition, inc);
                }
                body = Block::make(body, inc);
            }
        }
        Stmt s = For::make(op->name, op->min, op->extent, op->for_type, op->device_api, body);

        if ((op->for_type == ForType::Serial) || (op->for_type == ForType::Unrolled) ||
            (op->for_type == ForType::PragmaUnrolled) || (op->for_type == ForType::DelayUnroll)) {
            // Allocate and initialize addr before any the serial and unrolled loops
            if (prev_loop.empty()) {
                string func = split_string(current_loop, ".")[0];
                if (!loop_enclosing_mem_channel_access.empty()) {
                    s = Block::make(Provide::make("addr.temp", {0}, {}), s);
                    if (on_device) {
                        s = Realize::make("addr.temp", {Int(32)}, MemoryType::Auto, {}, const_true(), s);
                    } else {
                        s = Allocate::make("addr.temp", Int(32), MemoryType::Auto, {}, const_true(), s);
                    }
                }
            }

            if (op->for_type == ForType::Serial) {
                serial_loops.pop_back();
            } else {
                unrolled_loops.pop_back();
            }
            current_loop = prev_loop;
            num_mem_channel_accesses = prev_num_mem_channel_accesses;
        }

        if (ends_with(op->name, ".run_on_device")) {
            on_device = prev_on_device;
        }

        return s;
    }

    Stmt visit(const Evaluate *op) override {
        if (!in_function) {
            return IRMutator::visit(op);
        }
        if (auto call = op->value.as<Call>()) {
            if (call->is_intrinsic(Call::write_mem_channel)) {
                loop_enclosing_mem_channel_access = current_loop;
                vector<string> unrolled_loops_without_terms;
                check_is_single_PE(on_device, path_condition, unrolled_loops, {}, single_PE_condition, unrolled_loops_without_terms);
                vector<Expr> args = call->args;
                internal_assert(args[0].as<StringImm>());
                string name = args[0].as<StringImm>()->value;
                Expr value = mutate(args[1]);
                int lanes = value.type().lanes();
                Stmt s;
                Expr i = mutate(Call::make(Int(32), "addr.temp", {}, Call::Intrinsic));
                s = Store::make(name, value, lanes <= 1 ? i : Ramp::make(i*lanes, 1, lanes), call->param, const_true(value.type().lanes()), ModulusRemainder()*lanes);
                num_mem_channel_accesses++;
                return s;
            }
        }
        return IRMutator::visit(op);
    }

    Expr visit(const Call *op) override {
        if (!in_function) {
            return IRMutator::visit(op);
        }
        if (op->is_intrinsic(Call::read_mem_channel)) {
            loop_enclosing_mem_channel_access = current_loop;
            vector<string> unrolled_loops_without_terms;
            check_is_single_PE(on_device, path_condition, unrolled_loops, {}, single_PE_condition, unrolled_loops_without_terms);
            internal_assert(op->args[0].as<StringImm>());
            string name = op->args[0].as<StringImm>()->value;
            Expr addr = mem_addr.at(name);
            Expr i = mutate(addr);
            int lanes = op->type.lanes();
            Expr e = Load::make(op->type, name, lanes <= 1 ? i : Ramp::make(i*lanes, 1, lanes), op->image, op->param, const_true(op->type.lanes()), ModulusRemainder()*lanes);
            num_mem_channel_accesses++;
            return e;
        }  else if (op->name == "addr.temp") {
            // Both read/write_mem_channel will get here as they will call addr.temp to get the current address.
            // Add num_mem_channel_accesses as the offset to the address.
            return simplify(Call::make(Int(32), op->name, {}, Call::Intrinsic) + num_mem_channel_accesses);
        }
        return IRMutator::visit(op);
    }
};

Stmt replace_mem_channels(Stmt s, const std::map<std::string, Function> &env, const std::map<string, Place> &funcs_using_mem_channels) {
    map<string, Expr> mem_addr;
    FindAddressesOfMemChannels finder(mem_addr, env);
    s.accept(&finder);
    ReplaceMemChannel replacer(mem_addr);
    s = replacer.mutate(s);

    std::set<string> funcs;
    for(auto entry : env){
        if (entry.second.place() == Place::Device) {
            funcs.insert(entry.first);
        }
    }
    s = remove_lets(s, false, true, true, true, funcs);
    debug(2) << "IR after removing LetStmts in device kernels ...\n\n" << s << "\n";
    return s;
}

Stmt flatten_loops(Stmt s, const std::map<std::string, Function> &env) {
    ConstLoopFlattening clf;
    s = clf.mutate(s);
    debug(2) << "IR after const loop flattening ...\n\n" << s << "\n";

    std::set<string> funcs;
    for(auto entry : env){
        if (entry.second.place() == Place::Device) {
            funcs.insert(entry.first);
        }
    }
    s = remove_lets(s, false, true, true, true, funcs);
    debug(2) << "IR after removing LetStmts in device kernels ...\n\n" << s << "\n";

    // DynamicLoopFlattening dlf;
    // Stmt stmt3 = dlf.mutate(stmt2);
    // debug(2) << "IR after dynamic loop flattening ...\n\n" << stmt3 << "\n";
    // LoopMerging mgl;
    // Stmt stmt4 = mgl.mutate(stmt3);
    return s;
}

}
}
