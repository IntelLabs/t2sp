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
#include "CodeGen_Internal.h"
#include "CSE.h"
#include "EliminateBoolVectors.h"
#include "IREquality.h"
#include "IRMatch.h"
#include "IRMutator.h"
#include "IROperator.h"
#include "Scope.h"
#include "Simplify.h"
#include "../../t2s/src/DebugPrint.h"
#include "../../t2s/src/StandardizeIRForOpenCL.h"
#include "../../t2s/src/Utilities.h"

#include <map>
#include <queue>

namespace Halide {
namespace Internal {

using std::string;
using std::map;
using std::pair;
using std::string;
using std::vector;
using std::set;
using std::queue;

// Misc lowering of some expressions in a single statement
class MiscLoweringStmtForOpenCL : public IRMutator {
  private:
    // Return the variable representing the whole expression, if there is such a variable.
    const Variable *as_variable(const Expr &e) {
        if (const Variable *op = e.as<Variable>()) {
            return op;
        }
        // For a Let expression:
        //     Let x = value in body
        // body, not x, represents the entire expression. So check if the body is actually a variable.
        if (const Let * op = e.as<Let>()) {
            return as_variable(op->body);
        }
        return NULL;
    }

    bool is_variable(const Expr &e) {
        return as_variable(e) != NULL;
    }

    Expr save_to_intermediate_vector(const Expr &operand) {
        if (!is_variable(operand)) {
            Type t = operand.type();
            if (t.is_generated_struct() || (t.is_bool() && t.is_vector())) {
                // Operand is not a variable, and its type is a generated struct. We have to apply the operation to every field
                // of the struct, and thus we need hoist this operand so that code generation looks like:
                //     t = operand; // compute the operand in a Let Expr.
                //     ... = t.s0 + t.s1 ... // the fields now can be accessed
                // Note a bool vector has no OpenCL native implementation, and has to be implemented in a user-defined struct as well.
                string name = unique_name('t');
                Expr var = Variable::make(t, name);
                Expr e = Let::make(name, mutate(operand), var);
                return e;
            }
        }
        return operand;
    }

    Expr mutate_bool_vector_type(Expr &e, const Type &operand_type) {
        internal_assert(e.type().is_bool() && e.type().is_vector());
        Type bool_vec_type = e.type();
        Type intermediate_type = e.type().with_code(Type::Int).with_bits(operand_type.bits());
        string intermediate_name = unique_name('t');
        Expr intermediate_var = Variable::make(intermediate_type, intermediate_name);
        Expr cast = Cast::make(bool_vec_type, intermediate_var);
        e.set_type(intermediate_type);
        Expr new_e = Let::make(intermediate_name, e, cast);
        new_e.set_type(bool_vec_type);
        return new_e;
    }

  public:
    MiscLoweringStmtForOpenCL() {}

    Stmt mutate(const Stmt &s) override {
        if (const LetStmt *op = s.as<LetStmt>()) {
            return LetStmt::make(op->name, mutate(op->value), op->body);
        } else if (const AssertStmt *op = s.as<AssertStmt>()) {
            return AssertStmt::make(mutate(op->condition), mutate(op->message));
        } else if (const ProducerConsumer *op = s.as<ProducerConsumer>()) {
            return op;
        } else if (const Store *op = s.as<Store>()) {
            user_assert(is_one(op->predicate)) << "Predicated store is not supported inside OpenCL kernel.\n";

            Expr ramp_base = strided_ramp_base(op->index);
            if (ramp_base.defined()) {
                // Writing a contiguous ramp.
                Stmt store = Store::make(op->name, mutate(op->value), mutate(op->index), op->param, op->predicate, op->alignment);
                return store;
            } else if (op->index.type().is_vector()) {
                // If index is a vector, scatter vector elements. But if the index or value is an expression, it is illegal to
                // use index.s0, value.s0, etc.
                // To ensure correctness always, we will generate code like this instead:
                //    addr = op->index; value = op->value;
                //    A[addr.s0] = value.s0; A[addr.s1] = value.s1; ...
                Expr index1 = mutate(op->index);
                Expr value1 = mutate(op->value);

                string new_index_name, new_value_name;
                Expr new_index = index1;
                Expr new_value = value1;
                if (!is_variable(index1)) {
                    new_index_name = unique_name('t');
                    new_index = Variable::make(index1.type(), new_index_name);
                }
                if (!is_variable(value1)) {
                    new_value_name = unique_name('t');
                    new_value = Variable::make(value1.type(), new_value_name);
                }
                Stmt store = Store::make(op->name, new_value, new_index, op->param, op->predicate, op->alignment);
                if (!is_variable(value1)) {
                    store = LetStmt::make(new_value_name, value1, store);
                }
                if (!is_variable(index1)) {
                    store = LetStmt::make(new_index_name, index1, store);
                }
                return store;
            } else {
                Stmt store = Store::make(op->name, mutate(op->value), mutate(op->index), op->param, op->predicate, op->alignment);
                return store;
            }
        } else if (const For *op = s.as<For>()) {
            return For::make(op->name, mutate(op->min), mutate(op->extent), op->for_type, op->device_api, op->body);
        } else if (const Provide *op = s.as<Provide>()) {
            vector<Expr> values, args;
            for (auto &v : op->values) {
                values.push_back(mutate(v));
            }
            for (auto &a : op->args) {
                args.push_back(mutate(a));
            }
            return Provide::make(op->name, values, args);
        } else if (const Allocate *op = s.as<Allocate>()) {
            vector<Expr> extents;
            for (auto &e : op->extents) {
                extents.push_back(mutate(e));
            }
            return Allocate::make(op->name, op->type, op->memory_type, extents, mutate(op->condition), op->body, mutate(op->new_expr), op->free_function);
        } else if (const Free *op = s.as<Free>()) {
            return op;
        } else if (const Realize *op = s.as<Realize>()) {
            Region bounds;
            for (auto &b : op->bounds) {
                bounds.push_back({mutate(b.min), mutate(b.extent)});
            }
            return Realize::make(op->name, op->types, op->memory_type, bounds, mutate(op->condition), op->body);
        } else if (const Block *op = s.as<Block>()) {
            return op;
        } else if (const IfThenElse *op = s.as<IfThenElse>()) {
            return IfThenElse::make(mutate(op->condition), op->then_case, op->else_case);
        } else if (const Evaluate *op = s.as<Evaluate>()) {
            return Evaluate::make(mutate(op->value));
        } else if (const Prefetch *op = s.as<Prefetch>()) {
            Region bounds;
            for (auto &b : op->bounds) {
                bounds.push_back({mutate(b.min), mutate(b.extent)});
            }
            return Prefetch::make(op->name, op->types, bounds,op->prefetch, mutate(op->condition), op->body);
        } else if (const Acquire *op = s.as<Acquire>()) {
            return Acquire::make(mutate(op->semaphore), mutate(op->count), op->body);
        } else if (const Fork *op = s.as<Fork>()) {
            return op;
        } else {
            internal_assert(s.as<Atomic>());
            return s;
        }
    }

    Expr mutate(const Expr &e) override {
        static int count=0;
        count++;
        int original_count= count;
        debug(4) << "MUTATE " << original_count << ": " << e << "\n";
        Expr orig_e, des_e, lets_e, cse_e, ret_e;
        orig_e = e;

        // First, modify the expression to desirable computations.
        Expr new_e = e;
        if (const Add* op = e.as<Add>()) {
            if (const IntImm *int_imm = op->b.as<IntImm>()) {
                if (int_imm->value < 0) {
                    new_e = Sub::make(op->a, IntImm::make(int_imm->type, -int_imm->value));
                }
            }
        } else if (const Call* op = e.as<Call>()) {
            if (op->is_intrinsic(Call::shift_left) || op->is_intrinsic(Call::shift_right)) {
                // Some OpenCL implementations forbid mixing signed-and-unsigned shift values;
                // if the RHS is uint, quietly cast it back to int if the LHS is int
                if (op->args[0].type().is_int() && op->args[1].type().is_uint()) {
                    Type t = op->args[0].type().with_code(halide_type_int);
                    new_e = Call::make(op->type, op->name, {op->args[0], cast(t, op->args[1])}, op->call_type);
                }
            }
        } else if (const Div* op = e.as<Div>()) {
            int bits;
            char* ediv = getenv("EUCLIDEAN_DIVISION");
            if (is_const_power_of_two_integer(op->b, &bits)) {
                new_e = Call::make(op->type, Internal::Call::shift_right, {op->a, make_const(op->a.type(), bits)}, Internal::Call::PureIntrinsic);
            } else if (ediv && op->type.is_int()) {
                new_e = lower_euclidean_div(op->a, op->b);
            }
        } else if (const Mod* op = e.as<Mod>()) {
            int bits;
            char* ediv = getenv("EUCLIDEAN_DIVISION");
            if (is_const_power_of_two_integer(op->b, &bits)) {
                new_e = Call::make(op->type, Internal::Call::bitwise_and, {op->a, make_const(op->a.type(), (1 << bits) - 1)}, Internal::Call::PureIntrinsic);
            } else if (ediv && op->type.is_int()) {
                new_e = lower_euclidean_mod(op->a, op->b);
            }
        } else if (const Load* op = e.as<Load>()) {
            user_assert(is_one(op->predicate)) << "Predicated load is not supported inside OpenCL kernel.\n";

            Expr ramp_base = strided_ramp_base(op->index);
            if (!ramp_base.defined() and op->index.type().is_vector()) {
                // If index is a vector, gather vector elements. But if the index is an expression, it is illegal to use index.s0, etc.
                // To ensure correctness always, we will generate code like this instead:
                //  int8 value; addr = index; value.s0 = A[addr.s0]; value.s1 = A[addr.s1]; ...
                internal_assert(op->type.is_vector());
                string index_name = unique_name('t');
                Expr index_var = Variable::make(op->index.type(), index_name);
                Expr load = Load::make(op->type, op->name, index_var, op->image, op->param, op->predicate, op->alignment);
                new_e = Let::make(index_name, op->index, load);
            }
        }
        debug(4) << "MUTATE " << original_count << ": after lowering to desirable compute: " << new_e << "\n";
        des_e = new_e;

        // Second, create Let Expr for ALU operations' operands that are vectors but we need access their elements in this expression.
        if (const Add *op = new_e.as<Add>()) {
            new_e = Add::make(save_to_intermediate_vector(op->a), save_to_intermediate_vector(op->b));
        } else if (const Sub *op = new_e.as<Sub>()) {
            new_e = Sub::make(save_to_intermediate_vector(op->a), save_to_intermediate_vector(op->b));
        } else if (const Mul *op = new_e.as<Mul>()) {
            new_e = Mul::make(save_to_intermediate_vector(op->a), save_to_intermediate_vector(op->b));
        } else if (const Div *op = new_e.as<Div>()) {
            new_e = Div::make(save_to_intermediate_vector(op->a), save_to_intermediate_vector(op->b));
        } else if (const Mod *op = new_e.as<Mod>()) {
            new_e = Mod::make(save_to_intermediate_vector(op->a), save_to_intermediate_vector(op->b));
        } else if (const Min *op = new_e.as<Min>()) {
            new_e = Min::make(save_to_intermediate_vector(op->a), save_to_intermediate_vector(op->b));
        } else if (const Max *op = new_e.as<Max>()) {
            new_e = Max::make(save_to_intermediate_vector(op->a), save_to_intermediate_vector(op->b));
        } else if (const EQ *op = new_e.as<EQ>()) {
            new_e = EQ::make(save_to_intermediate_vector(op->a), save_to_intermediate_vector(op->b));
        } else if (const NE *op = new_e.as<NE>()) {
            new_e = NE::make(save_to_intermediate_vector(op->a), save_to_intermediate_vector(op->b));
        } else if (const LT *op = new_e.as<LT>()) {
            new_e = LT::make(save_to_intermediate_vector(op->a), save_to_intermediate_vector(op->b));
        } else if (const LE *op = new_e.as<LE>()) {
            new_e = LE::make(save_to_intermediate_vector(op->a), save_to_intermediate_vector(op->b));
        } else if (const GT *op = new_e.as<GT>()) {
            new_e = GT::make(save_to_intermediate_vector(op->a), save_to_intermediate_vector(op->b));
        } else if (const GE *op = new_e.as<GE>()) {
            new_e = GE::make(save_to_intermediate_vector(op->a), save_to_intermediate_vector(op->b));
        } else if (const And *op = new_e.as<And>()) {
            new_e = And::make(save_to_intermediate_vector(op->a), save_to_intermediate_vector(op->b));
        } else if (const Or *op = new_e.as<Or>()) {
            new_e = Or::make(save_to_intermediate_vector(op->a), save_to_intermediate_vector(op->b));
        } else if (const Not *op = new_e.as<Not>()) {
            new_e = Not::make(save_to_intermediate_vector(op->a));
        }
        debug(4) << "MUTATE " << original_count << ": after creating Lets: " << new_e << "\n";
        lets_e = new_e;

        // Third, we did not get into sub-expressions above, and now do it.
        if (new_e.as<IntImm>() || new_e.as<UIntImm>() || new_e.as<FloatImm>() || new_e.as<StringImm>() || new_e.as<Variable>()) {
            // Do nothhing.
        } else if (const Cast *op = new_e.as<Cast>()) {
            new_e = Cast::make(op->type, mutate(op->value));
        } else if (const Add *op = new_e.as<Add>()) {
            new_e = Add::make(mutate(op->a), mutate(op->b));
        } else if (const Sub *op = new_e.as<Sub>()) {
            new_e = Sub::make(mutate(op->a), mutate(op->b));
        } else if (const Mul *op = new_e.as<Mul>()) {
            new_e = Mul::make(mutate(op->a), mutate(op->b));
        } else if (const Div *op = new_e.as<Div>()) {
            new_e = Div::make(mutate(op->a), mutate(op->b));
        } else if (const Mod *op = new_e.as<Mod>()) {
            new_e = Mod::make(mutate(op->a), mutate(op->b));
        } else if (const Min *op = new_e.as<Min>()) {
            new_e = Min::make(mutate(op->a), mutate(op->b));
        } else if (const Max *op = new_e.as<Max>()) {
            new_e = Max::make(mutate(op->a), mutate(op->b));
        } else if (const EQ *op = new_e.as<EQ>()) {
            new_e = EQ::make(mutate(op->a), mutate(op->b));
        } else if (const NE *op = new_e.as<NE>()) {
            new_e = NE::make(mutate(op->a), mutate(op->b));
        } else if (const LT *op = new_e.as<LT>()) {
            new_e = LT::make(mutate(op->a), mutate(op->b));
        } else if (const LE *op = new_e.as<LE>()) {
            new_e = LE::make(mutate(op->a), mutate(op->b));
        } else if (const GT *op = new_e.as<GT>()) {
            new_e = GT::make(mutate(op->a), mutate(op->b));
        } else if (const GE *op = new_e.as<GE>()) {
            new_e = GE::make(mutate(op->a), mutate(op->b));
        } else if (const And *op = new_e.as<And>()) {
            new_e = And::make(mutate(op->a), mutate(op->b));
        } else if (const Or *op = new_e.as<Or>()) {
            new_e = Or::make(mutate(op->a), mutate(op->b));
        } else if (const Not *op = new_e.as<Not>()) {
            new_e = Not::make(mutate(op->a));
        } else if (const Select *op = new_e.as<Select>()) {
            new_e = Select::make(mutate(op->condition), mutate(op->true_value), mutate(op->false_value));
        } else if (const Load *op = new_e.as<Load>()) {
            new_e = Load::make(op->type, op->name, mutate(op->index), op->image, op->param, op->predicate, op->alignment);
        } else if (const Ramp *op = new_e.as<Ramp>()) {
            new_e = Ramp::make(mutate(op->base), mutate(op->stride), op->lanes);
        } else if (const Broadcast *op = new_e.as<Broadcast>()) {
            new_e = Broadcast::make(mutate(op->value), op->lanes);
        } else if (const Call *op = new_e.as<Call>()) {
            vector<Expr> args;
            for (const auto &a: op->args) {
                args.push_back(mutate(a));
            }
            new_e = Call::make(op->type, op->name, args, op->call_type, op->func, op->value_index, op->image, op->param);
        } else if (const Let *op = new_e.as<Let>()) {
            new_e = Let::make(op->name, mutate(op->value), mutate(op->body));
        } else {
            const Shuffle *shuffle = new_e.as<Shuffle>();
            internal_assert(shuffle);
            vector<Expr> vectors;
            for (const auto &v : shuffle->vectors) {
                vectors.push_back(mutate(v));
            }
            new_e = Shuffle::make(vectors, shuffle->indices);
        }
        debug(4) << "MUTATE " << original_count << ": after handling sub-expressions: " << new_e << "\n";
        cse_e = new_e;

        // Finally, see if we need change the return type of the expression.
        // If the expression returns a bool vector, but its operands are integers, for example, we
        // need generate code like:
        //      int16  x = (y == z);                 // y and z are int16
        //      bool16 b = {x.s0, x.s1, ..., x.s15}; // convert from int16 to bool16
        // Note that a bool vector like bool16 above has to be generated as a struct type, as OpenCL does not
        // support it natively.
        if (new_e.type().is_bool() && new_e.type().is_vector()) {
            if (const EQ *op = new_e.as<EQ>()) {
                new_e = mutate_bool_vector_type(new_e, op->a.type());
            } else if (const NE *op = new_e.as<NE>()) {
                new_e = mutate_bool_vector_type(new_e, op->a.type());
            } else if (const LT *op = new_e.as<LT>()) {
                new_e = mutate_bool_vector_type(new_e, op->a.type());
            } else if (const LE *op = new_e.as<LE>()) {
                new_e = mutate_bool_vector_type(new_e, op->a.type());
            } else if (const GT *op = new_e.as<GT>()) {
                new_e = mutate_bool_vector_type(new_e, op->a.type());
            } else if (const GE *op = new_e.as<GE>()) {
                new_e = mutate_bool_vector_type(new_e, op->a.type());
            }
        }
        debug(4) << "MUTATE " << original_count << ": after changing return type: " << new_e << "\n";
        ret_e= new_e;


        static int count1=0;
        count1++;
        debug(4) << "MUTATEPRocess: " << count1 << ": Orignal e: " << to_string(orig_e) << "\n";
        debug(4) << "MUTATEPRocess: " << count1 << ": des_e: " << to_string(des_e) << "\n";
        debug(4) << "MUTATEPRocess: " << count1 << ": lets_e: " << to_string(lets_e) << "\n";
        debug(4) << "MUTATEPRocess: " << count1 << ": cse_e: " << to_string(cse_e) << "\n";
        debug(4) << "MUTATEPRocess: " << count1 << ": ret_e: " << to_string(ret_e) << "\n";


        return new_e;
    }
};

class Let2LetStmt : public IRMutator {
private:
    vector<pair<string, Expr>> name_value_pairs; // For each "Let x=value" found, record x and value here.
public:
    bool changed; // Any Let has been hosited as LetStmt
    using IRMutator::mutate;
    Let2LetStmt() { changed = false; }

    // Create LetStms, according to name_value_pairs, before the current statement s
    Stmt make_LetStmts(const Stmt &s) {
        Stmt new_s = s;
        for (int i = name_value_pairs.size() - 1; i >= 0; i--) {
            new_s = LetStmt::make(name_value_pairs[i].first, name_value_pairs[i].second, new_s);
        }
        changed = name_value_pairs.size() > 0;
        name_value_pairs.clear();
        return new_s;
    }

    Stmt mutate(const Stmt &s) override {
        internal_assert(name_value_pairs.size() == 0); // The vector is empty before a statement is processed, and is cleared after that statement is processed..
        if (const LetStmt *op = s.as<LetStmt>()) {
            Stmt new_body = mutate(op->body);
            internal_assert(name_value_pairs.size() == 0);
            return make_LetStmts(LetStmt::make(op->name, mutate(op->value), new_body));
        } else if (const AssertStmt *op = s.as<AssertStmt>()) {
            return make_LetStmts(AssertStmt::make(mutate(op->condition), mutate(op->message)));
        } else if (const ProducerConsumer *op = s.as<ProducerConsumer>()) {
            return ProducerConsumer::make(op->name, op->is_producer, mutate(op->body));
        } else if (const Store *op = s.as<Store>()) {
            user_assert(is_one(op->predicate)) << "Predicated store is not supported inside OpenCL kernel.\n";
            return make_LetStmts(Store::make(op->name, mutate(op->value), mutate(op->index), op->param, op->predicate, op->alignment));
        } else if (const For *op = s.as<For>()) {
            Stmt new_body = mutate(op->body);
            internal_assert(name_value_pairs.size() == 0);
            return make_LetStmts(For::make(op->name, mutate(op->min), mutate(op->extent), op->for_type, op->device_api, new_body));
        } else if (const Provide *op = s.as<Provide>()) {
            vector<Expr> values, args;
            for (auto &v : op->values) {
                values.push_back(mutate(v));
            }
            for (auto &a : op->args) {
                args.push_back(mutate(a));
            }
            return make_LetStmts(Provide::make(op->name, values, args));
        } else if (const Allocate *op = s.as<Allocate>()) {
            Stmt new_body = mutate(op->body);
            internal_assert(name_value_pairs.size() == 0);
            vector<Expr> extents;
            for (auto &e : op->extents) {
                extents.push_back(mutate(e));
            }
            return  make_LetStmts(Allocate::make(op->name, op->type, op->memory_type, extents, mutate(op->condition), new_body, mutate(op->new_expr), op->free_function));
        } else if (const Free *op = s.as<Free>()) {
            return op;
        } else if (const Realize *op = s.as<Realize>()) {
            Stmt new_body = mutate(op->body);
            internal_assert(name_value_pairs.size() == 0);
            Region bounds;
            for (auto &b : op->bounds) {
                bounds.push_back({mutate(b.min), mutate(b.extent)});
            }
            return make_LetStmts(Realize::make(op->name, op->types, op->memory_type, bounds, mutate(op->condition), new_body));
        } else if (const Block *op = s.as<Block>()) {
            Stmt new_first = mutate(op->first);
            internal_assert(name_value_pairs.size() == 0);
            Stmt new_rest = mutate(op->rest);
            internal_assert(name_value_pairs.size() == 0);
            return Block::make(new_first, new_rest);
        } else if (const IfThenElse *op = s.as<IfThenElse>()) {
            Stmt new_then_case = mutate(op->then_case);
            internal_assert(name_value_pairs.size() == 0);
            Stmt new_else_case;
            if (op->else_case.defined()) {
                new_else_case = mutate(op->else_case);
            }
            internal_assert(name_value_pairs.size() == 0);
            return make_LetStmts(IfThenElse::make(mutate(op->condition), new_then_case, new_else_case));
        } else if (const Evaluate *op = s.as<Evaluate>()) {
            return make_LetStmts(Evaluate::make(mutate(op->value)));
        } else if (const Prefetch *op = s.as<Prefetch>()) {
            Stmt new_body = mutate(op->body);
            internal_assert(name_value_pairs.size() == 0);
            Region bounds;
            for (auto &b : op->bounds) {
                bounds.push_back({mutate(b.min), mutate(b.extent)});
            }
            return make_LetStmts(Prefetch::make(op->name, op->types, bounds,op->prefetch, mutate(op->condition), new_body));
        } else if (const Acquire *op = s.as<Acquire>()) {
            Stmt new_body = mutate(op->body);
            internal_assert(name_value_pairs.size() == 0);
            return make_LetStmts(Acquire::make(mutate(op->semaphore), mutate(op->count), new_body));
        } else if (const Fork *op = s.as<Fork>()) {
            Stmt new_first = mutate(op->first);
            internal_assert(name_value_pairs.size() == 0);
            Stmt new_rest = mutate(op->rest);
            internal_assert(name_value_pairs.size() == 0);
            return Fork::make(new_first, new_rest);
        } else {
            const Atomic *atomic = s.as<Atomic>();
            internal_assert(atomic);
            return Atomic::make(atomic->producer_name, atomic->mutex_name, mutate(atomic->body));
        }
    }

    Expr mutate(const Expr &e) override {
        if (const Let *op = e.as<Let>()) {
            Expr new_value = mutate(op->value);
            name_value_pairs.push_back(pair<string, Expr>(op->name, new_value));
            return mutate(op->body); // it is the body that represents the entire expression
        } else {
            return IRMutator::mutate(e);
        }
    }
};

/*
class GatherGVNOfEveryExprInStmt : public IRMutator {
private:
    GVN &gvn;
public:
    using IRMutator::mutate;
    GatherGVNOfEveryExprInStmt(GVN &gvn): gvn(gvn) {};

    Stmt mutate(const Stmt &s) override {
        if (const LetStmt *op = s.as<LetStmt>()) {
            return LetStmt::make(op->name, mutate(op->value), op->body);
        } else if (const AssertStmt *op = s.as<AssertStmt>()) {
            return AssertStmt::make(mutate(op->condition), mutate(op->message));
        } else if (const ProducerConsumer *op = s.as<ProducerConsumer>()) {
            return op;
        } else if (const Store *op = s.as<Store>()) {
            return Store::make(op->name, mutate(op->value), mutate(op->index),
                               op->param, mutate(op->predicate), op->alignment);
        } else if (const For *op = s.as<For>()) {
            return For::make(op->name, mutate(op->min), mutate(op->extent), op->for_type, op->device_api, op->body);
        } else if (const Provide *op = s.as<Provide>()) {
            vector<Expr> values, args;
            for (auto &v : op->values) {
                values.push_back(mutate(v));
            }
            for (auto &a : op->args) {
                args.push_back(mutate(a));
            }
            return Provide::make(op->name, values, args);
        } else if (const Allocate *op = s.as<Allocate>()) {
            vector<Expr> extents;
            for (auto &e : op->extents) {
                extents.push_back(mutate(e));
            }
            return Allocate::make(op->name, op->type, op->memory_type, extents, mutate(op->condition), op->body, mutate(op->new_expr), op->free_function);
        } else if (const Free *op = s.as<Free>()) {
            return op;
        } else if (const Realize *op = s.as<Realize>()) {
            Region bounds;
            for (auto &b : op->bounds) {
                bounds.push_back(Range(mutate(b.min), mutate(b.extent)));
            }
            return Realize::make(op->name, op->types, op->memory_type, bounds, mutate(op->condition), op->body);
        } else if (const Block *op = s.as<Block>()) {
            return op;
        } else if (const IfThenElse *op = s.as<IfThenElse>()) {
            return IfThenElse::make(mutate(op->condition), op->then_case, op->else_case);
        } else if (const Evaluate *op = s.as<Evaluate>()) {
            return Evaluate::make(mutate(op->value));
        } else if (const Prefetch *op = s.as<Prefetch>()) {
            Region bounds;
            for (auto &b : op->bounds) {
                bounds.push_back(Range(mutate(b.min), mutate(b.extent)));
            }
            return Prefetch::make(op->name, op->types, bounds, op->prefetch, mutate(op->condition), op->body);
        } else if (const Acquire *op = s.as<Acquire>()) {
            return Acquire::make(mutate(op->semaphore), mutate(op->count), op->body);
        } else if (const Fork *op = s.as<Fork>()) {
            return op;
        } else {
            internal_assert(s.as<Atomic>());
            return s;
        }
    }

    Expr mutate(const Expr &e) override {
        static int count=0;
        count++;
        debug(4) << "TOGVN: " << count << ": " << to_string(e) << "\n";
        return gvn.mutate(e);
    }
};
*/

// Standardize each statement in the IR for OpenCL code generation
class StmtStandardizerOpenCL : public IRMutator {
  public:
    StmtStandardizerOpenCL() : in_opencl_device_kernel(false) {}

  private:
    using IRMutator::visit;

    bool in_opencl_device_kernel;
  public:
    Stmt mutate(const Stmt &s) override {
        if (!s.defined()) {
            return s;
        }
        const For *loop = s.as<For>();
        if (loop && loop->device_api == DeviceAPI::OpenCL && ends_with(loop->name, ".run_on_device")) {
            in_opencl_device_kernel = true;
        }
        Stmt new_s = s;
        if (in_opencl_device_kernel) {
            static int count=0;
            count++;
            // Misc lowering of some expressions in this statement according to OpenCL's capability
            debug(4) << "MiscLoweringStmtForOpenCL: " << count << ": Statement before:\n" << to_string(new_s) << "\n";
            new_s = MiscLoweringStmtForOpenCL().mutate(new_s);
            debug(4) << "MiscLoweringStmtForOpenCL: " << count << ": Statement after:\n" << to_string(new_s) << "\n\n";

            // We have created Let expressions in the above step. Make them as LetStmt. For example:
            //      ... = select(cond, Let x = value in body, ...)
            // standardize the IR like this:
            //      LetStmt x = value in
            //          ... = select(cond, body, ...)
            // Then in code generation (NOT in IR), we should find the condition of the LetStmt, and print code like this, e.g.:
            //      int x; if (cond) { x = value; }
            //      if (cond) ... = body else ...
            debug(4) << "Let2LetStmt: " << count << ": Statement before:\n" << to_string(new_s) << "\n\n";
            new_s = Let2LetStmt().mutate(new_s);
            debug(4) << "Let2LetStmt: " << count << ": Statement after:\n" << to_string(new_s) << "\n\n";

            /*
            // Use statement-level global value numbering to label distinct expressions within the statement
            // We will hoist out expressions with multiple uses before the statement to avoid redundant computation, also make the generated code clearer.
            GVN gvn;
            new_s = GatherGVNOfEveryExprInStmt(gvn).mutate(new_s);

            // Figure out which ones we'll pull out as lets and variables.
            vector<pair<string, Expr>> lets;
            map<Expr, Expr, ExprCompare> replacements;
            for (size_t i = 0; i < gvn.entries.size(); i++) {
                const auto &e = gvn.entries[i];
                if (e->use_count > 1) {
                    string name = unique_name('t');
                    lets.emplace_back(name, e->expr);
                    // Point references to this expr to the variable instead.
                    replacements[e->expr] = Variable::make(e->expr.type(), name);
                    debug(4) << "StmtStandardizerOpenCL: " << i << " to replace: " << to_string(e->expr) << " with " << to_string(replacements[e->expr]) << "\n";
                }
            }

            // Rebuild the expr to include references to the variables:
            Replacer replacer(replacements);
            new_s = replacer.mutate(new_s);

            // Wrap the final Stmt in LetStmts.
            for (size_t i = lets.size(); i > 0; i--) {
                Expr value = lets[i - 1].second;
                // Drop this variable as an acceptable replacement for this expr.
                replacer.erase(value);
                // Use containing lets in the value.
                value = replacer.mutate(lets[i - 1].second);
                new_s = LetStmt::make(lets[i - 1].first, value, new_s);
            }*/
        }

        // Continue to look at the other statements
        new_s = IRMutator::mutate(new_s);

        if (loop && loop->device_api == DeviceAPI::OpenCL && ends_with(loop->name, ".run_on_device")) {
            in_opencl_device_kernel = false;
        }
        return new_s;
    }

    Expr mutate(const Expr &expr) override {
        // This class itself does not mutate an expression. We have given that job to the above misc lowering, gvn and replacer.
        return expr;
    }

};

Stmt standardize_ir_for_opencl_code_gen(Stmt s) {
    // Standardize every statement independently. The standardization process has the following steps:
    // 1. MISC lowering. Lower expressions in a statement to what can be directly expressed by OpenCL. For example, if
    //    an expression returns a vector, and individual elements of that vector have to be accessed later, then we should
    //    store the vector in a variable (using a Let Expr), before it can be accessed element-wise. For another example,
    //    if an expression returns a boolean vector, we have to make it return a vector of e.g. int type, and then cast that
    //    vector to a boolean vector. For other examples, we might convert / into a shift, etc.
    //    Example: a statement looks like
    //               ...= ... Select(cond, e, ...), where expression e has a type of bool2 vector and is not a variable.
    //    After MISC lowering, the statement looks like:
    //               ...= ... Select(cond, Let int2 t = e in (let bool2 t' = Cast(t) in t'), ...)
    // 2. Hoist the Let Exprs created before before the enclosing statement. For the above example:
    //               int2  t  = e if e has no side effect, otherwise int2 t = Select(cond, e, (int2){0, 0})
    //               bool2 t' = Cast(t)
    //               ...= ... Select(cond, t', ...)
    // 3. Also hoist CSEs.
    StmtStandardizerOpenCL standardizer;
    s = standardizer.mutate(s);
    return s;
}

} // Internal
} // Halide
