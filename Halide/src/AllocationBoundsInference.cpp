#include "AllocationBoundsInference.h"
#include "Bounds.h"
#include "IRMutator.h"
#include "IROperator.h"
#include "Simplify.h"
#include "Substitute.h"

#include "../../t2s/src/Utilities.h"

namespace Halide {
namespace Internal {

using std::map;
using std::set;
using std::string;
using std::vector;

class GlobalBoundsCollector : public IRMutator {
    using IRMutator::visit;
public:
    std::map<std::string, Expr> global_min;
    std::map<std::string, Expr> global_max;
private:

    Stmt visit(const For *op) override {
        Expr min = op->min;
        Expr extent = op->extent;
        if (extent.as<IntImm>() == nullptr && !global_max.empty()) {
            Expr max_substitute = substitute(global_max, extent);
            Expr min_substitute = substitute(global_min, extent);
            extent = Max::make(max_substitute, min_substitute);
            max_substitute = substitute(global_max, min);
            min_substitute = substitute(global_min, min);
            min = Min::make(max_substitute, min_substitute);
        }
        global_max[extract_last_token(op->name)] = simplify(extent);
        global_min[extract_last_token(op->name)] = simplify(min);
        return IRMutator::visit(op);
    }
};

// Figure out the region touched of each buffer, and deposit them as
// let statements outside of each realize node, or at the top level if
// they're not internal allocations.

class AllocationInference : public IRMutator {
    using IRMutator::visit;

    const map<string, Function> &env;
    const FuncValueBounds &func_bounds;
    set<string> touched_by_extern;

    Stmt visit(const Realize *op) override {
        map<string, Function>::const_iterator iter = env.find(op->name);
        internal_assert(iter != env.end());
        Function f = iter->second;
        const vector<string> f_args = f.args();
        const std::map<std::string, std::pair<Expr, Expr>> &arg_min_extents = f.arg_min_extents();

        Scope<Interval> empty_scope;
        Box b = box_touched(op->body, op->name, empty_scope, func_bounds);
        if (touched_by_extern.count(f.name())) {
            // The region touched is at least the region required at this
            // loop level of the first stage (this is important for inputs
            // and outputs to extern stages).
            Box required(op->bounds.size());
            for (size_t i = 0; i < required.size(); i++) {
                string prefix = op->name + ".s0." + f_args[i];
                required[i] = Interval(Variable::make(Int(32), prefix + ".min"),
                                       Variable::make(Int(32), prefix + ".max"));
            }

            merge_boxes(b, required);
        }

        GlobalBoundsCollector collector;
        collector.mutate(op->body);

        Stmt new_body = mutate(op->body);
        Stmt stmt = Realize::make(op->name, op->types, op->memory_type, op->bounds, op->condition, new_body);

        internal_assert(b.size() == op->bounds.size()) << b.size() << " " << op->bounds.size() << "\n";

        for (size_t i = 0; i < b.size(); i++) {
            // Get any applicable bound on this dimension
            Bound bound;
            for (size_t j = 0; j < f.schedule().bounds().size(); j++) {
                Bound b = f.schedule().bounds()[j];
                if (f_args[i] == b.var) {
                    bound = b;
                }
            }

            string prefix = op->name + "." + f_args[i];
            string min_name = prefix + ".min_realized";
            string max_name = prefix + ".max_realized";
            string extent_name = prefix + ".extent_realized";
            if (!b[i].is_bounded()) {
                user_error << op->name << " is accessed over an unbounded domain in dimension "
                           << f_args[i] << "\n";
            }
            Expr min, max, extent;
            b[i].min = simplify(b[i].min);
            b[i].max = simplify(b[i].max);
            if (arg_min_extents.find(f_args[i]) != arg_min_extents.end()) {
                min = arg_min_extents.at(f_args[i]).first;
            } else if (bound.min.defined()) {
                min = bound.min;
            } else {
                min = b[i].min;
            }
            if (arg_min_extents.find(f_args[i]) != arg_min_extents.end()) {
                extent = arg_min_extents.at(f_args[i]).second;
                max = simplify(min +extent - 1);
            } else if (bound.extent.defined()) {
                extent = bound.extent;
                max = simplify(min + extent - 1);
            } else {
                max = b[i].max;
                extent = simplify((max - min) + 1);
            }
            if (min.as<IntImm>() == nullptr && !collector.global_max.empty()) {
                Expr max_substitute = substitute(collector.global_max, min);
                Expr min_substitute = substitute(collector.global_min, min);
                min = simplify(Min::make(max_substitute, min_substitute));
            }
            if (extent.as<IntImm>() == nullptr && !collector.global_max.empty()) {
                Expr max_substitute = substitute(collector.global_max, extent);
                Expr min_substitute = substitute(collector.global_min, extent);
                extent = simplify(Max::make(max_substitute, min_substitute));
            }
            if (bound.modulus.defined()) {
                internal_assert(bound.remainder.defined());
                min -= bound.remainder;
                min = (min / bound.modulus) * bound.modulus;
                min += bound.remainder;
                Expr max_plus_one = max + 1;
                max_plus_one -= bound.remainder;
                max_plus_one = ((max_plus_one + bound.modulus - 1) / bound.modulus) * bound.modulus;
                max_plus_one += bound.remainder;
                extent = simplify(max_plus_one - min);
                max = max_plus_one - 1;
            }

            Expr min_var = Variable::make(Int(32), min_name);
            Expr max_var = Variable::make(Int(32), max_name);

            internal_assert(min_var.type() == min.type());
            internal_assert(max_var.type() == max.type());

            Expr error_msg = Call::make(Int(32), "halide_error_explicit_bounds_too_small",
                                        {f_args[i], f.name(), min_var, max_var, b[i].min, b[i].max},
                                        Call::Extern);

            if (bound.min.defined()) {
                stmt = Block::make(AssertStmt::make(min_var <= b[i].min, error_msg), stmt);
            }
            if (bound.extent.defined()) {
                stmt = Block::make(AssertStmt::make(max_var >= b[i].max, error_msg), stmt);
            }

            stmt = LetStmt::make(extent_name, extent, stmt);
            stmt = LetStmt::make(min_name, min, stmt);
            stmt = LetStmt::make(max_name, max, stmt);
        }
        return stmt;
    }

public:
    AllocationInference(const map<string, Function> &e, const FuncValueBounds &fb)
        : env(e), func_bounds(fb) {
        // Figure out which buffers are touched by extern stages
        for (map<string, Function>::const_iterator iter = e.begin();
             iter != e.end(); ++iter) {
            Function f = iter->second;
            if (f.has_extern_definition()) {
                touched_by_extern.insert(f.name());
                for (size_t i = 0; i < f.extern_arguments().size(); i++) {
                    ExternFuncArgument arg = f.extern_arguments()[i];
                    if (!arg.is_func()) continue;
                    Function input(arg.func);
                    touched_by_extern.insert(input.name());
                }
            }
        }
    }
};

Stmt allocation_bounds_inference(Stmt s,
                                 const map<string, Function> &env,
                                 const FuncValueBounds &fb) {
    AllocationInference inf(env, fb);
    s = inf.mutate(s);
    return s;
}

}  // namespace Internal
}  // namespace Halide
