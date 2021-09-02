#include "UnrollLoops.h"
#include "CSE.h"
#include "IRMutator.h"
#include "IROperator.h"
#include "Simplify.h"
#include "Substitute.h"
#include "../../t2s/src/Utilities.h"

using std::pair;
using std::vector;

namespace Halide {
namespace Internal {

class UnrollLoops : public IRMutator {
    using IRMutator::visit;

    vector<pair<std::string, Expr>> lets;
    const std::map<std::string, Function> &env;
    bool in_device{false};

    Stmt visit(const ProducerConsumer* op) override {
        Function func;
        if (op->is_producer && function_is_in_environment(op->name, env, func) && func.place() == Place::Device) {
            in_device = true;
        } else {
            in_device = false;
        }
        return IRMutator::visit(op);
    }

    Stmt visit(const LetStmt *op) override {
        if (is_pure(op->value)) {
            lets.emplace_back(op->name, op->value);
            Stmt s = IRMutator::visit(op);
            lets.pop_back();
            return s;
        } else {
            return IRMutator::visit(op);
        }
    }

    Stmt visit(const For *for_loop) override {
        if (!in_device && for_loop->for_type == ForType::Unrolled) {
            // Give it one last chance to simplify to an int
            Expr extent = simplify(for_loop->extent);
            Stmt body = for_loop->body;
            const IntImm *e = extent.as<IntImm>();

            if (e == nullptr) {
                // We're about to hard fail. Get really aggressive
                // with the simplifier.
                for (auto it = lets.rbegin(); it != lets.rend(); it++) {
                    extent = Let::make(it->first, it->second, extent);
                }
                extent = remove_likelies(extent);
                extent = substitute_in_all_lets(extent);
                extent = simplify(extent);
                e = extent.as<IntImm>();
            }

            Expr extent_upper;
            bool use_guard = false;
            if (e == nullptr) {
                // Still no luck. Try taking an upper bound and
                // injecting an if statement around the body.
                extent_upper = find_constant_bound(extent, Direction::Upper, Scope<Interval>());
                if (extent_upper.defined()) {
                    e = extent_upper.as<IntImm>();
                    use_guard = true;
                }
            }

            if (e == nullptr && permit_failed_unroll) {
                // Still no luck, but we're allowed to fail. Rewrite
                // to a serial loop.
                user_warning << "HL_PERMIT_FAILED_UNROLL is allowing us to unroll a non-constant loop into a serial loop. Did you mean to do this?\n";
                body = mutate(body);
                return For::make(for_loop->name, for_loop->min, for_loop->extent,
                                 ForType::Serial, for_loop->device_api, std::move(body));
            }

            user_assert(e)
                << "Can only unroll for loops over a constant extent.\n"
                << "Loop over " << for_loop->name << " has extent " << extent << ".\n";
            // body = mutate(body);

            if (e->value == 1) {
                user_warning << "Warning: Unrolling a for loop of extent 1: " << for_loop->name << "\n";
            }

            Stmt iters;
            for (int i = e->value - 1; i >= 0; i--) {
                Stmt iter = simplify(substitute(for_loop->name, for_loop->min + i, body));
                // Mutate at this place, for unrolling of irregular loops
                iter = mutate(iter);
                if (!iters.defined()) {
                    iters = iter;
                } else {
                    iters = Block::make(iter, iters);
                }
                if (use_guard) {
                    iters = IfThenElse::make(likely_if_innermost(i < for_loop->extent), iters);
                }
            }

            return iters;

        } else {
            return IRMutator::visit(for_loop);
        }
    }
    bool permit_failed_unroll = false;

public:
    UnrollLoops(const std::map<std::string, Function> &env) : env(env) {
        // Experimental autoschedulers may want to unroll without
        // being totally confident the loop will indeed turn out
        // to be constant-sized. If this feature continues to be
        // important, we need to expose it in the scheduling
        // language somewhere, but how? For now we do something
        // ugly and expedient.

        // For the tracking issue to fix this, see
        // https://github.com/halide/Halide/issues/3479
        permit_failed_unroll = get_env_variable("HL_PERMIT_FAILED_UNROLL") == "1";
    }
};

namespace {
class LoopReplacer : public IRMutator {
  public:
    LoopReplacer(const std::map<std::string, Function> &env, ForType type) : env(env), for_type(type) {}

  private:
    const std::map<std::string, Function> &env;
    ForType for_type;
    bool in_device{false};

    using IRMutator::visit;

    Stmt visit(const ProducerConsumer* op) override {
        Function func;
        if (op->is_producer && function_is_in_environment(op->name, env, func) && func.place() == Place::Device) {
            in_device = true;
        } else {
            in_device = false;
        }
        return IRMutator::visit(op);
    }

    Stmt visit(const For* op) override {
        if (in_device && op->for_type == ForType::Unrolled) {
            Stmt body = mutate(op->body);
            return For::make(op->name, op->min, op->extent, for_type, op->device_api, body);
        } else {
            return IRMutator::visit(op);
        }
    }
};
}

Stmt unroll_loops(Stmt s, const std::map<std::string, Function> &env) {
    char *unroll = getenv("PRAGMAUNROLL");
    if (unroll != NULL) {
        LoopReplacer loop_replacer(env, ForType::PragmaUnrolled);
        s = loop_replacer.mutate(s);
    } else {
        /* If the extent of a space loop is too large, segfault might be caused during unrolling. 
        To avoid stack overflow, we define a env variable DELAYUNROLL and does not physically unroll
        loops in this phase. Loops are actually unrolled in codegen. */
        unroll = getenv("DELAYUNROLL");
        if (unroll != NULL) {
            LoopReplacer loop_replacer(env, ForType::DelayUnroll);
            s = loop_replacer.mutate(s);
        }
    }
    
    return UnrollLoops(env).mutate(s);
}

}  // namespace Internal
}  // namespace Halide
