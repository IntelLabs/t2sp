#include <vector>
#include <algorithm>

#include "../../Halide/src/Func.h"
#include "../../Halide/src/Function.h"
#include "../../Halide/src/IR.h"
#include "../../Halide/src/Simplify_Internal.h"
#include "Simplify.h"
#include "Substitute.h"
#include "IRMutator.h"
#include "./DebugPrint.h"
#include "./NoIfSimplify.h"
#include "Math.h"
#include "./Utilities.h"
#include "TriangularLoopOptimize.h"

namespace Halide {

using namespace Internal;

using std::vector;
using std::map;

Func &Func::triangular_loop_optimize(Var outer_loop, Var inner_loop, int safelen) {
    const auto &func_dims = func.definition().schedule().dims();
    // Check if the number of loops variables is greater than 2
    user_assert(func_dims.size() >= 2);
    // Check if both inner loop and outer loop are adjacent time loops and 
    // the inner-loop range depends on the outer loop variable
    size_t i;
    for (i = 0; i < func_dims.size() - 1; i++) {
        if (inner_loop.name() == func_dims[i].var && outer_loop.name() == func_dims[i + 1].var) {
            user_assert(func_dims[i].for_type == ForType::Serial) << "Loop " << func_dims[i].var << " is supposed to be a time loop! \n";
            user_assert(func_dims[i + 1].for_type == ForType::Serial) << "Loop " << func_dims[i + 1].var << " is supposed to be a time loop! \n";;
            break;
        }
    }
    user_assert(i < func_dims.size() - 1) << "Can not find triangular loop structure in func " << func.name() 
                                          << ". Please check the given loop variables. \n";
    user_assert(safelen >= 0);

    auto &params_vec = func.definition().schedule().triangular_loop_params();
    TriangularLoopParams params;
    params.inner_loop_name = inner_loop.name();
    params.outer_loop_name = outer_loop.name();
    params.safelen = safelen;
    params_vec.push_back(params);
    return *this;
}

/* Back-end lowering pass */

namespace Internal {
namespace {

/* For a triangular loop like this:
   
    for i = 0..I
        for j = i..J
            ...

   We will flatten the loop and generate code like this:

    i = 0
    j = 0
    #pragma ivdep safelen(M)
    for i_j = 0..loop_bound
        if (j >= i)
            ...
        j++
        if (j == J)
            i++
            j = Min(J-M, i)
 
*/

class TriLoopFlattening : public IRMutator {
    using IRMutator::visit;
public:
    TriLoopFlattening(const std::map<std::string, Function>& _env) : env(_env) {
        in_opt_function = false;
    }

private:
    const std::map<std::string, Function> &env;
    vector<TriangularLoopParams> param_vector;
    string loop_enclosing_triangular_loop;          // The loop that immediately encloses the triangular loop
    bool in_opt_function;                           // The current IR is in a function that contains a triangualr loop to be optimized.

    Stmt visit(const ProducerConsumer* op) override {
        Function func;
        if (op->is_producer && function_is_in_environment(op->name, env, func) &&
            func.definition().schedule().triangular_loop_params().size() > 0) {
            in_opt_function = true;
            param_vector = func.definition().schedule().triangular_loop_params();
            Stmt s = IRMutator::visit(op);
            in_opt_function = false;
            return s;
        }
        return IRMutator::visit(op);
    }

    Expr calculate_merged_loop_extent(string outer_loop, Expr outer_min, Expr outer_extent, 
                                      string inner_loop, Expr inner_min, Expr inner_extent,
                                      int safelen) {
        internal_assert(is_const(outer_min) && is_const(outer_extent));
        const IntImm *e_min = outer_min.as<IntImm>();
        const IntImm *e_extent = outer_extent.as<IntImm>();
        Expr new_extent = Expr(0);
        for (int i = 0; i < e_extent->value; i++) {
            int iter = e_min->value + i;
            Expr current_inner_min = simplify(substitute(outer_loop, iter, inner_min));
            Expr current_inner_extent = simplify(substitute(outer_loop, iter, inner_extent));
            internal_assert(is_const(current_inner_min) && is_const(current_inner_extent));
            if (current_inner_extent.as<IntImm>()->value < (int)safelen) {
                new_extent += safelen;
            } else {
                new_extent += current_inner_extent;
            }
        }
        return simplify(new_extent);
    }

    Stmt visit(const For *op) override {
        if (!in_opt_function) {
            return IRMutator::visit(op);
        }

        if (op->body.as<For>() && extract_last_token(op->body.as<For>()->name) == param_vector[0].outer_loop_name) {
            const For *outer_for = op->body.as<For>();
            const For *inner_for = op->body.as<For>()->body.as<For>();
            internal_assert(is_const(outer_for->min) && is_const(outer_for->extent));
            internal_assert(inner_for && extract_last_token(inner_for->name) == param_vector[0].inner_loop_name); 

            Stmt body = mutate(inner_for->body);

            // increment loop var at the end of the inner loop
            /*
             ...
             j++
             if (j == J)
                 i++
                 j = Min(J-M, i)
            */
            Expr inner_for_max = simplify(inner_for->min + inner_for->extent);
            string inner_loop_var_temp = param_vector[0].inner_loop_name + ".temp";
            string outer_loop_var_temp = param_vector[0].outer_loop_name + ".temp";
            Stmt inner_inc = Provide::make(inner_loop_var_temp, {Call::make(Int(32), inner_loop_var_temp, {}, Call::Intrinsic) + Expr(1)}, {});
            Stmt outer_inc = Provide::make(outer_loop_var_temp, {Call::make(Int(32), outer_loop_var_temp, {}, Call::Intrinsic) + Expr(1)}, {});
            Expr new_min = Min::make(inner_for->min, simplify(inner_for_max - param_vector[0].safelen));
            Stmt inner_assign = Provide::make(inner_loop_var_temp, {new_min}, {});
            outer_inc = Block::make(outer_inc, inner_assign);
            Expr max_val_cond = EQ::make(Call::make(Int(32), inner_loop_var_temp, {}, Call::Intrinsic), inner_for_max);
            outer_inc = IfThenElse::make(max_val_cond, outer_inc);
            body = Block::make(body, inner_inc);
            body = Block::make(body, outer_inc);

            // Replace each use of the original loop vars with the new vars.
            body = substitute(outer_for->name, Call::make(Int(32), outer_loop_var_temp, {}, Call::Intrinsic), body);
            body = substitute(inner_for->name, Call::make(Int(32), inner_loop_var_temp, {}, Call::Intrinsic), body);

            // build new for loop
            Expr new_extent = calculate_merged_loop_extent(outer_for->name, outer_for->min, outer_for->extent, 
                                                           inner_for->name, inner_for->min, inner_for->extent,
                                                           param_vector[0].safelen);

            internal_assert(is_const(new_extent)) << "The new loop extent must be a constant after merging loop " << outer_for->name 
                                                  << " and loop " << inner_for->name << ".\n";
            debug(2) << "New extent after merging triangular loop: " << new_extent << "\n";

            string merged_loop_name = remove_postfix(outer_for->name, extract_last_token(outer_for->name)) + param_vector[0].outer_loop_name + param_vector[0].inner_loop_name;
            /*
            #pragma ivdep safelen(M)
            for ...
            */
            std::vector<Expr> args = {StringImm::make("Pragma"), StringImm::make("ivdep"), IntImm::make(Int(32), param_vector[0].safelen)};
            Expr annotation = Call::make(Int(32), Call::annotate, args, Call::Intrinsic);
            body = For::make(merged_loop_name, Expr(0), new_extent, ForType::Serial, DeviceAPI::None, body);
            body = Block::make(Evaluate::make(annotation), body);
            // body = For::make(inner_for->name, inner_for->min, inner_for->extent, inner_for->for_type, inner_for->device_api, body);
            // body = For::make(outer_for->name, outer_for->min, outer_for->extent, outer_for->for_type, outer_for->device_api, body);


            // Allocate and initialize new loop vars before entering triangular loop. 
            /*
             int i
             int j
             i = 0
             j = 0
            */
            body = Block::make(Provide::make(inner_loop_var_temp, {substitute(outer_for->name, outer_for->min, inner_for->min)}, {}), body);
            body = Block::make(Provide::make(outer_loop_var_temp, {outer_for->min}, {}), body);
            body = Realize::make(inner_loop_var_temp, {Int(32)}, MemoryType::Auto, {}, const_true(), body);
            body = Realize::make(outer_loop_var_temp, {Int(32)}, MemoryType::Auto, {}, const_true(), body);

            return For::make(op->name, op->min, op->extent, op->for_type, op->device_api, body);
        }
        return IRMutator::visit(op);
    }

    // Stmt visit(const Realize* op) override {
    //     if (ends_with(op->name, ".channel") ||
    //         ends_with(op->name, ".mem_channel") ||
    //         ends_with(op->name, ".shreg") ||
    //         ends_with(op->name,".temp") ||
    //         ends_with(op->name,".ibuffer")) {
    //         return IRMutator::visit(op);
    //     } else {
    //         Stmt body = mutate(op->body);
    //         Region new_bounds = op->bounds;
    //         for (size_t i = 0; i < new_bounds.size(); i++) {
    //             if (!is_const(new_bounds[i].min)) {
    //                 new_bounds[i].min = simplify(Min::make(substitute(param_vector[0].outer_loop_name, new_bounds[i+1].min, new_bounds[i].min), 
    //                                                        substitute(param_vector[0].outer_loop_name, new_bounds[i+1].min + new_bounds[i+1].extent - 1, new_bounds[i].min)));
    //             }
    //             if (!is_const(new_bounds[i].extent)) {
    //                 new_bounds[i].extent = simplify(Max::make(substitute(param_vector[0].outer_loop_name, new_bounds[i+1].min, new_bounds[i].extent), 
    //                                                           substitute(param_vector[0].outer_loop_name, new_bounds[i+1].min + new_bounds[i+1].extent - 1, new_bounds[i].extent)));
    //             }
    //         }
    //         return Realize::make(op->name, op->types, op->memory_type, new_bounds, op->condition, body);
    //     }
    // }
};

/* This class insert dummy iterations and ivdep pragma in order to reduce scheduled II:
   The corresponding IR for the code pattern looks like this:

    i = 0
    j = 0
    #pragma ivdep safelen(M)
    for i_j = 0..loop_bound
        if (j >= i)
            ...
        j++
        if (j == J)
            i++
            j = Min(J-M, i)
 
*/

// class InsertDummyIteration : public IRMutator {
//     using IRMutator::visit;
// public:
//     InsertDummyIteration(const std::map<std::string, Function>& _env) : env(_env) {}

// private:
//     const std::map<std::string, Function> &env;
//     vector<TriangularLoopParams> param_vector;
//     string loop_enclosing_triangular_loop;          // The loop that immediately encloses the triangular loop
//     bool in_opt_function;                           // The current IR is in a function that contains a triangualr loop to be optimized.

//     Stmt visit(const ProducerConsumer* op) override {
//         Function func;
//         if (op->is_producer) {
//             in_opt_function = false;
//             loop_enclosing_triangular_loop.clear();
//             param_vector.clear();
//             if (function_is_in_environment(op->name, env, func) && 
//                 func.definition().schedule().triangular_loop_params().size() > 0) {
//                 in_opt_function = true;
//                 param_vector = func.definition().schedule().triangular_loop_params();
//             }
//         }
//         return IRMutator::visit(op);
//     }

//     Expr recalculate_loop_extent_after_inserting_dummy_iterations(string outer_loop, Expr outer_min, Expr outer_extent, 
//                                                                   string inner_loop, Expr inner_min, Expr inner_extent,
//                                                                   size_t safelen) {
//         internal_assert(is_const(outer_min) && is_const(outer_extent));
//         const IntImm *e_min = outer_min.as<IntImm>();
//         const IntImm *e_extent = outer_extent.as<IntImm>();
//         Expr new_extent = Expr(0);
//         for (int i = 0; i < e_extent->value; i++) {
//             int iter = e_min->value + i;
//             Expr current_inner_min = simplify(substitute(outer_loop, iter, inner_min));
//             Expr current_inner_extent = simplify(substitute(outer_loop, iter, inner_extent));
//             internal_assert(is_const(current_inner_min) && is_const(current_inner_extent));
//             if (current_inner_extent.as<IntImm>()->value < (int)safelen) {
//                 new_extent += Expr(safelen);
//             } else {
//                 new_extent += current_inner_extent;
//             }
//         }
//         return simplify(new_extent);
//     }

//     Stmt visit(const For *op) override {
//         if (!in_opt_function) {
//             return IRMutator::visit(op);
//         }

//         if (op->body.as<For>() && extract_last_token(op->body.as<For>()->name) == param_vector[0].outer_loop_name) {
//             const For *outer_for = op->body.as<For>();
//             const For *inner_for = op->body.as<For>()->body.as<For>();
//             internal_assert(is_const(outer_for->min) && is_const(outer_for->extent));
//             internal_assert(inner_for && extract_last_token(inner_for->name) == param_vector[0].inner_loop_name); 

//             Stmt body = mutate(inner_for->body);

//             // increment loop var temp at the end of the loop
//             /*
//              ...
//              j++;
//              if (j == J)
//                 i++
//                 j = i
//             */
//             string inner_loop_var_temp = param_vector[0].inner_loop_name + ".temp";
//             string outer_loop_var_temp = param_vector[0].outer_loop_name + ".temp";
//             Stmt inner_inc = Provide::make(inner_loop_var_temp, {Call::make(Int(32), inner_loop_var_temp, {}, Call::Intrinsic) + Expr(1)}, {});
//             Stmt outer_inc = Provide::make(outer_loop_var_temp, {Call::make(Int(32), outer_loop_var_temp, {}, Call::Intrinsic) + Expr(1)}, {});
//             outer_inc = Block::make(outer_inc, Provide::make(inner_loop_var_temp, {inner_for->min}, {}));
//             Expr max_val_cond = EQ::make(Call::make(Int(32), inner_loop_var_temp, {}, Call::Intrinsic), 
//                                          simplify(inner_for->min + inner_for->extent));
//             outer_inc = IfThenElse::make(max_val_cond, outer_inc);
//             body = Block::make(body, inner_inc);
//             body = Block::make(body, outer_inc);

//             // Replace each use of the original loop vars by the new vars.
//             body = substitute(outer_for->name, Call::make(Int(32), outer_loop_var_temp, {}, Call::Intrinsic), body);
//             body = substitute(inner_for->name, Call::make(Int(32), inner_loop_var_temp, {}, Call::Intrinsic), body);

//             // build new for loop
//             Expr new_extent = recalculate_loop_extent_after_inserting_dummy_iterations(outer_for->name, outer_for->min, outer_for->extent, 
//                                                                                        inner_for->name, inner_for->min, inner_for->extent,
//                                                                                        param_vector[0].safelen);

//             internal_assert(is_const(new_extent)) << "The new loop extent must be a constant after merging loop " << outer_for->name 
//                                                   << " and loop " << inner_for->name << ".\n";
//             debug(2) << "New extent after merging triangular loop: " << new_extent << "\n";

//             string merged_loop_name = remove_postfix(outer_for->name, extract_last_token(outer_for->name)) + param_vector[0].outer_loop_name + param_vector[0].inner_loop_name;
//             body = For::make(merged_loop_name, Expr(0), new_extent, ForType::Serial, DeviceAPI::None, body);
//             // body = For::make(inner_for->name, inner_for->min, inner_for->extent, inner_for->for_type, inner_for->device_api, body);
//             // body = For::make(outer_for->name, outer_for->min, outer_for->extent, outer_for->for_type, outer_for->device_api, body);


//             // Allocate and initialize new loop vars before entering triangular loop. 
//             /*
//              int i
//              int j
//              i = 0
//              j = 0
//             */
//             body = Block::make(Provide::make(inner_loop_var_temp, {0}, {}), body);
//             body = Block::make(Provide::make(outer_loop_var_temp, {0}, {}), body);
//             body = Realize::make(inner_loop_var_temp, {Int(32)}, MemoryType::Auto, {}, const_true(), body);
//             body = Realize::make(outer_loop_var_temp, {Int(32)}, MemoryType::Auto, {}, const_true(), body);

//             return For::make(op->name, op->min, op->extent, op->for_type, op->device_api, body);
//         }
//         return IRMutator::visit(op);
//     }

//     Stmt visit(const Realize* op) override {
//         if (ends_with(op->name, ".channel") ||
//             ends_with(op->name, ".mem_channel") ||
//             ends_with(op->name, ".shreg") ||
//             ends_with(op->name,".temp") ||
//             ends_with(op->name,".ibuffer")) {
//             return IRMutator::visit(op);
//         } else {
//             Stmt body = mutate(op->body);
//             Region new_bounds = op->bounds;
//             for (size_t i = 0; i < new_bounds.size(); i++) {
//                 if (!is_const(new_bounds[i].min)) {
//                     new_bounds[i].min = simplify(Min::make(substitute(param_vector[0].outer_loop_name, new_bounds[i+1].min, new_bounds[i].min), 
//                                                            substitute(param_vector[0].outer_loop_name, new_bounds[i+1].min + new_bounds[i+1].extent - 1, new_bounds[i].min)));
//                 }
//                 if (!is_const(new_bounds[i].extent)) {
//                     new_bounds[i].extent = simplify(Max::make(substitute(param_vector[0].outer_loop_name, new_bounds[i+1].min, new_bounds[i].extent), 
//                                                               substitute(param_vector[0].outer_loop_name, new_bounds[i+1].min + new_bounds[i+1].extent - 1, new_bounds[i].extent)));
//                 }
//             }
//             return Realize::make(op->name, op->types, op->memory_type, new_bounds, op->condition, body);
//         }
//     }
// };

} // namespace

Stmt flatten_tirangualr_loop_nest(Stmt s, const std::map<std::string, Function> &env) {
    // Directly merge two loops
    TriLoopFlattening tlf(env);
    return tlf.mutate(s);
}

} // namespace Internal
} // namespace Halide
