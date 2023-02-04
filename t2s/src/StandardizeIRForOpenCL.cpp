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
  public:
    MiscLoweringStmtForOpenCL() {}

    Stmt mutate(const Stmt &s) override {
        if (const LetStmt *op = s.as<LetStmt>()) {
            return LetStmt::make(op->name, this->mutate(op->value), op->body);
        } else if (const AssertStmt *op = s.as<AssertStmt>()) {
            return AssertStmt::make(this->mutate(op->condition), this->mutate(op->message));
        } else if (const ProducerConsumer *op = s.as<ProducerConsumer>()) {
            return op;
        } else if (const Store *op = s.as<Store>()) {
            user_assert(is_one(op->predicate)) << "Predicated store is not supported inside OpenCL kernel.\n";

            Expr value = mutate(op->value);
            Expr ramp_base = strided_ramp_base(op->index);
            if (ramp_base.defined()) {
                // Writing a contiguous ramp.
                Stmt store = Store::make(op->name, value, op->index, op->param, op->predicate, op->alignment);
                return store;
            } else if (op->index.type().is_vector()) {
                // If index is a vector, scatter vector elements. But if the index is an expression, it is illegal to use index.s0, etc.
                // To ensure correctness always, we will generate code like this instead:
                //    addr = index; A[addr.s0] = value.s0; A[addr.s1] = value.s1; ...
                string index_name = unique_name('t');
                Expr index_var = Variable::make(op->index.type(), index_name);
                Stmt store = Store::make(op->name, value, index_var, op->param, op->predicate, op->alignment);
                store = LetStmt::make(index_name, mutate(op->index), store);
                return store;
            } else {
                // Do nothing for now
                return op;
            }
        } else if (const For *op = s.as<For>()) {
            return For::make(op->name, this->mutate(op->min), this->mutate(op->extent), op->for_type, op->device_api, op->body);
        } else if (const Provide *op = s.as<Provide>()) {
            vector<Expr> values, args;
            for (auto &v : op->values) {
                values.push_back(this->mutate(v));
            }
            for (auto &a : op->args) {
                args.push_back(this->mutate(a));
            }
            return Provide::make(op->name, values, args);
        } else if (const Allocate *op = s.as<Allocate>()) {
            vector<Expr> extents;
            for (auto &e : op->extents) {
                extents.push_back(this->mutate(e));
            }
            return Allocate::make(op->name, op->type, op->memory_type, extents, this->mutate(op->condition), op->body, this->mutate(op->new_expr), op->free_function);
        } else if (const Free *op = s.as<Free>()) {
            return op;
        } else if (const Realize *op = s.as<Realize>()) {
            Region bounds;
            for (auto &b : op->bounds) {
                bounds.push_back({this->mutate(b.min), this->mutate(b.extent)});
            }
            return Realize::make(op->name, op->types, op->memory_type, bounds, this->mutate(op->condition), op->body);
        } else if (const Block *op = s.as<Block>()) {
            return op;
        } else if (const IfThenElse *op = s.as<IfThenElse>()) {
            return IfThenElse::make(this->mutate(op->condition), op->then_case, op->else_case);
        } else if (const Evaluate *op = s.as<Evaluate>()) {
            return Evaluate::make(this->mutate(op->value));
        } else if (const Prefetch *op = s.as<Prefetch>()) {
            Region bounds;
            for (auto &b : op->bounds) {
                bounds.push_back({this->mutate(b.min), this->mutate(b.extent)});
            }
            return Prefetch::make(op->name, op->types, bounds,op->prefetch, this->mutate(op->condition), op->body);
        } else if (const Acquire *op = s.as<Acquire>()) {
            return Acquire::make(this->mutate(op->semaphore), this->mutate(op->count), op->body);
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
        debug(4) << "MUTATE " << count << ": " << e << "\n";
        if (const Add* op = e.as<Add>()) {
            if (const IntImm *int_imm = op->b.as<IntImm>()) {
                if (int_imm->value < 0) {
                    return Sub::make(op->a, IntImm::make(int_imm->type, -int_imm->value));
                }
            }
            return IRMutator::visit(op);
        } else if (const Call* op = e.as<Call>()) {
            if (op->is_intrinsic(Call::shift_left) || op->is_intrinsic(Call::shift_right)) {
                // Some OpenCL implementations forbid mixing signed-and-unsigned shift values;
                // if the RHS is uint, quietly cast it back to int if the LHS is int
                if (op->args[0].type().is_int() && op->args[1].type().is_uint()) {
                    Type t = op->args[0].type().with_code(halide_type_int);
                    Expr e = Call::make(op->type, op->name, {mutate(op->args[0]), cast(t, mutate(op->args[1]))}, op->call_type);
                    return e;
                }
            }
            return IRMutator::visit(op);
        } else if (const Div* op = e.as<Div>()) {
            int bits;
            char* ediv = getenv("EUCLIDEAN_DIVISION");
            if (is_const_power_of_two_integer(op->b, &bits)) {
                Expr e = Call::make(op->type, Internal::Call::shift_right, {mutate(op->a), make_const(op->a.type(), bits)}, Internal::Call::PureIntrinsic);
                return e;
            } else if (ediv && op->type.is_int()) {
                return lower_euclidean_div(mutate(op->a), mutate(op->b));
            }
            return IRMutator::visit(op);
        } else if (const Mod* op = e.as<Mod>()) {
            int bits;
            char* ediv = getenv("EUCLIDEAN_DIVISION");
            if (is_const_power_of_two_integer(op->b, &bits)) {
                Expr e = Call::make(op->type, Internal::Call::bitwise_and, {mutate(op->a), make_const(op->a.type(), (1 << bits) - 1)}, Internal::Call::PureIntrinsic);
                return e;
            } else if (ediv && op->type.is_int()) {
                return lower_euclidean_mod(mutate(op->a), mutate(op->b));
            }
            return IRMutator::visit(op);
        } else if (const Load* op = e.as<Load>()) {
            user_assert(is_one(op->predicate)) << "Predicated load is not supported inside OpenCL kernel.\n";

            Expr ramp_base = strided_ramp_base(op->index);
            if (ramp_base.defined()) {
                // Loading a contiguous ramp into a vector. Do nothing for now
                return op;
            } else if (op->index.type().is_vector()) {
                // If index is a vector, gather vector elements. But if the index is an expression, it is illegal to use index.s0, etc.
                // To ensure correctness always, we will generate code like this instead:
                //  int8 value; addr = index; value.s0 = A[addr.s0]; value.s1 = A[addr.s1]; ...
                internal_assert(op->type.is_vector());
                string index_name = unique_name('t');
                Expr index_var = Variable::make(op->index.type(), index_name);
                Expr load = Load::make(op->type, op->name, index_var, op->image, op->param, op->predicate, op->alignment);
                load = Let::make(index_name, mutate(op->index), load);
                return load;
            } else {
                // Do nothing for now
                return op;
            }
        } else {
            return IRMutator::mutate(e);
        }
    }
};

class GatherGVNOfEveryExprInStmt : public IRMutator {
private:
    GVN &gvn;
public:
    using IRMutator::mutate;
    GatherGVNOfEveryExprInStmt(GVN &gvn): gvn(gvn) {};

    Stmt mutate(const Stmt &s) override {
        if (const LetStmt *op = s.as<LetStmt>()) {
            return LetStmt::make(op->name, this->mutate(op->value), op->body);
        } else if (const AssertStmt *op = s.as<AssertStmt>()) {
            return AssertStmt::make(this->mutate(op->condition), this->mutate(op->message));
        } else if (const ProducerConsumer *op = s.as<ProducerConsumer>()) {
            return op;
        } else if (const Store *op = s.as<Store>()) {
            return Store::make(op->name, this->mutate(op->value), this->mutate(op->index),
                               op->param, this->mutate(op->predicate), op->alignment);
        } else if (const For *op = s.as<For>()) {
            return For::make(op->name, this->mutate(op->min), this->mutate(op->extent), op->for_type, op->device_api, op->body);
        } else if (const Provide *op = s.as<Provide>()) {
            vector<Expr> values, args;
            for (auto &v : op->values) {
                values.push_back(this->mutate(v));
            }
            for (auto &a : op->args) {
                args.push_back(this->mutate(a));
            }
            return Provide::make(op->name, values, args);
        } else if (const Allocate *op = s.as<Allocate>()) {
            vector<Expr> extents;
            for (auto &e : op->extents) {
                extents.push_back(this->mutate(e));
            }
            return Allocate::make(op->name, op->type, op->memory_type, extents, this->mutate(op->condition), op->body, this->mutate(op->new_expr), op->free_function);
        } else if (const Free *op = s.as<Free>()) {
            return op;
        } else if (const Realize *op = s.as<Realize>()) {
            Region bounds;
            for (auto &b : op->bounds) {
                bounds.push_back(Range(this->mutate(b.min), this->mutate(b.extent)));
            }
            return Realize::make(op->name, op->types, op->memory_type, bounds, this->mutate(op->condition), op->body);
        } else if (const Block *op = s.as<Block>()) {
            return op;
        } else if (const IfThenElse *op = s.as<IfThenElse>()) {
            return IfThenElse::make(this->mutate(op->condition), op->then_case, op->else_case);
        } else if (const Evaluate *op = s.as<Evaluate>()) {
            return Evaluate::make(this->mutate(op->value));
        } else if (const Prefetch *op = s.as<Prefetch>()) {
            Region bounds;
            for (auto &b : op->bounds) {
                bounds.push_back(Range(this->mutate(b.min), this->mutate(b.extent)));
            }
            return Prefetch::make(op->name, op->types, bounds, op->prefetch, this->mutate(op->condition), op->body);
        } else if (const Acquire *op = s.as<Acquire>()) {
            return Acquire::make(this->mutate(op->semaphore), this->mutate(op->count), op->body);
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

// Figure out if an expression should be hoisted out of the statement containing it. Here we use the use_count = 100 to
// represent if the expression should be hoisted.
// Modified from CSE.cpp ComputeUseCounts class
class FindMustHoistExprInStmt : public IRGraphVisitor {
    GVN &gvn;
    const Stmt &parent_stmt; // The statement containing the current expression
    const Expr *parent_expr; // The expression containing the current expression
private:
    // For an operand of a binary or unary operation only:
    bool must_hoist_operand(const Expr &operand) {
        if (!operand.as<Variable>()) {
            Type t = operand.type();
            if (t.is_generated_struct() || (t.is_bool() && t.is_vector())) {
                // Operand is not a variable, and its type is a generated struct. We have to apply the operation to every field
                // of the struct, and thus we need hoist this operand so that code generation looks like:
                //     t = operand; // compute the operand in a Let Expr.
                //     ... = t.s0 + t.s1 ... // the fields now can be accessed
                // Note a bool vector has no OpenCL native implementation, and has to be implemented in a user-defined struct as well.
                return true;
            }
        }
        return false;
    }

    // Some expressions must be hoisted out to make code generation work.
    bool must_hoist(const Expr &e) {
        // If this statement is a Store of a vector into non-contiguous addresses. We need hoist out the index and the value, as
        // we will generate code referring to distinct elements like: addr= ..; v=...; A[addr.s0] = v.s0; A[addr.s1] = v.s1; etc.
        if (const Store *store = parent_stmt.as<Store>()) {
            user_assert(is_one(store->predicate)) << "Predicated store is not supported inside OpenCL kernel.\n";
            Expr ramp_base = strided_ramp_base(store->index);
            if (!ramp_base.defined() && store->index.type().is_vector()) {
                if (e.same_as(store->index) || e.same_as(store->value)) {
                    if (!e.as<Variable>()) {
                        return true;
                    }
                }
                return false;
            }
        }

        // If this expression is an operand with a type of user-defined struct, we may have to hoist it out if we need access its fields.
        if (parent_expr) {
            if (const Add *op = parent_expr->as<Add>()) {
                if (e.same_as(op->a) || e.same_as(op->b)) {
                    return must_hoist_operand(e);
                }
            }
            if (const Sub *op = parent_expr->as<Sub>()) {
                if (e.same_as(op->a) || e.same_as(op->b)) {
                    return must_hoist_operand(e);
                }
            }
            if (const Mul *op = parent_expr->as<Mul>()) {
                if (e.same_as(op->a) || e.same_as(op->b)) {
                    return must_hoist_operand(e);
                }
            }
            if (const Div *op = parent_expr->as<Div>()) {
                if (e.same_as(op->a) || e.same_as(op->b)) {
                    return must_hoist_operand(e);
                }
            }
            if (const Mod *op = parent_expr->as<Mod>()) {
                if (e.same_as(op->a) || e.same_as(op->b)) {
                    return must_hoist_operand(e);
                }
            }
            if (const Min *op = parent_expr->as<Min>()) {
                if (e.same_as(op->a) || e.same_as(op->b)) {
                    return must_hoist_operand(e);
                }
            }
            if (const Max *op = parent_expr->as<Max>()) {
                if (e.same_as(op->a) || e.same_as(op->b)) {
                    return must_hoist_operand(e);
                }
            }
            if (const EQ *op = parent_expr->as<EQ>()) {
                if (e.same_as(op->a) || e.same_as(op->b)) {
                    return must_hoist_operand(e);
                }
            }
            if (const NE *op = parent_expr->as<NE>()) {
                if (e.same_as(op->a) || e.same_as(op->b)) {
                    return must_hoist_operand(e);
                }
            }
            if (const LT *op = parent_expr->as<LT>()) {
                if (e.same_as(op->a) || e.same_as(op->b)) {
                    return must_hoist_operand(e);
                }
            }
            if (const LE *op = parent_expr->as<LE>()) {
                if (e.same_as(op->a) || e.same_as(op->b)) {
                    return must_hoist_operand(e);
                }
            }
            if (const GT *op = parent_expr->as<GT>()) {
                if (e.same_as(op->a) || e.same_as(op->b)) {
                    return must_hoist_operand(e);
                }
            }
            if (const GE *op = parent_expr->as<GE>()) {
                if (e.same_as(op->a) || e.same_as(op->b)) {
                    return must_hoist_operand(e);
                }
            }
            if (const And *op = parent_expr->as<And>()) {
                if (e.same_as(op->a) || e.same_as(op->b)) {
                    return must_hoist_operand(e);
                }
            }
            if (const Or *op = parent_expr->as<Or>()) {
                if (e.same_as(op->a) || e.same_as(op->b)) {
                    return must_hoist_operand(e);
                }
            }
            if (const Not *op = parent_expr->as<Not>()) {
                if (e.same_as(op->a)) {
                    return must_hoist_operand(e);
                }
            }
        }

        if (const Broadcast *a = e.as<Broadcast>()) {
            return must_hoist(a->value);
        }

        if (const Cast *a = e.as<Cast>()) {
            return must_hoist(a->value);
        }

        if (const Ramp *a = e.as<Ramp>()) {
            return !is_const(a->stride);
        }

        return false;
    }

public:
    FindMustHoistExprInStmt(GVN &g, const Stmt &s) : gvn(g), parent_stmt(s), parent_expr(NULL) { }

    using IRGraphVisitor::include;
    using IRGraphVisitor::visit;

    // Find hoistable expressions only in this statement, but do not go down to other statements.
    void include(const Stmt &s) override {
        if (const LetStmt *op = s.as<LetStmt>()) {
            this->include(op->value);
        } else if (const AssertStmt *op = s.as<AssertStmt>()) {
            this->include(op->condition);
            this->include(op->message);
        } else if (s.as<ProducerConsumer>()) {
            // Do nothing
        } else if (const Store *op = s.as<Store>()) {
            this->include(op->predicate);
            this->include(op->value);
            this->include(op->index);
        } else if (const For *op = s.as<For>()) {
            this->include(op->min);
            this->include(op->extent);
        } else if (const Provide *op = s.as<Provide>()) {
            for (auto &v : op->values) {
                this->include(v);
            }
            for (auto &a : op->args) {
                this->include(a);
            }
        } else if (const Allocate *op = s.as<Allocate>()) {
            for (auto &e : op->extents) {
                this->include(e);
            }
            this->include(op->condition);
            this->include(op->new_expr);
        } else if (s.as<Free>()) {
            // Do nothing
        } else if (const Realize *op = s.as<Realize>()) {
            this->include(op->condition);
            for (auto &b : op->bounds) {
                this->include(b.min);
                this->include(b.extent);
            }
        } else if (s.as<Block>()) {
            // Do nothing
        } else if (const IfThenElse *op = s.as<IfThenElse>()) {
            this->include(op->condition);
        } else if (const Evaluate *op = s.as<Evaluate>()) {
            this->include(op->value);
        } else if (const Prefetch *op = s.as<Prefetch>()) {
            for (auto &b : op->bounds) {
                this->include(b.min);
                this->include(b.extent);
            }
            this->include(op->condition);
        } else if (const Acquire *op = s.as<Acquire>()) {
            this->include(op->semaphore);
            this->include(op->count);
        } else if (s.as<Fork>()) {
            // Do nothing
        } else {
            internal_assert(s.as<Atomic>());
            // Do nothing
        }
    }

    void include(const Expr &e) override {
        if (must_hoist(e)) {
            debug(4) << "FindMustHoistExprInStmt: must hoist " << e << "\n";

            // Find this thing's number.
            auto iter = gvn.output_numbering.find(e);
            if (iter != gvn.output_numbering.end()) {
                gvn.entries[iter->second]->use_count = 100;
            } else {
                internal_error << "Expr not in shallow numbering!\n";
            }
        }

        // Visit the children if we haven't been here before.
        const Expr *original_parent_expr = parent_expr;
        parent_expr = &e;
        if (e.as<IntImm>() || e.as<UIntImm>() || e.as<FloatImm>() || e.as<StringImm>() ||e.as<Variable>()) {
            // Nothing to do
        } else if (const Cast *op = e.as<Cast>()) {
            this->include(op->value);
        } else if (const Add *op = e.as<Add>()) {
            this->include(op->a);
            this->include(op->b);
        } else if (const Sub *op = e.as<Sub>()) {
            this->include(op->a);
            this->include(op->b);
        } else if (const Mul *op = e.as<Mul>()) {
            this->include(op->a);
            this->include(op->b);
        } else if (const Div *op = e.as<Div>()) {
            this->include(op->a);
            this->include(op->b);
        } else if (const Mod *op = e.as<Mod>()) {
            this->include(op->a);
            this->include(op->b);
        } else if (const Min *op = e.as<Min>()) {
            this->include(op->a);
            this->include(op->b);
        } else if (const Max *op = e.as<Max>()) {
            this->include(op->a);
            this->include(op->b);
        } else if (const EQ *op = e.as<EQ>()) {
            this->include(op->a);
            this->include(op->b);
        } else if (const NE *op = e.as<NE>()) {
            this->include(op->a);
            this->include(op->b);
        } else if (const LT *op = e.as<LT>()) {
            this->include(op->a);
            this->include(op->b);
        } else if (const LE *op = e.as<LE>()) {
            this->include(op->a);
            this->include(op->b);
        } else if (const GT *op = e.as<GT>()) {
            this->include(op->a);
            this->include(op->b);
        } else if (const GE *op = e.as<GE>()) {
            this->include(op->a);
            this->include(op->b);
        } else if (const And *op = e.as<And>()) {
            this->include(op->a);
            this->include(op->b);
        } else if (const Or *op = e.as<Or>()) {
            this->include(op->a);
            this->include(op->b);
        } else if (const Not *op = e.as<Not>()) {
            this->include(op->a);
        } else if (const Select *op = e.as<Select>()) {
            this->include(op->condition);
            this->include(op->true_value);
            this->include(op->false_value);
        } else if (const Load *op = e.as<Load>()) {
            this->include(op->predicate);
            this->include(op->index);
        } else if (const Ramp *op = e.as<Ramp>()) {
            this->include(op->base);
            this->include(op->stride);
        } else if (const Broadcast *op = e.as<Broadcast>()) {
            this->include(op->value);
        } else if (const Call *op = e.as<Call>()) {
            for (auto &a : op->args) {
                this->include(a);
            }
        } else if (const Let *op = e.as<Let>()) {
            this->include(op->value);
            this->include(op->body);
        } else {
            internal_assert(e.as<Shuffle>());
            for (auto &v : e.as<Shuffle>()->vectors) {
                this->include(v);
            }
        }
        parent_expr = original_parent_expr;
    }
};


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
            // Misc lowering of some expressions in this statement according to OpenCL's capability
            debug(4) << "MiscLoweringStmtForOpenCL: " << "Statement before:\n" << to_string(new_s) << "\n";
            new_s = MiscLoweringStmtForOpenCL().mutate(new_s);
            debug(4) << "MiscLoweringStmtForOpenCL: " << "Statement after:\n" << to_string(new_s) << "\n\n";

            // Use statement-level global value numbering to label distinct expressions within the statement
            // We will hoist out expressions with multiple uses before the statement to avoid redundant computation, also make the generated code clearer.
            GVN gvn;
            new_s = GatherGVNOfEveryExprInStmt(gvn).mutate(new_s);

            // We will also hoist out some expressions that have no equivalent OpenCL expressions. Their use_count is marked as 100.
            FindMustHoistExprInStmt find_hoistable(gvn, new_s);
            find_hoistable.include(new_s);

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
            }
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
    // Standardize every statement
    StmtStandardizerOpenCL standardizer;
    s = standardizer.mutate(s);
    return s;
}

} // Internal
} // Halide
