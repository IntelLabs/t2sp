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

// Return the variable representing the whole expression, if there is such a variable.
const Variable *as_variable(const Expr &e) {
    if (const Variable *op = e.as<Variable>()) {
        return op;
    }
    // For a Let expression:
    //     Let x = value in body
    // body, not x, represents the entire expression. So check if the body is actually a variable.
    // This is a special case we create during standarization. Sometime we have something like
    //     Let x = value in x
    // i.e. the body is just a varaiable. We have this case only because we will hoist that "Let x = value"
    // out of the enclosing statement, but leave that reference to x inside the statement.
    if (const Let * op = e.as<Let>()) {
        return as_variable(op->body);
    }
    return NULL;
}

bool is_variable(const Expr &e) {
    return as_variable(e) != NULL;
}

class MiscLoweringStmtForOpenCL : public IRMutator {
private:
    bool in_opencl_device_kernel;
public:
    MiscLoweringStmtForOpenCL() : in_opencl_device_kernel(false) {}

    Stmt mutate(const Stmt &s) override {
        if (!s.defined()) {
            return s;
        }
        const For *loop = s.as<For>();
        if (loop && loop->device_api == DeviceAPI::OpenCL && ends_with(loop->name, ".run_on_device")) {
            in_opencl_device_kernel = true;
        }
        Stmt new_s = IRMutator::mutate(s);

        if (loop && loop->device_api == DeviceAPI::OpenCL && ends_with(loop->name, ".run_on_device")) {
            in_opencl_device_kernel = false;
        }
        return new_s;
    }

    Expr mutate(const Expr &e) override {
        if (!in_opencl_device_kernel) {
            return e;
        }

        // Modify the expression to desirable computations.
        if (const Add* op = e.as<Add>()) {
            if (const IntImm *int_imm = op->b.as<IntImm>()) {
                if (int_imm->value < 0) {
                    return Sub::make(mutate(op->a), IntImm::make(int_imm->type, -int_imm->value));
                }
            }
        } else if (const Call* op = e.as<Call>()) {
            if (op->is_intrinsic(Call::shift_left) || op->is_intrinsic(Call::shift_right)) {
                // Some OpenCL implementations forbid mixing signed-and-unsigned shift values;
                // if the RHS is uint, quietly cast it back to int if the LHS is int
                if (op->args[0].type().is_int() && op->args[1].type().is_uint()) {
                    Type t = op->args[0].type().with_code(halide_type_int);
                    return Call::make(op->type, op->name, {mutate(op->args[0]), cast(t, mutate(op->args[1]))}, op->call_type);
                }
            }
        } else if (const Div* op = e.as<Div>()) {
            int bits;
            char* ediv = getenv("EUCLIDEAN_DIVISION");
            if (is_const_power_of_two_integer(op->b, &bits)) {
                return Call::make(op->type, Internal::Call::shift_right, {mutate(op->a), make_const(op->a.type(), bits)}, Internal::Call::PureIntrinsic);
            } else if (ediv && op->type.is_int()) {
                return lower_euclidean_div(mutate(op->a), mutate(op->b));
            }
        } else if (const Mod* op = e.as<Mod>()) {
            int bits;
            char* ediv = getenv("EUCLIDEAN_DIVISION");
            if (is_const_power_of_two_integer(op->b, &bits)) {
                return Call::make(op->type, Internal::Call::bitwise_and, {mutate(op->a), make_const(op->a.type(), (1 << bits) - 1)}, Internal::Call::PureIntrinsic);
            } else if (ediv && op->type.is_int()) {
                return lower_euclidean_mod(mutate(op->a), mutate(op->b));
            }
        }
        return IRMutator::mutate(e);
    }
};

class IntermediateVarsForVectorAccess : public IRMutator {
private:
    bool in_opencl_device_kernel;

    Stmt mutate_store(const Store *op) {
        user_assert(is_one(op->predicate)) << "Predicated store is not supported inside OpenCL kernel.\n";
        Stmt new_s = op;
        Expr ramp_base = strided_ramp_base(op->index);
        if (ramp_base.defined()) {
            // Writing a contiguous ramp.
            new_s = Store::make(op->name, mutate(op->value), mutate(op->index), op->param, op->predicate, op->alignment);
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
            new_s = Store::make(op->name, new_value, new_index, op->param, op->predicate, op->alignment);
            if (!is_variable(value1)) {
                new_s = LetStmt::make(new_value_name, value1, new_s);
            }
            if (!is_variable(index1)) {
                new_s = LetStmt::make(new_index_name, index1, new_s);
            }
        }
        return new_s;
    }

    Expr mutate_load(const Load *op) {
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
            return Let::make(index_name, mutate(op->index), load);
        }
        return op;
    }

    Expr save_to_intermediate_vector(const Expr &operand) {
        Expr new_operand = mutate(operand);
        if (!is_variable(new_operand)) {
            Type t = new_operand.type();
            if (t.is_generated_struct() || (t.is_bool() && t.is_vector())) {
                // The new operand is not a variable, and its type is a generated struct. We have to apply the operation to every field
                // of the struct, and thus we need hoist this operand so that code generation looks like:
                //     t = new_operand; // compute the new_operand in a Let Expr.
                //     ... = t.s0 + t.s1 ... // the fields now can be accessed
                // Note a bool vector has no OpenCL native implementation, and has to be implemented in a user-defined struct as well.
                string name = unique_name('t');
                Expr var = Variable::make(t, name);
                Expr e = Let::make(name, new_operand, var);
                return e;
            }
        }
        return new_operand;
    }

public:
    using IRMutator::mutate;

    IntermediateVarsForVectorAccess() : in_opencl_device_kernel(false) {}

    Stmt mutate(const Stmt &s) override {
        if (!s.defined()) {
            return s;
        }
        const For *loop = s.as<For>();
        if (loop && loop->device_api == DeviceAPI::OpenCL && ends_with(loop->name, ".run_on_device")) {
            in_opencl_device_kernel = true;
        }
        Stmt new_s;
        if (in_opencl_device_kernel) {
            if (const Store *op = s.as<Store>()) {
                new_s = mutate_store(op);
            } else {
                new_s = IRMutator::mutate(s);
            }
        } else {
            new_s = IRMutator::mutate(s);
        }

        if (loop && loop->device_api == DeviceAPI::OpenCL && ends_with(loop->name, ".run_on_device")) {
            in_opencl_device_kernel = false;
        }
        return new_s;
    }

    Expr mutate(const Expr &e) override {
        if (!in_opencl_device_kernel) {
            return e;
        }
        if (const Load* op = e.as<Load>()) {
            return mutate_load(op);
        }
        // Create Let Expr for ALU operations' operands that are vectors but we need access their elements in this expression.
        else if (const Add *op = e.as<Add>()) {
            return Add::make(save_to_intermediate_vector(op->a), save_to_intermediate_vector(op->b));
        } else if (const Sub *op = e.as<Sub>()) {
            return Sub::make(save_to_intermediate_vector(op->a), save_to_intermediate_vector(op->b));
        } else if (const Mul *op = e.as<Mul>()) {
            return Mul::make(save_to_intermediate_vector(op->a), save_to_intermediate_vector(op->b));
        } else if (const Div *op = e.as<Div>()) {
            return Div::make(save_to_intermediate_vector(op->a), save_to_intermediate_vector(op->b));
        } else if (const Mod *op = e.as<Mod>()) {
            return Mod::make(save_to_intermediate_vector(op->a), save_to_intermediate_vector(op->b));
        } else if (const Min *op = e.as<Min>()) {
            return Min::make(save_to_intermediate_vector(op->a), save_to_intermediate_vector(op->b));
        } else if (const Max *op = e.as<Max>()) {
            return Max::make(save_to_intermediate_vector(op->a), save_to_intermediate_vector(op->b));
        } else if (const EQ *op = e.as<EQ>()) {
            return EQ::make(save_to_intermediate_vector(op->a), save_to_intermediate_vector(op->b));
        } else if (const NE *op = e.as<NE>()) {
            return NE::make(save_to_intermediate_vector(op->a), save_to_intermediate_vector(op->b));
        } else if (const LT *op = e.as<LT>()) {
            return LT::make(save_to_intermediate_vector(op->a), save_to_intermediate_vector(op->b));
        } else if (const LE *op = e.as<LE>()) {
            return LE::make(save_to_intermediate_vector(op->a), save_to_intermediate_vector(op->b));
        } else if (const GT *op = e.as<GT>()) {
            return GT::make(save_to_intermediate_vector(op->a), save_to_intermediate_vector(op->b));
        } else if (const GE *op = e.as<GE>()) {
            return GE::make(save_to_intermediate_vector(op->a), save_to_intermediate_vector(op->b));
        } else if (const And *op = e.as<And>()) {
            return And::make(save_to_intermediate_vector(op->a), save_to_intermediate_vector(op->b));
        } else if (const Or *op = e.as<Or>()) {
            return Or::make(save_to_intermediate_vector(op->a), save_to_intermediate_vector(op->b));
        } else if (const Not *op = e.as<Not>()) {
            return Not::make(save_to_intermediate_vector(op->a));
        }
        return IRMutator::mutate(e);
    }
};

class CastToBoolVector : public IRMutator {
private:
    bool in_opencl_device_kernel;

    Expr mutate_bool_vector_type(Expr &e, const Type &operand_type) {
        internal_assert(e.type().is_bool() && e.type().is_vector());
        Type bool_vec_type = e.type();
        Type intermediate_type = e.type().with_code(Type::Int).with_bits(operand_type.bits());
        string intermediate_name = unique_name('t');
        Expr intermediate_var = Variable::make(intermediate_type, intermediate_name);
        Expr cast = Cast::make(bool_vec_type, intermediate_var);
        Expr new_e = Let::make(intermediate_name, e, cast);
        e.set_type(intermediate_type); // this prevents the expression to be matched and mutated again.
        new_e.set_type(bool_vec_type);
        return new_e;
    }

public:
    using IRMutator::mutate;

    CastToBoolVector() : in_opencl_device_kernel(false) {}

    Stmt mutate(const Stmt &s) override {
        if (!s.defined()) {
            return s;
        }
        const For *loop = s.as<For>();
        if (loop && loop->device_api == DeviceAPI::OpenCL && ends_with(loop->name, ".run_on_device")) {
            in_opencl_device_kernel = true;
        }
        Stmt new_s = IRMutator::mutate(s);

        if (loop && loop->device_api == DeviceAPI::OpenCL && ends_with(loop->name, ".run_on_device")) {
            in_opencl_device_kernel = false;
        }
        return new_s;
    }

    Expr mutate(const Expr &e) override {
        if (!in_opencl_device_kernel) {
            return e;
        }

        Expr new_e = e;
        // See if we need change the return type of the expression.
        // If the expression returns a bool vector, but its operands are integers, for example, we
        // need generate code like:
        //      int16  x = (y == z);                 // y and z are int16
        //      bool16 b = {x.s0, x.s1, ..., x.s15}; // convert from int16 to bool16
        // Note that a bool vector like bool16 above has to be generated as a struct type, as OpenCL does not
        // support it natively.
        if (new_e.type().is_bool() && new_e.type().is_vector()) {
            if (const EQ *op = new_e.as<EQ>()) {
                return mutate_bool_vector_type(new_e, op->a.type());
            } else if (const NE *op = new_e.as<NE>()) {
                return mutate_bool_vector_type(new_e, op->a.type());
            } else if (const LT *op = new_e.as<LT>()) {
                return mutate_bool_vector_type(new_e, op->a.type());
            } else if (const LE *op = new_e.as<LE>()) {
                return  mutate_bool_vector_type(new_e, op->a.type());
            } else if (const GT *op = new_e.as<GT>()) {
                return mutate_bool_vector_type(new_e, op->a.type());
            } else if (const GE *op = new_e.as<GE>()) {
                return mutate_bool_vector_type(new_e, op->a.type());
            }
        }
        return IRMutator::mutate(new_e);
    }
};

// Hoist Lets as LetStmts. For example:
//      ... = select(cond, Let x = value in body, ...)
// standardize the IR like this:
//      LetStmt x = value if value has no side effect, otherwise select(cond, value), in
//          ... = select(cond, body, ...)
class Let2LetStmt : public IRMutator {
private:
    bool in_opencl_device_kernel;
    vector<pair<string, Expr>> name_value_pairs; // For each "Let x=value" found, record x and value here.
public:
    using IRMutator::mutate;
    Let2LetStmt() : in_opencl_device_kernel(false) { }

    // Create LetStms, according to name_value_pairs, before the current statement s
    Stmt make_LetStmts(const Stmt &s) {
        Stmt new_s = s;
        for (int i = name_value_pairs.size() - 1; i >= 0; i--) {
            new_s = LetStmt::make(name_value_pairs[i].first, name_value_pairs[i].second, new_s);
        }
        name_value_pairs.clear();
        return new_s;
    }

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
            internal_assert(name_value_pairs.size() == 0); // The vector is empty before a statement is processed, and is cleared after that statement is processed..
            if (const LetStmt *op = s.as<LetStmt>()) {
                Stmt new_body = mutate(op->body);
                internal_assert(name_value_pairs.size() == 0);
                new_s = make_LetStmts(LetStmt::make(op->name, mutate(op->value), new_body));
            } else if (const AssertStmt *op = s.as<AssertStmt>()) {
                new_s = make_LetStmts(AssertStmt::make(mutate(op->condition), mutate(op->message)));
            } else if (const ProducerConsumer *op = s.as<ProducerConsumer>()) {
                new_s = ProducerConsumer::make(op->name, op->is_producer, mutate(op->body));
            } else if (const Store *op = s.as<Store>()) {
                user_assert(is_one(op->predicate)) << "Predicated store is not supported inside OpenCL kernel.\n";
                new_s = make_LetStmts(Store::make(op->name, mutate(op->value), mutate(op->index), op->param, op->predicate, op->alignment));
            } else if (const For *op = s.as<For>()) {
                Stmt new_body = mutate(op->body);
                internal_assert(name_value_pairs.size() == 0);
                new_s = make_LetStmts(For::make(op->name, mutate(op->min), mutate(op->extent), op->for_type, op->device_api, new_body));
            } else if (const Provide *op = s.as<Provide>()) {
                vector<Expr> values, args;
                for (auto &v : op->values) {
                    values.push_back(mutate(v));
                }
                for (auto &a : op->args) {
                    args.push_back(mutate(a));
                }
                new_s = make_LetStmts(Provide::make(op->name, values, args));
            } else if (const Allocate *op = s.as<Allocate>()) {
                Stmt new_body = mutate(op->body);
                internal_assert(name_value_pairs.size() == 0);
                vector<Expr> extents;
                for (auto &e : op->extents) {
                    extents.push_back(mutate(e));
                }
                new_s = make_LetStmts(Allocate::make(op->name, op->type, op->memory_type, extents, mutate(op->condition), new_body, mutate(op->new_expr), op->free_function));
            } else if (const Free *op = s.as<Free>()) {
                return op;
            } else if (const Realize *op = s.as<Realize>()) {
                Stmt new_body = mutate(op->body);
                internal_assert(name_value_pairs.size() == 0);
                Region bounds;
                for (auto &b : op->bounds) {
                    bounds.push_back({mutate(b.min), mutate(b.extent)});
                }
                new_s = make_LetStmts(Realize::make(op->name, op->types, op->memory_type, bounds, mutate(op->condition), new_body));
            } else if (const Block *op = s.as<Block>()) {
                Stmt new_first = mutate(op->first);
                internal_assert(name_value_pairs.size() == 0);
                Stmt new_rest = mutate(op->rest);
                internal_assert(name_value_pairs.size() == 0);
                new_s = Block::make(new_first, new_rest);
            } else if (const IfThenElse *op = s.as<IfThenElse>()) {
                Stmt new_then_case = mutate(op->then_case);
                internal_assert(name_value_pairs.size() == 0);
                Stmt new_else_case;
                if (op->else_case.defined()) {
                    new_else_case = mutate(op->else_case);
                }
                internal_assert(name_value_pairs.size() == 0);
                new_s = make_LetStmts(IfThenElse::make(mutate(op->condition), new_then_case, new_else_case));
            } else if (const Evaluate *op = s.as<Evaluate>()) {
                new_s = make_LetStmts(Evaluate::make(mutate(op->value)));
            } else if (const Prefetch *op = s.as<Prefetch>()) {
                Stmt new_body = mutate(op->body);
                internal_assert(name_value_pairs.size() == 0);
                Region bounds;
                for (auto &b : op->bounds) {
                    bounds.push_back({mutate(b.min), mutate(b.extent)});
                }
                new_s = make_LetStmts(Prefetch::make(op->name, op->types, bounds,op->prefetch, mutate(op->condition), new_body));
            } else if (const Acquire *op = s.as<Acquire>()) {
                Stmt new_body = mutate(op->body);
                internal_assert(name_value_pairs.size() == 0);
                new_s = make_LetStmts(Acquire::make(mutate(op->semaphore), mutate(op->count), new_body));
            } else if (const Fork *op = s.as<Fork>()) {
                Stmt new_first = mutate(op->first);
                internal_assert(name_value_pairs.size() == 0);
                Stmt new_rest = mutate(op->rest);
                internal_assert(name_value_pairs.size() == 0);
                new_s = Fork::make(new_first, new_rest);
            } else {
                const Atomic *atomic = s.as<Atomic>();
                internal_assert(atomic);
                new_s = Atomic::make(atomic->producer_name, atomic->mutex_name, mutate(atomic->body));
            }
        } else {
            new_s = IRMutator::mutate(s);
        }

        if (loop && loop->device_api == DeviceAPI::OpenCL && ends_with(loop->name, ".run_on_device")) {
            in_opencl_device_kernel = false;
        }
        return new_s;
    }

    Expr mutate(const Expr &e) override {
        if (!in_opencl_device_kernel) {
            return e;
        }

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

Stmt standardize_ir_for_opencl_code_gen(Stmt s) {
    // Standardize every statement independently. The standardization process has the following steps:
    // 1. MISC lowering. Lower expressions in a statement to desirable compute, e.g. divide by 2 ==> shift by 1
    // 2. If an expression returns a vector, and individual elements of that vector have to be accessed later, then we should
    //    store the vector in a variable (using a Let Expr), before it can be accessed element-wise.
    // 3. If an expression returns a boolean vector, we have to make it return a vector of e.g. int type, and then cast that
    //    vector to a boolean vector. For other examples, we might convert / into a shift, etc.
    //    Example: a statement looks like
    //               ...= ... Select(cond, e, ...), where expression e has a type of bool2 vector and is not a variable.
    //    After MISC lowering, the statement looks like:
    //               ...= ... Select(cond, Let int2 t = e in (let bool2 t' = Cast(t) in t'), ...)
    // 4. Hoist the Let Exprs created before before the enclosing statement as LetStmts. For the above example:
    //               LetStmt int2  t  = e if e has no side effect, otherwise int2 t = Select(cond, e)
    //                 LetStmt bool2 t' = Cast(t)
    //                   ...= ... Select(cond, t', ...)
    // 5. TODO: Also hoist CSEs.

    // Misc lowering of some expressions in this statement according to OpenCL's capability
    Stmt new_s = MiscLoweringStmtForOpenCL().mutate(s);
    debug(4) << "After MiscLoweringStmtForOpenCL:\n" << to_string(new_s) << "\n\n";

    // Save expressions to intermediate variables, if the expressions result in vectors but we need access their individual elements
    new_s = IntermediateVarsForVectorAccess().mutate(new_s);
    debug(4) << "After IntermediateVarsForVectorAccess:\n" << to_string(new_s) << "\n\n";

    // An expression producing a bool vector has to be changed into producing an int vector, then casting into a bool vector, as
    // the bool vector is not standard OpenCL construct, but will be implemented as a compiler-generated struct.
    new_s = CastToBoolVector().mutate(new_s);
    debug(4) << "After CastToBoolVector:\n" << to_string(new_s) << "\n\n";

    // We have created Let expressions in the above step. Make them as LetStmts. For example:
    //      ... = select(cond, Let x = value in body, ...)
    // standardize the IR like this:
    //      LetStmt x = value in
    //          ... = select(cond, body, ...)
    // Then in code generation (NOT in IR), we should find the condition of the LetStmt, and print code like this, e.g.:
    //      int x; if (cond) { x = value; }
    //      if (cond) ... = body else ...
    new_s = Let2LetStmt().mutate(new_s);
    debug(4) << "After Let2LetStmt:\n" << to_string(new_s) << "\n\n";

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
    return new_s;
}

} // Internal
} // Halide
