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
#include "CSE.h"
#include "IREquality.h"
#include "IRMatch.h"
#include "IRMutator.h"
#include "IROperator.h"
#include "Scope.h"
#include "Simplify.h"
#include "../../t2s/src/Utilities.h"
#include "../../t2s/src/StandardizeIRForOpenCL.h"

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

  private:
    using IRMutator::visit;

    Expr visit(const Call* op) override {
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
    }

    Expr visit(const Div *op) override {
		int bits;
		char* ediv = getenv("EUCLIDEAN_DIVISION");
		if (is_const_power_of_two_integer(op->b, &bits)) {
			Expr e = Call::make(op->type, Internal::Call::shift_right, {mutate(op->a), make_const(op->a.type(), bits)}, Internal::Call::PureIntrinsic);
			return e;
		} else if (ediv && op->type.is_int()) {
			return lower_euclidean_div(mutate(op->a), mutate(op->b));
		}
		return IRMutator::visit(op);
    }

    Expr visit(const Mod *op) override {
		int bits;
		char* ediv = getenv("EUCLIDEAN_DIVISION");
		if (is_const_power_of_two_integer(op->b, &bits)) {
			Expr e = Call::make(op->type, Internal::Call::bitwise_and, {mutate(op->a), make_const(op->a.type(), (1 << bits) - 1)}, Internal::Call::PureIntrinsic);
			return e;
		} else if (ediv && op->type.is_int()) {
			return lower_euclidean_mod(mutate(op->a), mutate(op->b));
		}
		return IRMutator::visit(op);
    }

    Expr visit(const Load *op) override {
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
    }
};

// This does not really mutate any expression. Jus to take advantage of the mutate(Expr) interface.
class GatherGVNOfEveryExprInStmt : public IRMutator {
private:
	GVN &gvn;
public:
    using IRMutator::mutate;

    Stmt mutate(const Stmt &s) override {
    	internal_error << "GatherGVNOfEveryExprInStmt targets only expressions (within a Stmt). It cannot be applied to a Stmt: " << s << "\n";
    	return Stmt();
    }

    Expr mutate(const Expr &e) override {
    	return gvn.mutate(e);
    }

    GatherGVNOfEveryExprInStmt(GVN &gvn): gvn(gvn) {};
};

// Figure out if an expression should be hoisted out of the statement containing it. Here we use the use_count = 100 to
// represent if the expression should be hoisted.
// Modified from CSE.cpp ComputeUseCounts class
class FindEveryHoistableExprInStmt : public IRGraphVisitor {
    GVN &gvn;
    const Stmt &s; // The statement to look at
private:
    // Some expressions are not worth hoisting out. Some must be hoisted out.
    bool should_hoist(const Expr &e) {
        if (is_const(e) || e.as<Variable>()) {
            return false;
        }

        // If this statement is a Store of a vector into non-contiguous addresses. We need hoist out the index and the value, as
		// we will generate code referring to distinct elements like v.s* here: addr= ..; v=...; A[addr.s0] = v.s0; A[addr.s1] = v.s1; etc.
        if (s.as<Store>()) {
        	const Store *store = s.as<Store>();
			user_assert(is_one(store->predicate)) << "Predicated store is not supported inside OpenCL kernel.\n";
			Expr ramp_base = strided_ramp_base(store->index);
			if (!ramp_base.defined() && store->index.type().is_vector() && (e.same_as(store->index) || e.same_as(store->value))) {
				return true;
			}
			return false;
        }

        if (const Broadcast *a = e.as<Broadcast>()) {
            return should_hoist(a->value);
        }

        if (const Cast *a = e.as<Cast>()) {
            return should_hoist(a->value);
        }

        if (const Ramp *a = e.as<Ramp>()) {
            return !is_const(a->stride);
        }

        return true;
    }

public:
    FindEveryHoistableExprInStmt(GVN &g, const Stmt &s) : gvn(g), s(s) { }

    using IRGraphVisitor::include;
    using IRGraphVisitor::visit;

    void include(const Stmt &s) override {
    	internal_error << "ComputeHoistInfo targets only expressions (within a Stmt). It cannot be applied to a Stmt: " << s << "\n";
    }

    void include(const Expr &e) override {
        // If it's not the sort of thing we want to extract as a let,
        // just use the generic visitor to increment use counts for
        // the children.
        debug(4) << "FindEveryHoistableExprInStmt: " << e
                 << "; should hoist: " << should_hoist(e) << "\n";
        if (!should_hoist(e)) {
            e.accept(this);
            return;
        }

        // Find this thing's number.
        auto iter = gvn.output_numbering.find(e);
        if (iter != gvn.output_numbering.end()) {
            gvn.entries[iter->second]->use_count = 100;
        } else {
            internal_error << "Expr not in shallow numbering!\n";
        }

        // Visit the children if we haven't been here before.
        IRGraphVisitor::include(e);
    }
};


// Standardize each statement in the IR for OpenCL code generation
class StmtStandardizerOpenCL : public IRMutator {
  public:
	StmtStandardizerOpenCL() : in_opencl_device_kernel(false) {}

  private:
    using IRMutator::visit;

    bool in_opencl_device_kernel;

    Stmt mutate(const Stmt &s) override {
    	const For *loop = s.as<For>();
        if (loop && loop->device_api == DeviceAPI::OpenCL && ends_with(loop->name, ".run_on_device")) {
            in_opencl_device_kernel = true;
        }
        Stmt new_s = s;
    	if (in_opencl_device_kernel) {
    		// Misc lowering of some expressions in this statement according to OpenCL's capability
    		new_s = MiscLoweringStmtForOpenCL().mutate(new_s);

			// Use statement-level global value numbering to label distinct expressions within the statement
    		// We will hoist out expressions with multiple uses before the statement to avoid redundant computation, also make the generated code clearer.
			GVN gvn;
			new_s = GatherGVNOfEveryExprInStmt(gvn).mutate(new_s);

			FindEveryHoistableExprInStmt find_hoistable(gvn);
			find_hoistable.include(new_s);

			// Figure out which ones we'll pull out as lets and variables.
			vector<pair<string, Stmt>> lets;
			map<Expr, Expr, ExprCompare> replacements;
			for (size_t i = 0; i < gvn.entries.size(); i++) {
				const auto &e = gvn.entries[i];
				if (e->use_count == 100) {
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
   		new_s = IRMutator::visit(new_s);

   		if (loop && loop->device_api == DeviceAPI::OpenCL && ends_with(loop->name, ".run_on_device")) {
            in_opencl_device_kernel = false;
        }
		return new_s;
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
