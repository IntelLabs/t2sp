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
#include "./DebugPrint.h"
#include "./Utilities.h"
#include "../../Halide/src/IRMutator.h"
#include "../../Halide/src/IREquality.h"
#include "../../Halide/src/ImageParam.h"

namespace Halide {

using std::max;
using std::min;
using std::map;
using std::string;
using std::vector;
using std::set;
using std::pair;
using std::make_pair;
using std::ofstream;
using std::tuple;

using namespace Internal;

// A struct converted from FuncOrExpr, by instantiating the Func (instead of a pointer),
// and by converting an ImageParam to Func (to simplify subsequent procesing).
struct Func_Expr {
    Func func;
    Expr expr;
    bool  is_expr;
};

string to_string(const Func_Expr &f) {
    std::ostringstream s;
    s << (f.is_expr ? to_string(f.expr) : f.func.name());
    return s.str();
}

string names_to_string(const vector<Func_Expr> &fs) {
    std::ostringstream s;
    for (size_t i = 0; i < fs.size(); i++) {
        s << ((i==0) ? "" : ", ") << to_string(fs[i]);
    }
    return s.str();
}

class FindDirectCalls : public IRVisitor {
public:
    // The unique direct callees with the given call types in an expression.
    vector<string>         callees;
    vector<Call::CallType> call_types;
    using IRVisitor::visit;

    FindDirectCalls(vector<Call::CallType> call_types) : call_types(call_types) {}

    void visit(const Call *op) override {
        IRVisitor::visit(op);
        if (std::find(call_types.begin(), call_types.end(), op->call_type) != call_types.end()) {
            if (std::find(callees.begin(), callees.end(), op->name) == callees.end()) {
                callees.push_back(op->name);
            }
        }
    }
};

// Find all the Halide Funcs that are depended on by the given operand.
void find_dependees(const Expr         &operand,     // The operand for whom to find its dependees
                    const vector<Func> &candidates,  // Dependees will be found from these candidates
                    vector<Func>       &dependees) { // Dependees found will be appended into the vector
    if (!operand.defined() || candidates.empty()) {
        return;
    }
    // Find the unique Halide functions directly called in the operand
    vector<string>  callees;
    FindDirectCalls finder({Call::Halide});
    operand.accept(&finder);
    callees = finder.callees;

    // Find the Halide functions transitively called in the operand
    size_t i = 0;
    while (i < callees.size()) {
        string callee = callees[i];
        // Get all the functions called by this callee directly.
        for (auto c : candidates) {
            if (c.name() == callee) {
                vector<Expr> values = c.values().as_vector();
                for (auto v : values) {
                    FindDirectCalls new_finder({Call::Halide});
                    v.accept(&new_finder);
                    for (auto new_callee : new_finder.callees) {
                        if (std::find(callees.begin(), callees.end(), new_callee) == callees.end()) {
                            callees.push_back(new_callee);
                        }
                    }
                }
                break;
            }
        }
        i++;
    }

    // Add the corresponding Funcs to dependees
    for (auto callee : callees) {
        bool added = false;
        for (auto d : dependees) {
            if (callee == d.name()) {
                added = true;
                break;
            }
        }
        if (!added) {
            for (auto candidate : candidates) {
                if (callee == candidate.name()) {
                    dependees.push_back(candidate);
                    break;
                }
            }
        }
    }
}

// All UREs, including Func f and all the other UREs it has merged with.
void all_ures(Func          f,
              vector<Func> &ures) {
    ures.push_back(f);
    for (auto other : f.function().definition().schedule().merged_ures()) {
        ures.push_back(other);
    }
}

// Check preconditions for isolating a set of Funcs (fs) from a consumer to a chain of producers.
void check_preconditions_of_isolate_producers(const vector<Func_Expr> &fs, const Func &consumer, const vector<Func> &producers) {
    user_assert(!fs.empty()) << "Isolating producer(s): no Func calls specified for isolation.\n";
    user_assert(!producers.empty()) << "Isolating producer(s): no producers specified.\n";

    // The UREs (the consumer and all the  Funcs it has merged with) must have one and only one non-extern definition.
    user_assert(consumer.defined()) << "Isolating producer(s) from an undefined Func " << consumer.name() << "\n";
    user_assert(!consumer.has_update_definition())  << "Isolating producers from Func " << consumer.name()
        << ", which has more than one definition.\n";
    user_assert(!consumer.is_extern()) << "Isolating producers from an extern Func " << consumer.name() << "\n";

    vector<Func> ures;
    all_ures(consumer, ures);
    for (size_t i = 1; i < ures.size(); i++) {  // We have checked consumer (i.e. ures[0]) before.
        Func f = ures[i];
        user_assert(f.defined()) << "Isolating producers from Func " << consumer.name()
                                 << " failed: Func " << f.name() << " is undefined\n";
        user_assert(!f.has_update_definition())  << "Isolating producers from Func " << consumer.name()
                                 << " failed: Func " << f.name() << " has more than one definition.\n";
        user_assert(!f.is_extern()) << "Isolating producers from Func " << consumer.name()
                                 << " failed: Func " << f.name() << " is an external function.\n";
    }

    // The consumer must be the representative of all the other UREs, and therefore, is not merged into any other Func
    user_assert(!consumer.function().definition().schedule().is_merged()) << "Cannot isolate from Func " << consumer.name()
            << ", which has been merged as a URE into another Func (the representative of the merged UREs). "
            << "To avoid the error, isolate from that reprensative Func instead.\n";

    // The producers are yet to be defined by the isolation
    for (auto p : producers) {
        user_assert(!p.defined()) << "Isolating (" << names_to_string(fs)
                    << ") into an already defined Func " << p.name()
                    << ", which is expected to be undefined before the isolation.\n";
    }

    // A Func or Expr to be isolated cannot be a component of another Func or Expr to be isolated. For example, you can
    // not isolate a + b and a, because a is a component of a + b. Instead, you can isolate, e.g. a + b and a + 0 (or a * 1), because
    // a + 0 (or a * 1) is not part of a + b (The compiler does not reduce a + 0 or a * 1 to a during isolation).
    for (size_t i = 0; i < fs.size(); i++) {
        for (size_t j = 0; j < fs.size(); j++) {
            if (i != j) {
                // Check if fs[j] is part of fs[i]
                if (!fs[i].is_expr) { // fs[i] is a Func (a name to match in the IR)
                    if (!fs[j].is_expr) { // fs[j] is also a Func (a name to match in the IR)
                        // Just check they are not the same
                        user_assert(fs[i].func.name() != fs[j].func.name()) << "Cannot isolate the same call to Func " << fs[i].func.name() << " twice.\n";
                    } else {
                        // fs[j] is an Expr, and not possible to be part of fs[i]. Do nothing.
                    }
                } else {
                    user_assert(!fs[j].is_expr || !equal(fs[i].expr, fs[j].expr)) << "Cannot isolate expression " + to_string(fs[j]) + " twice.\n";
                    SliceExprTree slicer(fs[j].is_expr ? "" : fs[j].func.name(), fs[j].is_expr ? fs[j].expr : Expr(), "Checking if " + to_string(fs[j]) + " is part of the expression " + to_string(fs[i].expr) + ": ");
                    fs[i].expr.accept(&slicer);
                    user_assert(!slicer.matched_operand.defined()) << "Cannot isolate both " + to_string(fs[j]) + " and " + to_string(fs[i].expr)
                            << ", as the former is a part of the latter. Try making them not contain each other. For example, instead of\n"
                            << "    Func f;\n"
                            << "    f(a, b) = select(..., a + b, a);\n"
                            << "    f.isolate_producer({a + b, a}, ...);\n"
                            << "you can\n"
                            << "    Func f;\n"
                            << "    f(a, b) = select(..., a + b, a + 0);\n"
                            << "    f.isolate_producer({a + b, a + 0}, ...);\n"
                            << "As long as the two operands to isolate are not containing each other, any other equivalent change (e.g. a * 1 instead of a + 0) should also work.\n";
                }
            }
        }
    }

    // To isolate out a call to a Func f, we require there is one and only one
    // such a call.
    for (auto f : fs) {
        bool matched = false;
        Expr matched_operand;
        Expr matched_condition;
        vector<const Let*> matched_lets;
        Func matched_func;
        for (auto u: ures) {
            vector<Expr> values = u.values().as_vector();
            for (auto v : values) {
                SliceExprTree slicer(f.is_expr ? "" : f.func.name(), f.is_expr ? f.expr : Expr(), "In isolating " + to_string(f) + " from Func " + u.name() + ": ");
                debug(4) << "Check_preconditions_of_isolate_producers: Slice tree " << v << " for " << to_string(f) << "\n";
                v.accept(&slicer);
                if (slicer.matched_operand.defined()) {
                    user_assert(!matched) << "Error: More than 1 occurrence of Func " << to_string(f)
                                      << " to isolate. A unique occurrence is required for isolation.\n"
                                      << "Try to uniquefy the occurrences. For example, in an Func\n"
                                      << "    f(x, y)=...g(x, y)...g(x-1, y)... \n"
                                      << "where g appears twice, and "
                                      << "thus cannot be isolated. But if we define another Func h:\n"
                                      << "    h(x, y) = g(x, y)\n"
                                      << "    f(x, y)=...h(x, y)...h(x-1, y)...\n"
                                      << "then g appears only once, and can be isolated.\n";
                    matched = true;
                    matched_operand = slicer.matched_operand;
                    matched_condition = slicer.condition;
                    matched_lets = slicer.lets;
                    matched_func = u;
                }

            }
        }
        user_assert(matched) << "Func " << to_string(f) << " is not used in, and thus cannot be isolated from, Func "
                << consumer.name() << ".\n"
                << "Here are related definition(s):\n" << to_string(consumer, true, true) ;

        // Let us say the call to Func f is inside an URE like this:
        //   1. Func f, g;
        //   2. f(i, j) = something;
        //   3. g(i, j) = select(condition, f(i, j-1), something else);
        // That call to f depends on the condition of the Select and the definition of
        // f (Line 2 above), which may further depend on some other Funcs.
        // All these (condition, defintion of f, and the some other Funcs) are called
        // "dependees". When we isolate the call, we also isolate out all its dependees.
        // These dependees should not include the definition of g above (Line 3), because
        // we just want to isolate out an RHS operand in it, not its entire definition.
        vector<Func> dependees;
        find_dependees(matched_operand, ures, dependees);
        find_dependees(matched_condition, ures, dependees);
        // TOFIX: should we check the dependeeds of the matched lets as well?
        //for (auto l : matched_lets) {
        //    find_dependee_defs(l, dependees);
        //}
        for (auto d : dependees) {
            user_assert(d.name() != matched_func.name())
                    << "In isolating an operand (" << to_string(f) <<")"
                    << ", the operand's entire containing Func (" << d.name()
                    << ") is unexpectedly found to have to be isolated as a dependee.\n"
                    << "Check if the operand is directly/indirectly defined by Func " << d.name()
                    << ". For example,\n"
                    << "     Func f, g;\n"
                    << "     f(i) = g(i-1);\n"
                    << "     g(i) = f(i);\n"
                    << "     g.merge_ures(f).isolate_producer_chain(f);\n"
                    << "It is obvious that the operand f(i) is cyclically dependent on g, which "
                    << "contains f(i). In general, we isolate both an operand and its dependees out together "
                    << "to make a self-contained producer; but the operand's containing Func (in this case, g) "
                    << "should not be isolated: we aim to isolate this operand, not its entire containing Func.\n";
        }
    }
}

// In the given UREs, match calls to the Funcs (fs), and also find the dependees
// of the calls.
void match_calls_and_find_dependees(const vector<Func_Expr> &fs,
                                    const vector<Func>       &ures,
                                    vector<Expr>             &matched_operands,
                                    vector<Expr>             &matched_exprs,
                                    vector<Func>             &dependees) {
    for (auto f : fs) {
        for (auto u: ures) {
            vector<Expr> values = u.values().as_vector();
            for (auto v : values) {
                SliceExprTree slicer(f.is_expr ? "" : f.func.name(), f.is_expr ? f.expr : Expr(), "In isolating " + to_string(f) + " from Func " + u.name() + ": ");
                v.accept(&slicer);
                if (slicer.matched_operand.defined()) {
                    matched_operands.push_back(slicer.matched_operand);
                    Expr matched_expr;
                    if (slicer.condition.defined()) {
                        matched_expr = Select::make(slicer.condition, slicer.matched_operand, Expr());
                    } else {
                        matched_expr = slicer.matched_operand;
                    }
                    // Add the Let Exprs along the path to the matched operand.
                    for (int i = slicer.lets.size() - 1; i >= 0; i--) {
                        matched_expr = Let::make(slicer.lets[i]->name, slicer.lets[i]->value, matched_expr);
                    }
                    matched_exprs.push_back(matched_expr);
                    find_dependees(matched_expr, ures, dependees);

                    // Our preconditions (checked before) require that there is one and only one call to f.
                    goto nextf;
                }
            }
        }
nextf: ;
    }
}

struct FuncCompare
{
   bool operator() (const Func& lhs, const Func& rhs) const
   {
       return lhs.name() < rhs.name();
   }
};

void clone_func(Func &clone, const string &name, Place place, const Func &original) {
    map<FunctionPtr, FunctionPtr> empty; // No existing clone.
    original.function().deep_copy(name, clone.function().get_contents(), empty);
    clone.function().place(place);

    clone.function().definition().schedule().merged_ures().clear();
    clone.function().definition().schedule().is_merged() = false;
    clone.function().definition().schedule().fuse_level() = FuseLoopLevel();
}

// Clone the dependees. Build a map from a dependee to its clone.
void clone_dependees(const vector<Func>           &dependees,
                     vector<Func>                 &clones,
                     map<Func, Func, FuncCompare> &originals_to_clones,
                     const Func                   &p) { // The producer Func to be created later.
    internal_assert(clones.empty()); // nothing cloned yet.
    for (auto d : dependees) {
        Func clone;
        clone_func(clone, p.name() + (string)"_" + d.name(), p.function().place(), d);
        clones.push_back(clone);
        originals_to_clones[d] = clone;
    }
    // The dependees are bottom up, and thus the cloned funcs. Reverse the order to top down
    std::reverse(clones.begin(), clones.end());
}

// Define new Funcs for the matched expression to the given Funcs in fs.
// Build a map from fs to the new Funcs.
void define_funcs_for_matched_exprs(const vector<Func_Expr> &fs,// The Funcs whose calls, or Exprs, were matched
                                    const Func              &c, // The consumer Func, from where the calls to fs were matched
                                    const Func              &p, // The producer Func to be created later.
                                    const vector<Expr>      &matched_exprs,               // The matched calls
                                    vector<Func>            &new_funcs_for_matched_exprs // The new Funcs to create for each matched calls
                                   ) {
    // Every Func in fs has one and only one call matched
    internal_assert(fs.size() == matched_exprs.size());
    // The consumer Func has one and only one definition
    internal_assert(c.defined() && !c.has_update_definition());

    vector<Expr> args = c.function().definition().args();
    for (size_t i = 0; i < fs.size(); i++) {
        Func_Expr f = fs[i];
        Expr       e = matched_exprs[i];
        string     new_func_name = p.name() + (string)"_" + (f.is_expr ? (string) "e" : f.func.name());
        Func new_func(new_func_name, p.function().place());
        new_func(args) = e;
        new_funcs_for_matched_exprs.push_back(new_func);
        // The new Func is not to be inlined anywhere.
        new_func.compute_root();
    }
}

// Replace the given operands with calls to the corresponding Funcs with the given args
class SubstituteOperands : public IRMutator {
    using IRMutator::visit;

    const vector<Expr> &operands;
    const vector<Func> &funcs;
    const vector<Expr> &args;

    Expr mutate(const Expr &expr) override {
        for (size_t i = 0; i < operands.size(); i++) {
            if (equal(expr, operands[i])) {
                Expr e = Call::make(funcs[i].function(), args);
                total_replaced++;
                return e;
            }
        }
        return IRMutator::mutate(expr);
    }

    Stmt mutate(const Stmt &stmt) override {
        return IRMutator::mutate(stmt);
    }

public:
    unsigned int total_replaced;

    SubstituteOperands(const vector<Expr> &operands,  const vector<Func> &funcs, const vector<Expr> &args)
        : operands(operands), funcs(funcs), args(args) {
        internal_assert(operands.size() == funcs.size());
        total_replaced = 0;
    }
};

// Replace all calls to a Halide function with a substitute.
class SubstituteCalls : public IRMutator {
    using IRMutator::visit;

    const Func &original;
    const Func &substitute;

    Expr visit(const Call *c) override {
        Expr expr = IRMutator::visit(c);
        c = expr.as<Call>();
        internal_assert(c);

        if (c->name == original.name()) {
            debug(4) << "...Replace call to Func \"" << c->name << "\" with "
                     << "\"" << substitute.name() << "\"\n";
            internal_assert(c->call_type == Call::Halide);
            // The substitue is a Halide Func that has one and only one definition
            internal_assert(substitute.defined() && !substitute.has_update_definition());
            expr = Call::make(substitute.function(), c->args);
        }
        return expr;
    }

public:
    SubstituteCalls(const Func &original, const Func &substitute)
        : original(original), substitute(substitute) {
    }
};

// In Func f, substitute every call to a Func in the map to a call to
// its mapped Func.
void substitute_calls(Func                               &f,
                      const map<Func, Func, FuncCompare> &old_to_new) {
    for (auto element : old_to_new) {
        Func original = element.first;
        Func substitute = element.second;

        SubstituteCalls subs_calls(original, substitute);
        f.function().mutate(&subs_calls);
    }
}

void fix_map(map<Func, Func, FuncCompare> &m, const Func &dest, const Func &new_dest) {
    for (auto itr : m) {
        if (itr.second.name() == dest.name()) {
            m[itr.first] = new_dest;
            return;
        }
    }
}

void convert_to_Func_Expr(const vector<FuncOrExpr> &_fs, vector<Func_Expr> &fs) {
    for (auto _f : _fs) {
        Func_Expr f;
        if (_f.is_image) {
            const ImageParam &i = *(_f.image);
            f.func = (Func) i;
            f.is_expr = false;
        } else if (_f.is_expr) {
            f.expr = _f.expr;
            f.is_expr = true;
        } else {
            f.func = *_f.func;
            f.is_expr = false;
        }
        fs.push_back(f);
    }
};

// Isolate the calls of Funcs in fs from this Func to Func p (producer)
Func &Func::isolate_producer(const vector<FuncOrExpr> &_fs, Func p) {
    // Convert all ImageParams to Funcs for the simplicity of processing.
    vector<Func_Expr> fs;
    convert_to_Func_Expr(_fs, fs);

    check_preconditions_of_isolate_producers(fs, *this, {p});
    invalidate_cache();

    // All the UREs, including this Func and all the merged ones.
    vector<Func> ures;
    all_ures(*this, ures);

    // For each Func or Expr in fs, the matched operand is the unique expression or Func call
    // that the programmer has specified to match. There might be a path to such a
    // matched operand, with a path condition, and may be with some Let Exprs on the path.
    // The correpsonding matched Expr is like Let(a, b, Let(c, d, Select(condition, matched operand))), etc.
    vector<Expr> matched_operands;
    vector<Expr> matched_exprs;
    // The Funcs among the UREs that the matched_exprs depend on.
    vector<Func> dependees;
    match_calls_and_find_dependees(fs, ures, matched_operands, matched_exprs, dependees);

    // Clone the dependees. Build a map from a dependee to its clone.
    vector<Func>                 clones;
    map<Func, Func, FuncCompare> originals_to_clones;
    clone_dependees(dependees, clones, originals_to_clones, p);

    // Define new Funcs for each matched_expr, e.g. g(...) = Select(cond, call to f).
    // and build a map from fs to the new Funcs: e.g. f to g.
    vector<Func> new_funcs_for_matched_exprs;
    define_funcs_for_matched_exprs(fs, *this, p, matched_exprs, new_funcs_for_matched_exprs);
    // Every Func or Expr to be matched should get matched.
    internal_assert(new_funcs_for_matched_exprs.size() == fs.size());

    // Clone the first new Func as p. Then merge all the other new Funcs into p.
    vector<Func> all_new_funcs;
    if (clones.size() == 0) {
        clone_func(p, p.name(), p.function().place(), new_funcs_for_matched_exprs[0]);
        new_funcs_for_matched_exprs[0] = p;
        all_new_funcs = new_funcs_for_matched_exprs;
    } else {
        clone_func(p, p.name(), p.function().place(), clones[0]);
        fix_map(originals_to_clones, clones[0], p);
        all_new_funcs = clones;
        all_new_funcs[0] = p;
        all_new_funcs.insert(all_new_funcs.end(),
                             new_funcs_for_matched_exprs.begin(), new_funcs_for_matched_exprs.end());
    }
    vector<Func> all_except_first_new_funcs;
    for (size_t i = 0; i < all_new_funcs.size(); i++) {
        // In the new Funcs, replace all the references to dependees with those to their clones
        substitute_calls(all_new_funcs[i], originals_to_clones);
        if (i > 0) {
            all_except_first_new_funcs.push_back(all_new_funcs[i]);
        }
    }
    p.merge_ures(all_except_first_new_funcs, true);

    // Copy this Func's schedule info to the new Funcs
    for (auto a : all_new_funcs) {
        a.function().definition().schedule().rvars() = func.definition().schedule().rvars();
        a.function().definition().schedule().splits() = func.definition().schedule().splits();
        a.function().definition().schedule().dims() = func.definition().schedule().dims();
        a.function().definition().schedule().transform_params() = func.definition().schedule().transform_params();
        a.function().arg_min_extents() = func.arg_min_extents();
        a.function().definition().schedule().is_input() = true;
        a.function().isolated_from_as_producer() = name();
        a.function().isolated_operands_as_producer() = matched_operands;
    }

    // In this Func, for every URE, replace the matched expressions with calls to the new Funcs.
    vector<Expr> substitute_args;
    for (auto a : this->args()) {
        substitute_args.push_back(a);
    }
    SubstituteOperands sub_opnd(matched_operands, new_funcs_for_matched_exprs, substitute_args);
    for (auto u : ures) {
        u.function().mutate(&sub_opnd);
    }
    // All the matched operands should have been replaced.
    internal_assert(sub_opnd.total_replaced == matched_operands.size());

    vector<Func> p_ures; // UREs of Func p
    all_ures(p, p_ures);
    debug(4) << "After isolating " << names_to_string(fs) << " to producer " << p.name() << ":\n"
             << to_string<Func>(p_ures, false) << "\n"
             << to_string<Func>(ures, false) << "\n";

    return *this;
}

Func &Func::isolate_producer_chain(const vector<FuncOrExpr> &_fs, const vector<Func> &producers) {
    // Convert all ImageParams to Funcs for the simplicity of processing.
    vector<Func_Expr> fs;
    convert_to_Func_Expr(_fs, fs);

    check_preconditions_of_isolate_producers(fs, *this, producers);
    invalidate_cache();

    // If producers are, e.g. {f, g, h}, that means a pipeline f->g->h->this Func. So we
    // isolate into h first. And then from h, isolate into {f, g}.
    Func p = producers.back();
    this->isolate_producer(_fs, p);
    internal_assert(p.defined()) << "Producer " << p.name()
        << " is undefined after isolating calls to Func(s) (" << names_to_string(fs) << "\n";

    if (producers.size() > 1) {
        vector<Func> remains(producers.begin(), producers.begin() + producers.size() - 1);
        p.isolate_producer_chain(_fs, remains);
    }
    return *this;
}

}
