#include <map>
#include <queue>

#include "CSE.h"
#include "IREquality.h"
#include "IRMatch.h"
#include "IRMutator.h"
#include "IROperator.h"
#include "Scope.h"
#include "Simplify.h"
#include "../../t2s/src/Utilities.h"

namespace Halide {
namespace Internal {

using std::map;
using std::pair;
using std::string;
using std::vector;
using std::set;
using std::queue;

namespace {

// Some expressions are not worth lifting out into lets, even if they
// occur redundantly many times. They may also be illegal to lift out
// (e.g. calls with side-effects).
// This list should at least avoid lifting the same cases as that of the
// simplifier for lets, otherwise CSE and the simplifier will fight each
// other pointlessly.
bool should_extract(const Expr &e, bool lift_all) {
    if (is_const(e)) {
        return false;
    }

    if (e.as<Variable>()) {
        return false;
    }

    if (lift_all) {
        return true;
    }

    if (const Broadcast *a = e.as<Broadcast>()) {
        return should_extract(a->value, false);
    }

    if (const Cast *a = e.as<Cast>()) {
        return should_extract(a->value, false);
    }

    if (const Add *a = e.as<Add>()) {
        return !(is_const(a->a) || is_const(a->b));
    }

    if (const Sub *a = e.as<Sub>()) {
        return !(is_const(a->a) || is_const(a->b));
    }

    if (const Mul *a = e.as<Mul>()) {
        return !(is_const(a->a) || is_const(a->b));
    }

    if (const Div *a = e.as<Div>()) {
        return !(is_const(a->a) || is_const(a->b));
    }

    if (const Ramp *a = e.as<Ramp>()) {
        return !is_const(a->stride);
    }

    return true;
}

// A global-value-numbering of expressions. Returns canonical form of
// the Expr and writes out a global value numbering as a side-effect.
class GVN : public IRMutator {
public:
    struct Entry {
        Expr expr;
        int use_count = 0;
        // All consumer Exprs for which this is the last child Expr.
        map<ExprWithCompareCache, int> uses;
        Entry(const Expr &e)
            : expr(e) {
        }
    };
    vector<std::unique_ptr<Entry>> entries;

    map<Expr, int, ExprCompare> shallow_numbering, output_numbering;
    map<ExprWithCompareCache, int> leaves;

    int number = -1;

    IRCompareCache cache;

    GVN()
        : number(0), cache(8) {
    }

    Stmt mutate(const Stmt &s) override {
        internal_error << "Can't call GVN on a Stmt: " << s << "\n";
        return Stmt();
    }

    ExprWithCompareCache with_cache(Expr e) {
        return ExprWithCompareCache(e, &cache);
    }

    Expr mutate(const Expr &e) override {
        // Early out if we've already seen this exact Expr.
        {
            auto iter = shallow_numbering.find(e);
            if (iter != shallow_numbering.end()) {
                number = iter->second;
                return entries[number]->expr;
            }
        }

        // We haven't seen this exact Expr before. Rebuild it using
        // things already in the numbering.
        number = -1;
        Expr new_e = IRMutator::mutate(e);

        // 'number' is now set to the numbering for the last child of
        // this Expr (or -1 if there are no children). Next we see if
        // that child has an identical parent to this one.

        auto &use_map = number == -1 ? leaves : entries[number]->uses;
        auto p = use_map.emplace(with_cache(new_e), (int)entries.size());
        auto iter = p.first;
        bool novel = p.second;
        if (novel) {
            // This is a never-before-seen Expr
            number = (int)entries.size();
            iter->second = number;
            entries.emplace_back(new Entry(new_e));
        } else {
            // This child already has a syntactically-equal parent
            number = iter->second;
            new_e = entries[number]->expr;
        }

        // Memorize this numbering for the old and new forms of this Expr
        shallow_numbering[e] = number;
        output_numbering[new_e] = number;
        return new_e;
    }
};

/** Fill in the use counts in a global value numbering. */
class ComputeUseCounts : public IRGraphVisitor {
    GVN &gvn;
    bool lift_all;

public:
    ComputeUseCounts(GVN &g, bool l)
        : gvn(g), lift_all(l) {
    }

    using IRGraphVisitor::include;
    using IRGraphVisitor::visit;

    void include(const Expr &e) override {
        // If it's not the sort of thing we want to extract as a let,
        // just use the generic visitor to increment use counts for
        // the children.
        debug(4) << "Include: " << e
                 << "; should extract: " << should_extract(e, lift_all) << "\n";
        if (!should_extract(e, lift_all)) {
            e.accept(this);
            return;
        }

        // Find this thing's number.
        auto iter = gvn.output_numbering.find(e);
        if (iter != gvn.output_numbering.end()) {
            gvn.entries[iter->second]->use_count++;
        } else {
            internal_error << "Expr not in shallow numbering!\n";
        }

        // Visit the children if we haven't been here before.
        IRGraphVisitor::include(e);
    }
};

/** Rebuild an expression using a map of replacements. Works on graphs without exploding. */
class Replacer : public IRGraphMutator {
public:
    Replacer() = default;
    Replacer(const map<Expr, Expr, ExprCompare> &r)
        : IRGraphMutator() {    
        expr_replacements = r;
    }

    void erase(const Expr &e) {
        expr_replacements.erase(e);
    }
};

class RemoveLets : public IRGraphMutator {
    using IRGraphMutator::visit;

public:
    // By default, remove Lets unconditionally.
    RemoveLets() :
        remove_Lets(true), remove_LetStmts(false), funcs_only(false), serial_loop_only(false) {
        path_condition = const_true();
    }

    RemoveLets(bool remove_Lets, bool remove_LetStmts, bool funcs_only,
               bool serial_loop_only, const set<string> &funcs) :
        remove_Lets(remove_Lets), remove_LetStmts(remove_LetStmts), funcs_only(funcs_only),
        serial_loop_only(serial_loop_only), funcs(funcs) {
        in_replacable_func = false;
        in_serial_loop = false;
        path_condition = const_true();
    }

private:
    bool remove_Lets;      // Remove Lets
    bool remove_LetStmts;  // Remove LetStmts
    bool funcs_only;       // Remove Lets/LetStmts in the given funcs below.
    bool serial_loop_only; // Do removal only inside a serial loop.
    const set<string> funcs;

    bool in_replacable_func; // Inside the produce of a given func
    bool in_serial_loop;     // Currently inside a serial loop.
    Expr path_condition;     // Path condition to the current IR node.

    Scope<Expr> scope;       // Record Let/LetStmts' variables' values

    // Special case: Let/LetStmt x=select(cond, true_value, Undefined)
    // In this case, x is defined only when cond is true. This could happen due to isolation, or
    // CSE across statements. We need check that in the body, under the path condition of any
    // occurrence of x, cond must be true, and thus we can replace x with the true_value. For this
    // purpose, define the following scope to record cond. This scope must be pushed/popped
    // simultaneously with the above "scope".
    Scope<Expr> conditions;

private:
    class FindCallsNotToSubstitute : public IRVisitor {
    public:
        using IRVisitor::visit;
        void visit(const Call* op) override {
            if ((op->name == Call::get_intrinsic_name(Call::read_channel)) ||
                 op->name == Call::get_intrinsic_name(Call::read_channel_nb)) {
                result = true;
                return;
            }
            IRVisitor::visit(op);
        }
        bool result = false;
    };

    // Internal helper functions
    void update_path_condition(const Expr &new_condition) {
        if (equal(path_condition, const_true())) {
            path_condition = new_condition;
        } else {
            path_condition = path_condition && new_condition;
        }
    }

    // If the current path condition is true, cond must be true.
    bool condition_holds(const Expr &cond) {
        if (equal(cond, const_true())) {
            return true;
        }
        if (equal(cond, path_condition)) {
            return true;
        }
        // Check if path_condition == cond && any other constraint
        // Since cond is part of path_condition, if the latter is true, cond must be true
        /* map<string, Expr> result;
           Expr x = Variable::make(Bool(), "*");
           if (expr_match(cond && x, path_condition, result)) {
              return true;
         } */
        // Break the condition and path condition into conjunction terms. Test the former
        // is part of the latter.
        vector<Expr> cond_terms = break_logic_into_conjunction(cond);
        vector<Expr> path_cond_terms = break_logic_into_conjunction(path_condition);
        for (auto c : cond_terms) {
            bool matched = false;
            for (auto p : path_cond_terms) {
                auto broadcast_p = p.as<Broadcast>();
                Expr p_val = broadcast_p ?broadcast_p->value :p;
                if (equal(c, const_true()) || equal(c, p_val)) {
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                return false;
            }
        }
        return true;
    }

    void visit_Select_or_IfThenElse(bool is_select, const Select *select, const IfThenElse *if_then_else, Expr &new_select, Stmt &new_if_then_else) {
        Expr new_true_value, new_false_value;
        Stmt new_then_case, new_else_case;
        Expr prev_path_condition = path_condition;

        if (is_select) {
            update_path_condition(select->condition);
            new_true_value = mutate(select->true_value);
        } else {
            update_path_condition(if_then_else->condition);
            new_then_case = mutate(if_then_else->then_case);
        }
        path_condition = prev_path_condition;

        if (is_select && select->false_value.defined()) {
            update_path_condition(!select->condition);
            new_false_value = mutate(select->false_value);
        } else if (!is_select && if_then_else->else_case.defined()) {
            update_path_condition(!if_then_else->condition);
            new_else_case = mutate(if_then_else->else_case);
        }
        path_condition = prev_path_condition;

        if (is_select) {
            new_select = Select::make(mutate(select->condition), new_true_value, new_false_value);
        } else {
            new_if_then_else = IfThenElse::make(mutate(if_then_else->condition), new_then_case, new_else_case);
        }
    }

    void visit_Let_or_LetStmt(bool is_let, const Let *let, const LetStmt *letstmt,  Expr &new_let, Stmt &new_letstmt) {
        bool do_removal = true;
        if ((funcs_only && !in_replacable_func) || (serial_loop_only && !in_serial_loop)) {
            do_removal = false;
        } else if (is_let && !remove_Lets) {
            do_removal = false;
        } else if (!is_let && !remove_LetStmts) {
            do_removal = false;
        }

        if (do_removal) {
            FindCallsNotToSubstitute finder;
            if (is_let) {
                let->value.accept(&finder);
            } else {
                letstmt->value.accept(&finder);
            }
            if (finder.result) {
                do_removal = false;
            }
        }

        if (!do_removal) {
            // Continue mutation without pushing the variable into the scope, so it won't be replaced in the body
            if (is_let) {
                new_let = IRGraphMutator::visit(let);
            } else {
                new_letstmt = IRGraphMutator::visit(letstmt);
            }
            return;
        }

        const std::string &name = is_let ? let->name : letstmt->name;
        const Expr &value = is_let ? let->value : letstmt->value;

        // Special handling: Let/LetStmt x=select(cond, true_value, Undefined)
        Expr new_value;
        Expr new_value_condition;
        const Select *select = value.as<Select>();
        if (select != NULL && !select->false_value.defined()) {
            Expr old_condition = path_condition;
            update_path_condition(select->condition);
            new_value_condition = path_condition;
            new_value = mutate(select->true_value);
            path_condition = old_condition;
        } else {
            new_value_condition = path_condition;
            new_value = mutate(value);
        }

        // When we enter a let, we invalidate all cached mutations
        // with values that reference this var due to shadowing. When
        // we leave a let, we similarly invalidate any cached
        // mutations we learned on the inside that reference the var.

        // A blunt way to handle this is to temporarily invalidate
        // *all* mutations, so we never see the same Expr node
        // on the inside and outside of a Let
        decltype(expr_replacements) tmp;
        tmp.swap(expr_replacements);
        ScopedBinding<Expr> bind(scope, name, new_value);
        ScopedBinding<Expr> bind1(conditions, name, new_value_condition);
        if (is_let) {
            new_let = mutate(let->body);
        } else {
            new_letstmt = mutate(letstmt->body);
        }
        tmp.swap(expr_replacements);
    }

private:
    Expr visit(const Variable *op) override {
        if (scope.contains(op->name)) {
            Expr value = scope.get(op->name);
            Expr condition = conditions.get(op->name);
            // debug(4) << " Var: " << op->name << "\n"
            //         << "   condition: " << condition << "\n"
            //         << "   path condition: " << path_condition << "\n";
            internal_assert(condition_holds(condition));
            return value;
        }
        return op;
    }

    Expr visit(const Select *op) override {
        Expr new_select;
        Stmt new_if_then_else; // useless
        visit_Select_or_IfThenElse(true, op, NULL, new_select, new_if_then_else);
        return new_select;
    }

    Stmt visit(const IfThenElse *op) override {
        Expr new_select; // useless
        Stmt new_if_then_else;
        visit_Select_or_IfThenElse(false, NULL, op, new_select, new_if_then_else);
        return new_if_then_else;
    }

    Expr visit(const Let *op) override {
        Expr new_let;
        Stmt new_letstmt; // useless
        visit_Let_or_LetStmt(true, op, NULL, new_let, new_letstmt);
        return new_let;
    }

    Stmt visit(const LetStmt *op) override {
        Expr new_let;     // useless
        Stmt new_letstmt;
        visit_Let_or_LetStmt(false, NULL, op, new_let, new_letstmt);
        return new_letstmt;
    }

    Stmt visit(const ProducerConsumer* op) override{
        if(op->is_producer){
            if(!funcs_only || funcs.find(op->name) != funcs.end()){
                bool old_in_replacable_func = in_replacable_func;
                in_replacable_func = true;
                Stmt s = IRGraphMutator::visit(op);
                in_replacable_func = old_in_replacable_func;
                return s;
            }
        }
        return IRGraphMutator::visit(op);
    }

    Stmt visit(const For* op) override{
        bool old_in_serial_loop = in_serial_loop;
        if (op->for_type == ForType::Serial) {
            in_serial_loop = true;
        }
        Stmt s = IRGraphMutator::visit(op);
        in_serial_loop = old_in_serial_loop;
        return s;
    }
};

class CSEEveryExprInStmt : public IRMutator {
    bool lift_all;

public:
    using IRMutator::mutate;

    Expr mutate(const Expr &e) override {
        return common_subexpression_elimination(e, lift_all);
    }

    CSEEveryExprInStmt(bool l)
        : lift_all(l) {
    }
};

}  // namespace

Expr common_subexpression_elimination(const Expr &e_in, bool lift_all) {
    Expr e = e_in;

    // Early-out for trivial cases.
    if (is_const(e) || e.as<Variable>()) return e;

    debug(4) << "\n\n\nInput to CSE " << e << "\n";

    e = RemoveLets().mutate(e);

    debug(4) << "After removing lets: " << e << "\n";

    GVN gvn;
    e = gvn.mutate(e);

    ComputeUseCounts count_uses(gvn, lift_all);
    count_uses.include(e);

    debug(4) << "Canonical form without lets " << e << "\n";

    // Figure out which ones we'll pull out as lets and variables.
    vector<pair<string, Expr>> lets;
    vector<Expr> new_version(gvn.entries.size());
    map<Expr, Expr, ExprCompare> replacements;
    for (size_t i = 0; i < gvn.entries.size(); i++) {
        const auto &e = gvn.entries[i];
        if (e->use_count > 1) {
            string name = unique_name('t');
            lets.emplace_back(name, e->expr);
            // Point references to this expr to the variable instead.
            replacements[e->expr] = Variable::make(e->expr.type(), name);
        }
        debug(4) << i << ": " << e->expr << ", " << e->use_count << "\n";
    }

    // Rebuild the expr to include references to the variables:
    Replacer replacer(replacements);
    e = replacer.mutate(e);

    debug(4) << "With variables " << e << "\n";

    // Wrap the final expr in the lets.
    for (size_t i = lets.size(); i > 0; i--) {
        Expr value = lets[i - 1].second;
        // Drop this variable as an acceptible replacement for this expr.
        replacer.erase(value);
        // Use containing lets in the value.
        value = replacer.mutate(lets[i - 1].second);
        e = Let::make(lets[i - 1].first, value, e);
    }

    debug(4) << "With lets: " << e << "\n";

    return e;
}

Stmt common_subexpression_elimination(const Stmt &s, bool lift_all) {
    return CSEEveryExprInStmt(lift_all).mutate(s);
}

Stmt remove_lets(const Stmt &s, bool remove_Lets, bool remove_LetStmts, bool funcs_only,
                 bool serial_loop_only, const set<string> &funcs) {
    return RemoveLets(remove_Lets, remove_LetStmts, funcs_only, serial_loop_only, funcs).mutate(s);
}

Expr remove_lets(const Expr &e) {
    return RemoveLets(true, false, false, false, {}).mutate(e);
}

// Testing code.

namespace {

// Normalize all names in an expr so that expr compares can be done
// without worrying about mere name differences.
class NormalizeVarNames : public IRMutator {
    int counter;

    map<string, string> new_names;

    using IRMutator::visit;

    Expr visit(const Variable *var) override {
        map<string, string>::iterator iter = new_names.find(var->name);
        if (iter == new_names.end()) {
            return var;
        } else {
            return Variable::make(var->type, iter->second);
        }
    }

    Expr visit(const Let *let) override {
        string new_name = "t" + std::to_string(counter++);
        new_names[let->name] = new_name;
        Expr value = mutate(let->value);
        Expr body = mutate(let->body);
        return Let::make(new_name, value, body);
    }

public:
    NormalizeVarNames()
        : counter(0) {
    }
};

void check(Expr in, Expr correct) {
    Expr result = common_subexpression_elimination(in);
    NormalizeVarNames n;
    result = n.mutate(result);
    internal_assert(equal(result, correct))
        << "Incorrect CSE:\n"
        << in
        << "\nbecame:\n"
        << result
        << "\ninstead of:\n"
        << correct << "\n";
}

// Construct a nested block of lets. Variables of the form "tn" refer
// to expr n in the vector.
Expr ssa_block(vector<Expr> exprs) {
    Expr e = exprs.back();
    for (size_t i = exprs.size() - 1; i > 0; i--) {
        string name = "t" + std::to_string(i - 1);
        e = Let::make(name, exprs[i - 1], e);
    }
    return e;
}

}  // namespace

void cse_test() {
    Expr x = Variable::make(Int(32), "x");
    Expr y = Variable::make(Int(32), "y");

    Expr t[32], tf[32];
    for (int i = 0; i < 32; i++) {
        t[i] = Variable::make(Int(32), "t" + std::to_string(i));
        tf[i] = Variable::make(Float(32), "t" + std::to_string(i));
    }
    Expr e, correct;

    // This is fine as-is.
    e = ssa_block({sin(x), tf[0] * tf[0]});
    check(e, e);

    // Test a simple case.
    e = ((x * x + x) * (x * x + x)) + x * x;
    e += e;
    correct = ssa_block({x * x,               // x*x
                         t[0] + x,            // x*x + x
                         t[1] * t[1] + t[0],  // (x*x + x)*(x*x + x) + x*x
                         t[2] + t[2]});
    check(e, correct);

    // Check for idempotence (also checks a case with lets)
    check(correct, correct);

    // Check a case with redundant lets
    e = ssa_block({x * x,
                   x * x,
                   t[0] / t[1],
                   t[1] / t[1],
                   t[2] % t[3],
                   (t[4] + x * x) + x * x});
    correct = ssa_block({x * x,
                         t[0] / t[0],
                         (t[1] % t[1] + t[0]) + t[0]});
    check(e, correct);

    // Check a case with nested lets with shared subexpressions
    // between the lets, and repeated names.
    Expr e1 = ssa_block({x * x,                 // a = x*x
                         t[0] + x,              // b = a + x
                         t[1] * t[1] * t[0]});  // c = b * b * a
    Expr e2 = ssa_block({x * x,                 // a again
                         t[0] - x,              // d = a - x
                         t[1] * t[1] * t[0]});  // e = d * d * a
    e = ssa_block({e1 + x * x,                  // f = c + a
                   e1 + e2,                     // g = c + e
                   t[0] + t[0] * t[1]});        // h = f + f * g

    correct = ssa_block({x * x,                                        // t0 = a = x*x
                         t[0] + x,                                     // t1 = b = a + x     = t0 + x
                         t[1] * t[1] * t[0],                           // t2 = c = b * b * a = t1 * t1 * t0
                         t[2] + t[0],                                  // t3 = f = c + a     = t2 + t0
                         t[0] - x,                                     // t4 = d = a - x     = t0 - x
                         t[3] + t[3] * (t[2] + t[4] * t[4] * t[0])});  // h (with g substituted in)
    check(e, correct);

    // Test it scales OK.
    e = x;
    for (int i = 0; i < 100; i++) {
        e = e * e + e + i;
        e = e * e - e * i;
    }
    Expr result = common_subexpression_elimination(e);

    {
        Expr pred = x * x + y * y > 0;
        Expr index = select(x * x + y * y > 0, x * x + y * y + 2, x * x + y * y + 10);
        Expr load = Load::make(Int(32), "buf", index, Buffer<>(), Parameter(), const_true(), ModulusRemainder());
        Expr pred_load = Load::make(Int(32), "buf", index, Buffer<>(), Parameter(), pred, ModulusRemainder());
        e = select(x * y > 10, x * y + 2, x * y + 3 + load) + pred_load;

        Expr t2 = Variable::make(Bool(), "t2");
        Expr cse_load = Load::make(Int(32), "buf", t[3], Buffer<>(), Parameter(), const_true(), ModulusRemainder());
        Expr cse_pred_load = Load::make(Int(32), "buf", t[3], Buffer<>(), Parameter(), t2, ModulusRemainder());
        correct = ssa_block({x * y,
                             x * x + y * y,
                             t[1] > 0,
                             select(t2, t[1] + 2, t[1] + 10),
                             select(t[0] > 10, t[0] + 2, t[0] + 3 + cse_load) + cse_pred_load});

        check(e, correct);
    }

    {
        Expr pred = x * x + y * y > 0;
        Expr index = select(x * x + y * y > 0, x * x + y * y + 2, x * x + y * y + 10);
        Expr load = Load::make(Int(32), "buf", index, Buffer<>(), Parameter(), const_true(), ModulusRemainder());
        Expr pred_load = Load::make(Int(32), "buf", index, Buffer<>(), Parameter(), pred, ModulusRemainder());
        e = select(x * y > 10, x * y + 2, x * y + 3 + pred_load) + pred_load;

        Expr t2 = Variable::make(Bool(), "t2");
        Expr cse_load = Load::make(Int(32), "buf", select(t2, t[1] + 2, t[1] + 10), Buffer<>(), Parameter(), const_true(), ModulusRemainder());
        Expr cse_pred_load = Load::make(Int(32), "buf", select(t2, t[1] + 2, t[1] + 10), Buffer<>(), Parameter(), t2, ModulusRemainder());
        correct = ssa_block({x * y,
                             x * x + y * y,
                             t[1] > 0,
                             cse_pred_load,
                             select(t[0] > 10, t[0] + 2, t[0] + 3 + t[3]) + t[3]});

        check(e, correct);
    }

    {
        Expr halide_func = Call::make(Int(32), "dummy", {0}, Call::Halide);
        e = halide_func * halide_func;
        Expr t0 = Variable::make(halide_func.type(), "t0");
        // It's okay to CSE Halide call within an expr
        correct = Let::make("t0", halide_func, t0 * t0);
        check(e, correct);
    }

    debug(0) << "common_subexpression_elimination test passed\n";
}

}  // namespace Internal
}  // namespace Halide
