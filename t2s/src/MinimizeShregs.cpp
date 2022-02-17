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
#include "../../Halide/src/IRMutator.h"
#include "../../Halide/src/IRVisitor.h"
#include "../../Halide/src/Simplify.h"
#include "../../Halide/src/Substitute.h"
#include "./DebugPrint.h"
#include "./MinimizeShregs.h"
#include "./Utilities.h"
#include <algorithm>

namespace Halide {
using namespace Internal;
namespace Internal {
using std::map;
using std::pair;
using std::tuple;
using std::string;
using std::vector;

/* This is to minimize the shift registers for "unscheduled" space-time transform, for which currently a variable has
 * shift registers with bounds corresponds to the bounds of loops. That is, if there are 4 loops l0-l3 with extents of
 * N0-N3 starting from the innermost level, then a variable V that has shift registers assigned has been allocated the
*  registers like this:
 *   V.shreg[N0][N1][N2][N3] // Bounds of every loop starting from the innermost level
 * and any reference to V(l0, l1, l2, l3) will remain so. This is of course correct, but may be wasteful. Take GEMM as an
 * example. We have the following piece of specification:
 *   #define P             kkk,           jjj,     iii,     jj, ii, kk,          k,     j, i
 *   #define P_kkk_minus_1 kkk - 1,       jjj,     iii,     jj, ii, kk,          k,     j, i
 *   #define P_kk_minus_1  kkk + KKK - 1, jjj,     iii,     jj, ii, kk - 1,      k,     j, i
 *   #define P_k_minus_1   kkk + KKK - 1, jjj,     iii,     jj, ii, kk + KK - 1, k - 1, j, i
 *   C(P) = select(..., 0, select(..., select(..., C(P_k_minus_1), C(P_kk_minus_1)), C(P_kkk_minus_1))) ...
 * So there are 3 dependences for variable C:
 *   <1, 0, 0, 0, 0, 0, 0, 0, 0>, <-KKK + 1, 0, 0, 0, 0, 1, 0, 0, 0>, <-KKK + 1, 0, 0, 0, 0, -KK + 1, 1, 0, 0>
 * Currently, we have the following IR:
 *   Realize C.shreg[KKK][JJJ][III][JJ][II][KK][K][J][I] // Bounds of every loop starting from the innermost level
 *   for i, j, k, kk, ii, jj
 *     unroll for iii, jjj
 *        vectorized for kkk
 *           C[kkk][jjj][iii][jj][ii][kk][k][j][i] = select(..., 0, select(..., select(...,
 *              C[kkk + KKK - 1][jjj][iii][jj][ii][kk + KK - 1][k - 1][j][i],
 *              C[kkk + KKK - 1][jjj][iii][jj][ii][kk - 1][k][j][i]),
 *              C[kkk - 1][jjj][iii][jj][ii][kk][k][j][i])) ...
 * As we can see, there is no reuse of any register for two different values, even if they do not live simultaneously.
 * A better solution is to allocate registers and compute like this:
 *   Realize C.shreg[1][JJ * II][JJJ][III] // Bounds of [linearized k, kk, and kkk][linearized jj and ii][PE loops jjj and iii]
 *   for i, j, k, kk, ii, jj
 *     unroll for iii, jjj
 *        rotate C.shreg at the "linearized jj and ii" dimension, i.e.
   *           C.shreg[0][z][jjj][iii]= C.shreg[0][(z - 1) mod JJ * II][jjj][iii] for any z from JJ * II - 1 downto 0 (included)
 *        vectorized for kkk
 *          C.shreg[0][0][jjj][iii] = select(..., 0, select(..., select(...,
 *          C.shreg[0][0][jjj][iii],
 *          C.shreg[0][0][jjj][iii]),
 *          C.shreg[0][0][jjj][iii])) ...
 * Note (1) C(P), C(P_k_minus_1), C(P_kk_minus_1), and C(P_kkk_minus_1) all have been mapped to the same register
 *          C.shreg[0][0][jjj][iii]. The first 0 is for "linearized k, kk, and kkk". The second 0 is for
 *          "linearized jj and ii".  Basically, after rotation, 0 is the index of a dimension of current iteration. Then
 *          from the (linearized) distance, we can found the index of other iterations.
 *      (2) Looking from right, the shift registers put PE dimensions first, followed by linearized contiguous dimensions at
 *          which all dependence distances equal 0, and finally, linearized all the other dimensions. The PE loops are put the
 *          first, because we want shift registers to be located inside each PE separately.
 *      (3) Any linearized dimensions with an extent of 1 can actually be optimized away, although not shown.
 *
 * In general, given dependences of a variable V, we would re-allocate shift registers like this:
 *   V.shreg[vectorized dim if distances at it = 0][linearized time_dims]
 *          [linearized zero_dims_0][linearized zero_dims_1]...[linearized zero_dims_n][PE_dims]
 * The "PE_dims" are the unrolled loop(s) of space-time transform, but can be extended to include more loops (discussed below).
 * A "linearized zero_dims_*" is a group of linearized, contiguous, non-PE dims at which the distances of all the dependences
 * are 0. The "vectorized dim" is there only when all dependences' distances at the vectorized loop equals 0 (Otherwise, it
 * will be part of the "linearized time_dims" instead). All the other dimensions are linearized as a single
 * "linearized time_dims".
 *
 * For any "linearized zero_dims_*" or "linearized time_dims", shift/rotate the registers whenever its innermost dimension is
 * entered. Such a zero_dims just re-organizes the multi-dimensional registers into a linear space, because these multi-dim
 * registers are accessed dimension after dimension and each dimension is accessed linearly. So it is easier to manage them
 * as a linear space instead of multi-dimensions.
 * Why shifting registers for linearized time_dims? Let us say the linearized time_dims has two loops i (inner) and o (outer)
 * with extents of I and O, and there is a dependence whose distance is <di, do> at these two loops. That is, the current
 * iteration of the two loops, <i, j>, will access a previous value produced by a iteration <i-di, o-do>. Since the two loops
 * are linearized together as time, the previous value is in produced at time step (i - di) + (o - do) * I =
 * (i + o * I) - (di + do * I) = the time step of the current iteration <i, j> - the linearized distance. Therefore, if we let
 * the current iteration always writes into register 0, and shift registers every time step, the previous value will be in a
 * register whose number equals the linearized distance.
 *
 * For a variable, we may consider the following steps in implementation:
 * (1) PE_dims = unrolled loops.
 *     Although some unrolled loop(s) may be treated as part of time_dims for less registers, we'd better make them as PE_dims.
 *     For example, for the same GEMM example, if we make the innermost loop kkk as unrolled loop instead of vectorized loop,
 *     we'd better treat it as a PE dim. That is, allocate registers for this dimension separately with the loop extent being
 *     the dimension's size. Otherwise, suppose we allow loop kkk to be linearized with loop kk and k, just like the example
 *     above, we would achieve the minimum register requirement (The 3 dimensions together will require only 1 register);
 *     that would make any PE[kkk][jjj][iii] to share a single register with any other PE[kkk'][jjj][iii], where kkk' can be
 *     arbitrary. That violates what "PE" means. A "PE" is not supposed to share resources with other PEs. A PE is supposed to
 *     be a self-contained independent module. So we would stick to the concept that PE_dims are unrolled loops, even though
 *     that might lead to worse register pressure (in which case, programmers should improve their specifications, instead).
 * (2) vectorized dim: If every dependence has a distance of 0 at the vectorized loop, make a "vectorized dim". This adds a
 *     "space" dimension to V.shreg, just like other "space" dimensions ("linearized zero_dims*" and "PE_dims").
 * (3) zero_dims: all contiguous dims with distances always be 0 are grouped into a zero_dims. We need look at only dims that
 *     are inside the outermost dim whose distances are not always 0.
 * (4) time_dims: All the other dims except space dims (PE/vectorized/zero_dims).
 *     If the vectorized loop, if any, is included as part of the time_dims, the vectorized loop will be devectorized later,
 *     because an iteration of the vectorized loop depends on another iteration of the vectorized loop, i.e. the loop is not
 *     data parallel, and cannot be vectorized. However, the devectorization phase might still be able to find part of the loop
 *     to be vectorizable.
 */

namespace {
// A definition or calling of a function
struct Access {
    bool         define; // Definition/Call
    string       var;    // Name of the function defined or called
    vector<Expr> args;
    vector<Expr> args_without_prefix;
    Type         type;   // Data type
};

// Collect calls and definitions of funcs that have been assigned shift registers.
class RegAccessCollector : public IRVisitor {
    using IRVisitor::visit;
private:
    map<string, Expr> arg_map;                      // remove prefix for a real loop var. (e.g. A.s0.i --> i)
public:
    vector<Access> accesses;

    RegAccessCollector() {}

    void visit(const For* op) override {
        arg_map.insert({op->name, Variable::make(Int(32), extract_last_token(op->name))});
        IRVisitor::visit(op);
    }

    void visit(const Call* op) override {
        IRVisitor::visit(op);
        if (op->is_intrinsic(Call::write_shift_reg)) {
            const StringImm *v = op->args[0].as<StringImm>();
            internal_assert(v);
            string func_name = remove_postfix(v->value, ".shreg");
            vector<Expr> args = {op->args.begin() + 1, op->args.end() - 1};
            vector<Expr> args_without_prefix;
            for (auto arg : args) {
                args_without_prefix.push_back(substitute(arg_map, arg));
            }
            Access access = {true, func_name, args, args_without_prefix, op->type};
            accesses.push_back(access);
        } else if (op->is_intrinsic(Call::read_shift_reg)) {
            const StringImm *v = op->args[0].as<StringImm>();
            internal_assert(v);
            string func_name = remove_postfix(v->value, ".shreg");
            vector<Expr> args = {op->args.begin() + 1, op->args.end()};
            vector<Expr> args_without_prefix;
            for (auto arg : args) {
                args_without_prefix.push_back(substitute(arg_map, arg));
            }
            Access access = {false, func_name, args, args_without_prefix, op->type};
            accesses.push_back(access);
        }
    }
};

// A flow dependence, i.e. a write to a read.
struct FlowDependence {
    vector<int> distance; // Distance vector. The first/last element corresponds to the innermost/outermost loop.
    bool        is_up;    // True/false: the write is lexically below/above the read. True also when write is in
                          // the LHS and read is in the RHS of the same statement.
};

// All flow dependences for a variable.
struct FlowDependences {
    vector<Expr>           args;        // The args of the unique write access
    vector<Expr>           args_without_prefix;
    vector<FlowDependence> dependences; // All the dependences.
    Type                   type;        // The data type of an access.
};

vector<int> distance_between_accesses(const string &var, const vector<Expr> &source_args, const vector<Expr> &sink_args) {
    internal_assert(source_args.size() == sink_args.size());
    vector<int> distance;
    for (size_t k = 0; k < source_args.size(); k++) {
        // We must ensure the source access (a write) is in terms of the loop variables, not any other expressions.
        internal_assert(source_args[k].as<Variable>());
        // TODO: how to handdle the read op with a constant index? 
        Expr dk = is_const(sink_args[k]) ? Expr(0) : simplify(source_args[k] - sink_args[k]);
        user_assert(dk.as<IntImm>()) << "Dependence distance vector is not constant:\n"
                << "\tWrite: " << var << "(" << to_string<Expr>(source_args) << "\n"
                << "\tRead:  " << var << "(" << to_string<Expr>(sink_args) << "\n"
                << "\t" << k << "'th distance element: " << to_string(dk) << " is not constant\n";
        distance.push_back(dk.as<IntImm>()->value);
    }
    return std::move(distance);
}

// Collect dependence info to a map: Func name -> dependences.
// We take care of only flow dependences between write/read_shift_regs.
void collect_dependences_for_shift_regs(const Stmt s,
                                        map<string, FlowDependences> &func_to_deps) {
    // From the stmt, collect all the accesses (definitions and calls) of every func. These
    // accesses are in their lexical order in the IR.
    RegAccessCollector collector;
    s.accept(&collector);
    const vector<Access> &accesses = collector.accesses;

    // From the accesses, calculate dependence distances for every func.
    for (size_t i = 0; i < accesses.size(); i++) {
        const Access &ai = accesses[i];
        if (!ai.define) {
            continue;
        }
        for (size_t j = 0; j < accesses.size(); j++) {
            const Access &aj = accesses[j];
            internal_assert(j == i || !aj.define || aj.var != ai.var); // The write access must be unique for a single variable.
            if (j == i || aj.var != ai.var) {
                continue;
            }
            internal_assert(ai.args.size() == aj.args.size());
            internal_assert(ai.type == aj.type);
            vector<int> distance = distance_between_accesses(ai.var, ai.args_without_prefix, aj.args_without_prefix);
            bool up = (i > j);
            // Check the distance is valid (i.e. >0 if up, otherwise >= 0)
            bool valid = false;
            string error;
            for (int k = distance.size() - 1; k >= 0; k--) {
                if (distance[k] > 0) {
                    valid = true;
                    break;
                }
                if (distance[k] < 0) {
                    valid = false;
                    error = to_string(k) + "'th element = " + to_string(distance[k]) + " is negative";
                    break;
                }
                if (k > 0) {
                    continue;
                }
                // The entire distance vector is 0.
                if (up) {
                    valid = false;
                    error = " distance vector is 0 but read lexically appears before write";
                    break;
                } else {
                    valid = true;
                    break;
                }
            }
            user_assert(valid) << "Dependence distance vector is not valid:\n"
                    << "\tWrite:    " << ai.var << "(" << to_string<Expr>(ai.args) << "\n"
                    << "\tRead:     " << aj.var << "(" << to_string<Expr>(aj.args) << "\n"
                    << "\tDistance: " << to_string<int>(distance) << "\n"
                    << "\tError:    " << error << "\n";
            if (func_to_deps.find(ai.var) == func_to_deps.end()) {
                func_to_deps[ai.var] = {ai.args, ai.args_without_prefix, {{distance, up}}, ai.type};
            } else {
                func_to_deps[ai.var].dependences.push_back({distance, up});
            }
        }
    }
}

class LoopInfoCollector : public IRVisitor {
    using IRVisitor::visit;
  public:
    LoopInfoCollector(vector<Expr> &loop_vars, vector<Expr> &unrolled_loop_vars, Expr &vectorized_loop_var,
                      map<string, Expr> &loop_mins, map<string, Expr> &loop_extents, 
                      map<string, Expr> &global_min, map<string, Expr> &global_max) :
                      loop_vars(loop_vars), unrolled_loop_vars(unrolled_loop_vars), vectorized_loop_var(vectorized_loop_var),
                      loop_mins(loop_mins), loop_extents(loop_extents), global_min(global_min), global_max(global_max) {}

  private:
    vector<Expr>      &loop_vars;
    vector<Expr>      &unrolled_loop_vars;
    Expr              &vectorized_loop_var;
    map<string, Expr> &loop_mins;           // Loop name -> min
    map<string, Expr> &loop_extents;        // Loop name -> extent
    map<std::string, Expr> &global_min;
    map<std::string, Expr> &global_max;

    void visit(const For* op) override {
        Expr var = Variable::make(Int(32), op->name);
        loop_vars.push_back(var);
        if (op->for_type == ForType::Unrolled) {
            unrolled_loop_vars.push_back(var);
        }
        if (op->for_type == ForType::Vectorized) {
            internal_assert(!vectorized_loop_var.defined()); // Currently only one vectorized loop is allowed.
            vectorized_loop_var = var;
        }
        Expr loop_extent = op->extent;
        Expr loop_min = op->min;
        loop_mins[op->name] = loop_min;
        loop_extents[op->name] = loop_extent;
        
        if (loop_extent.as<IntImm>() == nullptr && !global_max.empty()) {
            Expr max_substitute = substitute(global_max, loop_extent);
            Expr min_substitute = substitute(global_min, loop_extent);
            loop_extent = Max::make(max_substitute, min_substitute);
            max_substitute = substitute(global_max, loop_min);
            min_substitute = substitute(global_min, loop_min);
            loop_min = Min::make(max_substitute, min_substitute);
        }
        global_max[op->name] = simplify(loop_min+loop_extent-1);
        global_min[op->name] = simplify(loop_min);
        op->body.accept(this);
    }
};

void collect_loop_info(Stmt s, vector<Expr>& loop_vars, vector<Expr> &unrolled_loop_vars, Expr &vectorized_loop_var,
                       map<string, Expr> &loop_mins, map<string, Expr> &loop_extents,
                       map<string, Expr> &global_min, map<string, Expr> &global_max) {
    LoopInfoCollector collector(loop_vars, unrolled_loop_vars, vectorized_loop_var, loop_mins, loop_extents, global_min, global_max);
    s.accept(&collector);
}

// Return the sub-vector corresponding to the indices if within is true,
// or corresponding to the other indices if within is false.
template<typename T>
vector<T> sub_vector(const vector<T> &v, const vector<int> &indices, bool within = true) {
    vector<T> sub;
    for (size_t i = 0; i < v.size(); i++) {
        if (std::find(indices.begin(), indices.end(), i) == indices.end()) {
            if (!within) {
                sub.push_back(v[i]);
            }
        } else {
            if (within) {
                sub.push_back(v[i]);
            }
        }
    }
    return std::move(sub);
}

// Expand a range (from, to) into a vector {from, from + 1, ..., to}
vector<int> range_to_vector(int from, int to) {
    vector<int> indices;
    for (int i = from; i <= to; i++) {
        indices.push_back(i);
    }
    return std::move(indices);
}

// Vector is positive, looking from the outermost.
bool vector_is_positive(const vector<int> &v) {
    internal_assert(!v.empty());
    for (int i = v.size() - 1; i >= 0; i--) {
        if (v[i] > 0) {
            return true;
        } else if (v[i] < 0) {
            return false;
        }
    }
    return false;
}

// Vector is zero.
bool vector_is_zero(const vector<int> &v) {
    internal_assert(!v.empty());
    for (int i = v.size() - 1; i >= 0; i--) {
       if (v[i] != 0) {
            return false;
        }
    }
    return true;
}

// Vector is negative, looking from the outermost.
bool vector_is_negative(const vector<int> &v) {
    internal_assert(!v.empty());
    for (int i = v.size() - 1; i >= 0; i--) {
       if (v[i] > 0) {
            return false;
       } else if (v[i] < 0) {
           return true;
      }
    }
    return false;
}

// Test if the var is one of the vars.
bool var_is_member(const Expr &var, const vector<Expr> &vars) {
    internal_assert(var.as<Variable>());
    for (auto &v : vars) {
      if (v.as<Variable>()->name == var.as<Variable>()->name) {
          return true;
      }
    }
    return false;
}

// Locate the loop vars in args.
vector<int> locate_loops(const vector<Expr> &args, const vector<Expr> &loop_vars) {
    vector<int> dims;
    for (size_t i = 0; i < args.size(); i++) {
        if (var_is_member(args[i], loop_vars)) {
            dims.push_back(i);
        }
    }
    return dims;
}

// Return the min of the loop indexed by dim.
Expr loop_min(const vector<Expr>      &args,
              const map<string, Expr> &loop_mins,
              int                      dim) {
    internal_assert(dim < (int)args.size());
    const Expr &arg = args[dim];

    internal_assert(arg.as<Variable>());
    const string &arg_name = arg.as<Variable>()->name;

    internal_assert(loop_mins.find(arg_name) != loop_mins.end());
    return loop_mins.at(arg_name);
}

// Return the extent of the loop indexed by dim.
Expr loop_extent(const vector<Expr>      &args,
                 const map<string, Expr> &loop_extents,
                 int                      dim) {
    internal_assert(dim < (int)args.size());
    const Expr &arg = args[dim];

    internal_assert(arg.as<Variable>());
    const string &arg_name = arg.as<Variable>()->name;

    internal_assert(loop_extents.find(arg_name) != loop_extents.end());
    return loop_extents.at(arg_name);
}

// The outermost loop at which some dependence distanceis not zero.
int outermost_non_zero_loop(const vector<FlowDependence> &dependences, const vector<Expr> &args) {
    for (int i = args.size() - 1; i >= 0; i--) {
        for (auto &d : dependences) {
            internal_assert(d.distance.size() == args.size());
            if (d.distance[i] != 0) {
                return i;
            }
        }
    }
    // all dependence distance vectors are 0.
    return -1;
}

bool distances_can_be_zero_at_dims(const vector<FlowDependence> &dependences, const vector<int> &dims) {
    for (auto d : dependences) {
        vector<int> sub_distance = sub_vector<int>(d.distance, dims);
        if (sub_distance.empty() || vector_is_zero(sub_distance)) {
            return true;
        }
    }
    return false;
}

bool distances_can_be_non_zero_at_dims(const vector<FlowDependence> &dependences, const vector<int> &dims) {
    for (auto d : dependences) {
        vector<int> sub_distance = sub_vector<int>(d.distance, dims);
        if (!sub_distance.empty() && !vector_is_zero(sub_distance)) {
            return true;
        }
    }
    return false;
}

// Find the first dimension at which all distances equal 0 from start (included) before the end.
// Return end if no such dimension is found.
int first_zero_dim(const vector<FlowDependence> &dependences, int start, int end) {
    for (int i = start; i < end; i++) {
        if (distances_can_be_zero_at_dims(dependences, {i}) && !distances_can_be_non_zero_at_dims(dependences, {i})) {
            return i;
        }
    }
    return end;
}

// Find the first dimension at which some distance(s) != 0 from start (included) before the end.
// Return end if no such dimension is found.
int first_non_zero_dim(const vector<FlowDependence> &dependences, int start, int end) {
    for (int i = start; i < end; i++) {
        if (distances_can_be_non_zero_at_dims(dependences, {i})) {
            return i;
        }
    }
    return end;
}

Expr linearized_extent(const vector<Expr>      &args,
                       const map<string, Expr> &loop_extents,
                       const vector<int>       &dims) {
    Expr extent = IntImm::make(Int(32), 1);
    for (auto d : dims) {
        const Expr &arg = args[d];
        const string &arg_name = arg.as<Variable>()->name;
        extent = simplify(extent * loop_extents.at(arg_name));
    }
    return extent;
}

// Find contiguous zero dimensions from start (included) before the end.
// Return the next dimension after the newly-found zero-dims, or end if
// there is no zero-dims found.
int make_one_group_of_zero_dims(const vector<FlowDependence> &dependences,
                                const map<string, Expr>       loop_extents,
                                int                           start,
                                int                           end,
                                ShiftRegAlloc                &alloc) {
    int next_start = first_zero_dim(dependences, start, end);
    if (next_start == end) {
        // No dimension is zero
        return end;
    }
    int next_end = first_non_zero_dim(dependences, next_start + 1, end);
    // All dimensions in the range of [next_start, next_end) are zeros.
    internal_assert(next_start <= next_end - 1);
    vector<int> dims = range_to_vector(next_start, next_end - 1);
    alloc.linearized_dims.push_back(dims);
    Expr extent = linearized_extent(alloc.args, loop_extents, dims);
    alloc.linearized_extents.push_back(extent);
    alloc.strategy.push_back(RegStrategy::Rotate); // Rotate registers for a zero_dims.
    return next_end;
}

int make_direct_access_of_zero_dims(const vector<FlowDependence> &dependences,
                                    const map<string, Expr>       loop_extents,
                                    int                           start,
                                    int                           end,
                                    ShiftRegAlloc                &alloc) {
    int next_start = first_zero_dim(dependences, start, end);
    if (next_start == end) {
        // No dimension is zero
        return end;
    }
    int next_end = next_start + 1;
    vector<int> dims = range_to_vector(next_start, next_end - 1);
    alloc.linearized_dims.push_back(dims);
    Expr extent = linearized_extent(alloc.args, loop_extents, dims);
    alloc.linearized_extents.push_back(extent);
    alloc.strategy.push_back(RegStrategy::DirectAccess);
    return next_end;
}

void make_zero_dims(const Function               &func,
                    const vector<FlowDependence> &dependences,
                    const vector<Expr>           &loop_vars,
                    const map<string, Expr>       loop_extents,
                    const map<string, Expr>      &global_min,
                    const map<string, Expr>      &global_max,
                    int                           outermost_non_zero_dim,
                    ShiftRegAlloc                &alloc) {
    map<string, Expr> new_extents(loop_extents);
    for (auto &kv : new_extents) {
        kv.second = simplify(Max::make(substitute(global_max, kv.second), substitute(global_min, kv.second)));
    }
    int last_PE_dim;
    if (!alloc.PE_dims.empty()) {
        last_PE_dim = alloc.PE_dims.back();
    } else {
        last_PE_dim = -1;
    }
    int i = last_PE_dim + 1;
    while (i < outermost_non_zero_dim) {
        auto &min_ext = func.arg_min_extents().at(func.args()[i]);
        if (!is_const(min_ext.first) || !is_const(min_ext.second)) {
            i = make_direct_access_of_zero_dims(dependences, new_extents, i, outermost_non_zero_dim, alloc);
        } else {
            i = make_one_group_of_zero_dims(dependences, loop_extents, i, outermost_non_zero_dim, alloc);
        }
    }
}

// Linearize a specific part of the distance vector.
int linearized_distance(const vector<Expr>      &args,
                        const vector<int>       &distance,
                        const vector<int>       &dims,         // The part of the distance to linearize.
                        const map<string, Expr> &loop_extents) {
    // Get the sub-distance vector. We require it to be non-negative.
    vector<int> sub_distance = sub_vector<int>(distance,  dims);
    internal_assert(!vector_is_negative(sub_distance));

    int lin_distance = 0;
    Expr prev_dim_extent = IntImm::make(Int(32), 1);
    for (auto i : dims) {
        internal_assert(prev_dim_extent.as<IntImm>());
        int coefficient = prev_dim_extent.as<IntImm>()->value;
        lin_distance += coefficient * distance[i];
        Expr extent = loop_extent(args, loop_extents, i);
        prev_dim_extent = simplify(prev_dim_extent * extent);
    }
    internal_assert(lin_distance >= 0);
    return lin_distance;
}

// Note: this function can be invoked only after zero_dims have been found.
void make_time_dims(const vector<FlowDependence> &dependences,
                    const map<string, Expr>      &loop_extents,
                    int                           outermost_non_zero_dim,
                    ShiftRegAlloc                &alloc) {
    vector<int> time_dims;
    for (int i = 0; i <= outermost_non_zero_dim; i++) {
        if (std::find(alloc.PE_dims.begin(), alloc.PE_dims.end(), i) != alloc.PE_dims.end()) {
            continue;
        }
        if (i == alloc.vectorized_dim && alloc.vectorized_dim_as_space) {
            continue;
        }
        bool in_zero_dim = false;
        for (auto &z : alloc.linearized_dims) {
            if (i >= z[0] && i <= z.back()) {
                in_zero_dim =  true;
                break;
            }
        }
        if (!in_zero_dim) {
            time_dims.push_back(i);
        }
    }
    if (time_dims.empty()) {
        return;
    }

    bool new_old_values_live_simultaneously = false;
    int max_linearized_time_distance = 0;
    Expr lin_time_extent;
    RegStrategy strategy;
    for (auto &d : dependences) {
        // Get the sub-distance vector
        vector<int> sub_distance = sub_vector<int>(d.distance, alloc.PE_dims);
        bool is_intra_PE_dependence = sub_distance.empty() || vector_is_zero(sub_distance);
        if (is_intra_PE_dependence) {
            // IR is like this:
            //    V(x) = ...
            //    ...  = V(x - 1) + V(x)
            // In this case, the new value V(x) and old value V(x - 1) are live simultaneously in an iteration of loop x.
            if (!d.is_up && vector_is_positive(d.distance)) {
                new_old_values_live_simultaneously = true;
            }
        } else {
            // This is an inter-PE dependence. At a time step, an old value in a PE1 is being read by another PE2, and PE1
            // is also writing a new value. Therefore, both new and old values live simultaneously in a PE.
            new_old_values_live_simultaneously = true;
        }

        int linearized_time_distance = linearized_distance(alloc.args, d.distance, time_dims, loop_extents);
        // For now, we require the linearized time distance to be non-negative.
        // TODO: remove this limitation in future. Just imagine the shift registers have two directions: toward left is for
        // a value with positive distance, toward right is for a value with negative distance. So the sum of the absolute
        // values of the positive and negative value is the total number of registers.
        internal_assert(linearized_time_distance >= 0);
        if (linearized_time_distance > max_linearized_time_distance) {
            max_linearized_time_distance = linearized_time_distance;
        }
    }
    if (new_old_values_live_simultaneously) {
        // The lifetimes like this, e.g.:
        //    V(x) = ...
        //    ...  = V(x - 1) + V(x)
        // The new value V(x) and old value V(x - 1) live simultaneously, and we need map
        // them to different registers. The code generation style should be like this:
        //    declare R[2];
        // At every time step, shift the registers
        //    R[1] = R[0];
        // and then
        //    R[0] = ...         // V(x) = ...
        //    ...  = R[1] + R[0] // ...  = V(x - 1) + V(x)
        lin_time_extent = IntImm::make(Int(32), max_linearized_time_distance + 1);
        strategy = RegStrategy::Shift;
    } else {
        // The lifetimes will be like this, e.g.:
        //    V(x) = V(x - 1) + V(x - 2)
        //    ...  = V(x)
        // In this case, the new value V(x) do not live simultaneously with any old value V(x - 1) or V(x - 2).
        // Instead of using 3 registers for them, we can use 2 registers with code generation like this:
        //    declare R[2];
        // At every time step, rotate the registers,
        //    tmp = R[1];
        //    R[1] = R[0];
        //    R[0] = tmp;
        // and then
        //    R[0] = R[1] + R[0] //  V(x) = V(x - 1) + V(x - 2)
        //    ...  = R[0]        //  ...  = V(x)
        lin_time_extent = IntImm::make(Int(32), std::max(max_linearized_time_distance, 1));
        strategy = RegStrategy::Rotate;
    }

    // Insert the time_dims into linearized dims according to the position of its innermost dimension.
    int innermost_time_dim = time_dims[0];
    int lin_time_position = alloc.linearized_dims.size();
    for (size_t i = 0; i < alloc.linearized_dims.size(); i++) {
        auto &z = alloc.linearized_dims[i];
        int innermost_zero_dim = z[0];
        internal_assert(innermost_time_dim != innermost_zero_dim);
        if (innermost_time_dim < innermost_zero_dim) {
            lin_time_position = i;
        }
    }
    internal_assert(lin_time_position <= (int)alloc.linearized_dims.size());
    alloc.linearized_dims.insert(alloc.linearized_dims.begin() + lin_time_position, time_dims);
    alloc.linearized_extents.insert(alloc.linearized_extents.begin() + lin_time_position, lin_time_extent);
    alloc.strategy.insert(alloc.strategy.begin() + lin_time_position, strategy);
}

void debug_print_alloc(const string &func_name, const ShiftRegAlloc &alloc) {
    debug(4) << "Register allocation for " << func_name << ":";
    if (alloc.vectorized_dim_as_space) {
        debug(4) << "\n\tVectorized dim: " << to_string(alloc.args[alloc.vectorized_dim]);
    }
    for (size_t i = 0; i < alloc.linearized_dims.size(); i++) {
        auto &z = alloc.linearized_dims[i];
        debug(4) << "\n\tLinearized dims: " << to_string<Expr>(sub_vector<Expr>(alloc.args, z))
                 << "\n\t\tLinearized extent: " << to_string(alloc.linearized_extents[i])
                 << "\n\t\tStrategy: " << (alloc.strategy[i] == RegStrategy::Rotate ? "rotate" : (alloc.strategy[i] == RegStrategy::Shift ? "shift" : "direct access"));
    }
    debug(4) << "\n\tPE_dims: " << to_string<Expr>(sub_vector<Expr>(alloc.args, alloc.PE_dims))
             << "\n\t\tExtents: " << to_string<Expr>(alloc.PE_extents) << "\n";
}

// Linearize args of the specific dimensions into a single arg.
Expr linearize_args(const vector<Expr>      &args,
                    const vector<int>       &dims,
                    const map<string, Expr> &loop_mins,
                    const map<string, Expr> &loop_extents) {
    Expr new_arg = IntImm::make(Int(32), 0);
    Expr prev_dim_extent = IntImm::make(Int(32), 1);
    for (size_t i = 0; i < dims.size(); i++) {
        Expr min = loop_min(args, loop_mins, dims[i]);
        Expr extent = loop_extent(args, loop_extents, dims[i]);
        new_arg = simplify(new_arg + (args[dims[i]] - min) * prev_dim_extent);
        prev_dim_extent = simplify(prev_dim_extent * extent);
    }
    return new_arg;
}

void decide_shift_reg_alloc_for_unscheduled_stt(const string            &func_name,
                                                const Function          &func,
                                                const FlowDependences   &dependences,
                                                const vector<Expr>      &loop_vars,
                                                const vector<Expr>      &unrolled_loop_vars,
                                                const Expr              &vectorized_loop_var,
                                                const map<string, Expr> &loop_mins,
                                                const map<string, Expr> &loop_extents,
                                                const map<string, Expr> &global_min,
                                                const map<string, Expr> &global_max,
                                                ShiftRegAlloc           &alloc) {
    // Check that the dependence args are all loop variables.
    for (auto &a : dependences.args) {
        internal_assert(var_is_member(a, loop_vars));
    }
    const vector<Expr>           &args = dependences.args;
    const vector<Expr>           &args_without_prefix = dependences.args_without_prefix;
    const vector<FlowDependence> &deps = dependences.dependences;
    const Type                    type = dependences.type;

    // Find unrolled dimensions from the args.
    vector<int> unrolled_dims = locate_loops(args, unrolled_loop_vars);

    // Make a decision
    alloc.type = type;
    alloc.args = args;
    alloc.args_without_prefix = args_without_prefix;
    alloc.PE_dims = unrolled_dims;
    alloc.vectorized_dim = -1;
    alloc.vectorized_dim_as_space = false;
    if (vectorized_loop_var.defined()) {
        vector<int> vectorized_dims = locate_loops(args, {vectorized_loop_var});
        // Vectorized loop is always at the innermost level (i.e. dim 0)
        internal_assert(vectorized_dims.size() == 1 && vectorized_dims[0] == 0);
        alloc.vectorized_dim = 0;
        alloc.vectorized_extent = loop_extent(args, loop_extents, alloc.vectorized_dim);
        if (!distances_can_be_non_zero_at_dims(deps, {0})) {
            alloc.vectorized_dim_as_space = true;
        }
    }
    int outermost_non_zero_dim = outermost_non_zero_loop(deps, args);
    make_zero_dims(func, deps, loop_vars, loop_extents, global_min, global_max, outermost_non_zero_dim, alloc);
    make_time_dims(deps, loop_extents, outermost_non_zero_dim, alloc);

    // Calculate extents of the dimensions, except vectorized_extent and linearized_extents,
    // whcih have already been calculated.
    for (auto i : alloc.PE_dims) {
        alloc.PE_extents.push_back(loop_extent(args, loop_extents, i));
    }

    debug_print_alloc(func_name, alloc);
}

// Map the args of an access to a variable (i.e. the args of a write/read_shift_reg()) to the new args according to the register
// allocation decision.
vector<Expr> map_args(const string            &func_name,
                      const vector<Expr>      &args_of_access,
                      const vector<int>       &distance,
                      const ShiftRegAlloc     &alloc,
                      const map<string, Expr> &loop_mins,
                      const map<string, Expr> &loop_extents) {
    vector<Expr> new_args;
    if (alloc.vectorized_dim_as_space) {
        new_args.push_back(simplify(alloc.args[alloc.vectorized_dim] - distance[alloc.vectorized_dim]));
    }
    for (size_t i = 0; i < alloc.linearized_dims.size(); i++) {
        auto &z = alloc.linearized_dims[i];
        int lin_distance = linearized_distance(alloc.args, distance, z, loop_extents);
        internal_assert(alloc.linearized_extents[i].as<IntImm>());
        int lin_extent = alloc.linearized_extents[i].as<IntImm>()->value;
        internal_assert(lin_distance >= 0 && lin_distance <= lin_extent);
        if (lin_distance == lin_extent) {
            internal_assert(alloc.strategy[i] == RegStrategy::Rotate);
            lin_distance = 0;
        }
        if (alloc.strategy[i] == RegStrategy::DirectAccess) {
            internal_assert(z.size() == 1);
            new_args.push_back(alloc.args[z[0]]);
        } else {
            Expr new_arg = IntImm::make(Int(32), lin_distance);
            new_args.push_back(new_arg);
        }
    }
    for (auto i : alloc.PE_dims) {
        // new_args.push_back(simplify(alloc.args[i] - distance[i]));
        new_args.push_back(simplify(args_of_access[i]));
    }
    return std::move(new_args);
}

// Return the new bounds of the shift registers according to the register allocation decision.
Region shift_regs_bounds(const map<string, Expr> &loop_mins,
                         const map<string, Expr> &loop_extents,
                         const ShiftRegAlloc     &alloc) {
    // In the new bounds, we will translate every zero_/time_dims' min to 0. We do not change the mins of
    // vectorized_/PE_dims, because these space dimensions' mins may be dynamic. For example, in LU,
    // the space loop j's min depends on its outer loop k.
    Expr zero = IntImm::make(Int(32), 0);

    Region new_bounds;
    if (alloc.vectorized_dim_as_space) {
        Expr min = loop_min(alloc.args, loop_mins, alloc.vectorized_dim);
        new_bounds.push_back({min, alloc.vectorized_extent});
    }
    for (auto extent : alloc.linearized_extents) {
        new_bounds.push_back({zero, extent});
    }
    for (size_t i = 0; i < alloc.PE_dims.size(); i++) {
        Expr min = loop_min(alloc.args, loop_mins, alloc.PE_dims[i]);
        Expr extent = alloc.PE_extents[i];
        new_bounds.push_back({min, extent});
    }
    return std::move(new_bounds);
}

// For linearized zero_dims_x or time_dims, make inner_args: [v][l0]...[l(x_minus_1)], or outer_args:
// [l(x_plus_1)][ln][PE_args], depending on if inner is true/false. Also find these args' extents
void get_new_args_and_extents(const map<string, Expr> &loop_extents,
                              const ShiftRegAlloc     &alloc,
                              int                      index,         // x
                              bool                     inner,         // True/false: inner/outer args
                              vector<Expr>            &new_args,
                              vector<Expr>            &new_extents) {
    Expr zero = IntImm::make(Int(32), 0);
    int start_lin_dim, end_lin_dim;
    if (inner) {
        start_lin_dim = 0;
        end_lin_dim = index;
    } else {
        start_lin_dim = index + 1;
        end_lin_dim = alloc.linearized_dims.size();
    }
    for (int i = start_lin_dim; i < end_lin_dim; i++) {
        if (equal(alloc.linearized_extents[i], 1)) {
            new_args.push_back(zero);
        } else {
            std::string l_loop_name = unique_name("dummy") + ".s0.l" + to_string(i);
            Expr l_loop_var = Variable::make(Int(32), l_loop_name);
            new_args.push_back(l_loop_var);
        }
        new_extents.push_back(alloc.linearized_extents[i]);
    }

    if (inner) {
        if (alloc.vectorized_dim_as_space) {
            if (equal(alloc.vectorized_extent, 1)) {
                new_args.push_back(zero);
            } else {
                std::string v_loop_name = unique_name("dummy") + ".s0.v";
                Expr v_loop_var = Variable::make(Int(32), v_loop_name);
                new_args.insert(new_args.begin(), v_loop_var);
            }
            new_extents.insert(new_extents.begin(), alloc.vectorized_extent);
        }
    } else {
        for (size_t i = 0; i < alloc.PE_dims.size(); i++) {
            int index = alloc.PE_dims[i];
            internal_assert(alloc.args[index].as<Variable>());
            const string &original_loop_name = alloc.args[index].as<Variable>()->name;
            std::string new_loop_name = unique_name("dummy") + ".s0." + extract_last_token(original_loop_name);
            new_args.push_back(Variable::make(Int(32), new_loop_name));
            new_extents.push_back(alloc.PE_extents[i]);
        }
    }
}

// If the indexed linearized_dims immediately encloses the given loop, insert shifting of the linearized_dims
// before the loop. For example, let us say we are working on "linearized_dims_x":
//   for (ln = 0; ln < Ln; ln++) // for "linearized_dims_n"
//     ...
//     for (l(x_plus_1) = 0; l(x_plus_1) < L(x_plus_1); l(x_plus_1)++) // for "linearized_dims_(x+1)"
//       for (l(x_minus_1) = 0; l(x_minus_1) < L(x_minus_1); l(x_minus_1)++) // for "linearized_dims_(x-1)"
//         ...
//         for (l0 = 0; l0 < L0; l0++)     // for "linearized_dims_0"
//             for (v = 0; v < V; v++)     // for "vectorized_dim", if vectorized_dim_as_space is true
//               unrolled for PE_args      // each PE shifts its registers independently below
//                     let inner_args = [v][l0]...[l(x_minus_1)]
//                     let outer_args = [l(x_plus_1)][ln][PE_args]
//                     ***tmp[PE_args] = V.shreg[inner_args][Lx - 1][outer_args]
//                        unrolled for (lx = 0; lx < Lx - 1; lx++) // for "linearized zero_dims_x"
//                            V.shreg[inner_args][Lx - 1 - lx][outer_args] = V.shreg[inner_args][Lx - 2 - lx][outer_args]
//                     ***V.shreg[inner_args][0][outer_args] = tmp[PE_args]
// Here outer_linearized_args = [arg for linearized_dims_(x+1)]...[arg for linearized_dims_n]
// ***: generated only when rotation is needed.
void shift_linearized_dim(const For                           *op,          // The loop to prepend the new code
                          const map<string, Expr>             &loop_mins,
                          const map<string, Expr>             &loop_extents,
                          const string                        &func_name,
                          const ShiftRegAlloc                 &alloc,
                          int                                  index,        // Index to alloc.linearized_dims
                          vector<tuple<string, Type, Region>> &temporaries,  // Temporaries for rotating registers.
                          Stmt                                &shifts) {
    const vector<Expr> &args = alloc.args;

    // Get the dimension of the given loop
    int current_dim = -1;
    for (size_t i = 0; i < args.size(); i++) {
        internal_assert(args[i].as<Variable>());
        if (op->name == args[i].as<Variable>()->name) {
            current_dim = i;
            break;
        }
    }
    if (current_dim < 0) {
        // The given loop is not a real computing loop. It might be some dummy loop like "f.s0.run_on_device", etc.
        return;
    }

    // Get "Lx" above
    Expr Lx = alloc.linearized_extents[index];
    int  innermost_dim_of_lx = alloc.linearized_dims[index][0];

    // Currently, if vectorized loop is part of the time_dims, we require the linearized time extent to be 1, so that
    // we do not have to shift registers at all for the time_dims. Otherwise, we need insert shifting inside the
    // vectorized loop, and also devectorize the loop, because the shifting means that every iteration of the vectorized
    // loop produces a new value, and that new value is to be used in a next iteration of the vectorized loop. In other
    // words, the vectorized loop is no longer data parallel.
    // TODO: remove this limitation, and allow shifting at the vectorized loop and devectorizing the loop.
    if (std::find(alloc.linearized_dims[index].begin(), alloc.linearized_dims[index].end(), alloc.vectorized_dim) !=
        alloc.linearized_dims[index].end()) {
        internal_assert(equal(Lx, 1));
    }

    if (equal(Lx, 1)) {
        // Nothing to do
        return;
    }

    if (current_dim + 1 != innermost_dim_of_lx) {
        // The zero/time_dims does not immediately enclose the current loop
        return;
    }

    // Get "[v][l0]..[l(x_minus_1)]" shown above, and these args' extents
    vector<Expr> inner_args, inner_extents;
    get_new_args_and_extents(loop_extents, alloc, index, true, inner_args, inner_extents);

    // Get "[outer_linearized_args][PE_args]" shown above
    vector<Expr> outer_args, outer_extents;
    get_new_args_and_extents(loop_extents, alloc, index, false, outer_args, outer_extents);

    // Get "lx"
    std::string lx_loop_name = unique_name("dummy") + ".s0.l" + to_string(index);
    Expr lx_loop_var = Variable::make(Int(32), lx_loop_name);

    // Make V.shreg[inner_args][Lx - 1 - lx][outer_args]
    vector<Expr> write_args = inner_args;
    write_args.push_back(simplify(Lx - 1 - lx_loop_var));
    write_args.insert(write_args.end(), outer_args.begin(), outer_args.end());
    write_args.insert(write_args.begin(), Expr(func_name + ".shreg"));

    // Make V.shreg[inner_args][Lx - 2 - lx][outer_args]
    vector<Expr> read_args = inner_args;
    read_args.push_back(simplify(Lx - 2 - lx_loop_var));
    read_args.insert(read_args.end(), outer_args.begin(), outer_args.end());
    read_args.insert(read_args.begin(), Expr(func_name + ".shreg"));

    // V.shreg[inner_args][Lx - 1 - lx][outer_args] = V.shreg[inner_args][Lx - 2 - lx][outer_args]
    Type type = alloc.type;
    Expr read = Call::make(type, Call::read_shift_reg, read_args, Call::Intrinsic);
    write_args.push_back(read);
    Stmt write = Evaluate::make(Call::make(type, Call::write_shift_reg, write_args, Call::Intrinsic));

    Stmt shift = For::make(lx_loop_name, IntImm::make(Int(32), 0), simplify(Lx - 1), ForType::Unrolled, DeviceAPI::None, write);

    // Rotate if needed.
    bool add_rotate = alloc.strategy[index] == RegStrategy::Rotate;
    if (add_rotate) {
        // Create a temporary
        string temp_name = unique_name(func_name) + ".temp";
        Expr temp_var = Variable::make(type, temp_name);
        Region temp_bounds;
        for (size_t i = 0; i < alloc.PE_dims.size(); i++) {
            int dim = alloc.PE_dims[i];
            Expr arg = alloc.args[dim];
            Expr min = loop_mins.at(arg.as<Variable>()->name);
            temp_bounds.push_back({min, alloc.PE_extents[i]});
        }
        temporaries.push_back(tuple<string, Type, Region>(temp_name, type, temp_bounds));

        // Make V.shreg[v][l0]..[l(x_minus_1)][0][outer_linearized_args][PE_args] = tmp[PE_args]
        vector<Expr> lowest_args = inner_args;
        lowest_args.push_back(0);
        lowest_args.insert(lowest_args.end(), outer_args.begin(), outer_args.end());
        vector<Expr> new_PE_args = {outer_args.begin() + outer_args.size() - alloc.PE_dims.size(), outer_args.end()};
        Expr read_temp =  Call::make(type, temp_name, new_PE_args, Call::Intrinsic);
        lowest_args.push_back(read_temp);
        lowest_args.insert(lowest_args.begin(), Expr(func_name + ".shreg"));
        Stmt write_lowest = Evaluate::make(Call::make(type, Call::write_shift_reg, lowest_args, Call::Intrinsic));
        shift = Block::make(shift, write_lowest);

        // tmp[PE_args] = V.shreg[v][l0]..[l(x_minus_1)][Lx - 1][outer_linearized_args][PE_args]
        vector<Expr> highest_args = inner_args;
        highest_args.push_back(simplify(Lx - 1));
        highest_args.insert(highest_args.end(), outer_args.begin(), outer_args.end());
        highest_args.insert(highest_args.begin(), Expr(func_name + ".shreg"));
        Expr read_highest =  Call::make(type, Call::read_shift_reg, highest_args, Call::Intrinsic);
        Stmt write_temp = Provide::make(temp_name, {read_highest}, new_PE_args);
        shift = Block::make(write_temp, shift);
    }

    // Add PE loops
    size_t start_of_PE_dims = outer_args.size() - alloc.PE_dims.size(); // Start position of PE_dims in outer_args
    for (size_t i = start_of_PE_dims; i < outer_args.size(); i++) {
        internal_assert(outer_args[i].as<Variable>());
        const string &new_loop_name = outer_args[i].as<Variable>()->name;
        const Expr &min = loop_min(alloc.args, loop_mins, alloc.PE_dims[i - start_of_PE_dims]);
        const Expr &extent = loop_extent(alloc.args, loop_extents, alloc.PE_dims[i - start_of_PE_dims]);
        shift = For::make(new_loop_name, min, extent, ForType::Unrolled, DeviceAPI::None, shift);
    }

    // Add loops for the other non-PE args: for ln...l(x_plus_1)l(x_minus_1), ..., l0, v
    vector<Expr> other_args = inner_args;
    other_args.insert(other_args.begin(), outer_args.begin(), outer_args.begin() + outer_args.size() - alloc.PE_dims.size());
    vector<Expr> other_extents = inner_extents;
    other_extents.insert(other_extents.begin(), outer_extents.begin(), outer_extents.begin() + outer_args.size() - alloc.PE_dims.size());
    for (size_t i = 0; i < other_args.size(); i++) {
        if (other_args[i].as<Variable>()) {
            shift = For::make(other_args[i].as<Variable>()->name, IntImm::make(Int(32), 0), other_extents[i],
                              ForType::Serial, DeviceAPI::None, shift);
        } else {
            // We have set it as a constant 0. This arg will be removed subsequently because the loop extent is 1.
            internal_assert(equal(other_args[i], 0));
        }
    }

    if (shift.defined()) {
        if (shifts.defined()) {
            shifts = Block::make(shifts, shift);
        } else {
            shifts = shift;
        }
    }
}

// If any linearized zero_dims_* or linearized time_dims immediately encloses the given loop, insert shifting of the lin_zero/time_dims
// before the loop. That is, shift registers for the lin_zero/time_dims when the lin_zero/time_dims is justed entered.
void shift_linearized_dims(const For                           *op,
                           const map<string, Expr>             &loop_mins,
                           const map<string, Expr>             &loop_extents,
                           const string                        &func_name,
                           const ShiftRegAlloc                 &alloc,
                           vector<tuple<string, Type, Region>> &temporaries,
                           Stmt                                &shifts) {
    for (size_t i = 0; i < alloc.linearized_dims.size(); i++) {
        if (alloc.strategy[i] != RegStrategy::DirectAccess) {
            shift_linearized_dim(op, loop_mins, loop_extents, func_name, alloc, i, temporaries, shifts);
        }
    }
}

class MinimizeShiftRegs : public IRMutator {
    using IRMutator::visit;
private:
    const map<string, Function> &env;

    // Loop info
    map<string, Expr> loop_mins;              // Loop name -> min
    map<string, Expr> loop_extents;           // Loop name -> extent
    map<std::string, Expr> global_min;
    map<std::string, Expr> global_max;
    vector<Expr>      unrolled_loop_vars;     // Unrolled loop vars, ordered from the innermost.
    Expr              vectorized_loop_var;

    // Temporaries for helping rotating registers.
    vector<tuple<string, Type, Region>> temporaries; // Every element: Temporary's name, type, bounds

public:
    // Func name --> register allocation decision of the func.
    map<string, ShiftRegAlloc> func_to_regalloc;
    map<string, Expr> arg_map;
    MinimizeShiftRegs(const map<string, Function> &env) : env(env) {}

    Stmt visit(const ProducerConsumer *op) override {
        Function stt_func;
        if (op->is_producer
            && function_is_in_environment(op->name, env, stt_func)
            && stt_func.definition().schedule().transform_params().size() > 0) {
            const auto &params = stt_func.definition().schedule().transform_params();
            if (!params[0].sch_vector_specified) {
                // This func has STT specified but without a scheduling vector.

                // Func name --> dependences of the func. Note that there might be multiple Func names found, because
                // multiple UREs can be merged into the stt_func.
                map<string, FlowDependences> func_to_deps;
                collect_dependences_for_shift_regs(op->body, func_to_deps);
                if (func_to_deps.empty()) {
                    return op;
                }

                // Loop info.
                vector<Expr> all_loop_vars, all_unrolled_loop_vars; // No order is enforced on these vars. Instead, we will
                                                                    // get order info based on the args of a dependence.
                collect_loop_info(op->body, all_loop_vars, all_unrolled_loop_vars, vectorized_loop_var, loop_mins, loop_extents, global_min, global_max);
                for (auto &entry: func_to_deps) {
                    const auto &deps = entry.second;
                    vector<int> unrolled_dims = locate_loops(deps.args, all_unrolled_loop_vars);
                    unrolled_loop_vars = sub_vector<Expr>(deps.args, unrolled_dims); // These vars are ordered.
                    if (vectorized_loop_var.defined()) {
                        vector<int> v_dims = locate_loops(deps.args, {vectorized_loop_var});
                        internal_assert(v_dims.size() == 1 && v_dims[0] == 0); // One vectorized loop allowed at the inermost level.
                    }
                    break;
                }

                // Decide how to minimize shift registers.
                for (auto &entry: func_to_deps) {
                    const string &func_name = entry.first;
                    const auto &deps = entry.second;
                    ShiftRegAlloc alloc;
                    Function func = env.at(func_name);
                    decide_shift_reg_alloc_for_unscheduled_stt(func_name, func, deps, all_loop_vars, all_unrolled_loop_vars, vectorized_loop_var, 
                                                               loop_mins, loop_extents, global_min, global_max, alloc);
                    func_to_regalloc[func_name] = std::move(alloc);
                }

                Stmt stmt = IRMutator::visit(op);

                for (auto &t : temporaries) {
                    stmt = Realize::make(std::get<0>(t), {std::get<1>(t)}, MemoryType::Auto, std::get<2>(t), const_true(), stmt);
                }
                return stmt;
            }
       }
        return IRMutator::visit(op);
    }

    Expr visit(const Call *op) override {
        if (op->is_intrinsic(Call::write_shift_reg)) {
            const StringImm *v = op->args[0].as<StringImm>();
            internal_assert(v);
            string func_name = remove_postfix(v->value, ".shreg");
            if (func_to_regalloc.find(func_name) == func_to_regalloc.end()) {
                return IRMutator::visit(op);
            }
            vector<Expr> args = {op->args.begin() + 1, op->args.end() - 1};
            vector<Expr> args_without_prefix;
            for (auto arg : args) {
                args_without_prefix.push_back(substitute(arg_map, arg));
            }
            vector<int> distance = distance_between_accesses(func_name, func_to_regalloc.at(func_name).args_without_prefix, args_without_prefix);
            vector<Expr> new_args = map_args(func_name, args, distance, func_to_regalloc.at(func_name), loop_mins, loop_extents);
            new_args.insert(new_args.begin(), op->args[0]);
            new_args.push_back(mutate(op->args.back()));
            Expr new_call = Call::make(op->type, Call::write_shift_reg, new_args, op->call_type, op->func, op->value_index,
                                       op->image, op->param);
            return new_call;
        } else if (op->is_intrinsic(Call::read_shift_reg)) {
            const StringImm *v = op->args[0].as<StringImm>();
            internal_assert(v);
            string func_name = remove_postfix(v->value, ".shreg");
            if (func_to_regalloc.find(func_name) == func_to_regalloc.end()) {
                return IRMutator::visit(op);
            }
            vector<Expr> args = {op->args.begin() + 1, op->args.end()};
            vector<Expr> args_without_prefix;
            for (auto arg : args) {
                args_without_prefix.push_back(substitute(arg_map, arg));
            }
            vector<int> distance = distance_between_accesses(func_name, func_to_regalloc.at(func_name).args_without_prefix, args_without_prefix);
            vector<Expr> new_args = map_args(func_name, args, distance, func_to_regalloc.at(func_name), loop_mins, loop_extents);
            new_args.insert(new_args.begin(), op->args[0]);
            Expr new_call = Call::make(op->type, Call::read_shift_reg, new_args, op->call_type, op->func, op->value_index,
                                       op->image, op->param);
            return new_call;
        }
        return IRMutator::visit(op);
    }

    Stmt visit(const For *op) override {
        arg_map.insert({op->name, Variable::make(Int(32), extract_last_token(op->name))});
        Stmt stmt = IRMutator::visit(op);

        // If a linearized_dims immediately encloses this loop, insert shifting of the linearized_dims before this loop.
        // That is, shift registers for the linearized_dims when the linearized_dims is justed entered.
        Stmt shifts;
        for (auto &entry : func_to_regalloc) {
            const string &func_name = entry.first;
            const ShiftRegAlloc &alloc = entry.second;
            shift_linearized_dims(op, loop_mins, loop_extents, func_name, alloc, temporaries, shifts);
        }
        if (shifts.defined()) {
            stmt = Block::make(shifts, stmt);
        }
        return stmt;
    }

    Stmt visit(const Realize *op) override {
        Stmt new_body = mutate(op->body);
        Region new_bounds = op->bounds;
        if (ends_with(op->name, ".shreg")) {
            string func_name = remove_postfix(op->name, ".shreg");
            if (func_to_regalloc.find(func_name) != func_to_regalloc.end()) {
                new_bounds = shift_regs_bounds(loop_mins, loop_extents, func_to_regalloc.at(func_name));
            }
        }
        Stmt new_realize = Realize::make(op->name, op->types, op->memory_type, new_bounds, op->condition, new_body);
        return new_realize;
    }
};

// Remove a bound of shift registers if the extent is 1. This is to make the code look cleaner.
class RemoveUnitBoundsOfShiftRegs : public IRMutator {
    using IRMutator::visit;
private:
    map<string, vector<int>> func_to_bounds_to_remove;

    // Remove the args without the given dimensions.
    template<typename T>
    vector<T> purge_dimensions(const vector<T> &v, const vector<int> &dims_to_remove) {
        vector<T> new_v;
        for (size_t i = 0; i < v.size(); i++) {
            if (std::find(dims_to_remove.begin(), dims_to_remove.end(), i) == dims_to_remove.end()) {
                new_v.push_back(v[i]);
            }
        }
        return std::move(new_v);
    }
public:
    RemoveUnitBoundsOfShiftRegs() {}

    Stmt visit(const Realize *op) override {
        if (ends_with(op->name, ".shreg")) {
            vector<int> bounds_to_remove;
            for (size_t i = 0; i < op->bounds.size(); i++) {
                auto &b = op->bounds[i];
                if (equal(b.extent, 1)) {
                    bounds_to_remove.push_back(i);
                }
            }
            if (!bounds_to_remove.empty()) {
                Region new_bounds = purge_dimensions<Range>(op->bounds, bounds_to_remove);
                string func_name = remove_postfix(op->name, ".shreg");
                func_to_bounds_to_remove[func_name] = std::move(bounds_to_remove);
                Stmt new_realize = Realize::make(op->name, op->types, op->memory_type, std::move(new_bounds), op->condition,
                                                 mutate(op->body));
                return new_realize;
            }
        }
        return IRMutator::visit(op);
    }

    Expr visit(const Call *op) override {
        if (op->is_intrinsic(Call::write_shift_reg)) {
            const StringImm *v = op->args[0].as<StringImm>();
            internal_assert(v);
            string func_name = remove_postfix(v->value, ".shreg");
            if (func_to_bounds_to_remove.find(func_name) == func_to_bounds_to_remove.end()) {
                return IRMutator::visit(op);
            }
            const auto &bounds_to_remove = func_to_bounds_to_remove.at(func_name);
            vector<Expr> args = {op->args.begin() + 1, op->args.end() - 1};
            vector<Expr> new_args = purge_dimensions<Expr>(args, bounds_to_remove);
            new_args.insert(new_args.begin(), op->args[0]);
            new_args.push_back(mutate(op->args.back()));
            Expr new_call = Call::make(op->type, Call::write_shift_reg, new_args, op->call_type, op->func, op->value_index,
                                       op->image, op->param);
            return new_call;
        } else if (op->is_intrinsic(Call::read_shift_reg)) {
            const StringImm *v = op->args[0].as<StringImm>();
            internal_assert(v);
            string func_name = remove_postfix(v->value, ".shreg");
            if (func_to_bounds_to_remove.find(func_name) == func_to_bounds_to_remove.end()) {
                return IRMutator::visit(op);
            }
            const auto &bounds_to_remove = func_to_bounds_to_remove.at(func_name);
            vector<Expr> args = {op->args.begin() + 1, op->args.end()};
            vector<Expr> new_args = purge_dimensions<Expr>(args, bounds_to_remove);
            new_args.insert(new_args.begin(), op->args[0]);
            Expr new_call = Call::make(op->type, Call::read_shift_reg, new_args, op->call_type, op->func, op->value_index,
                                       op->image, op->param);
            return new_call;
        }
        return IRMutator::visit(op);
    }
};
} // Namespace

Stmt minimize_shift_registers(Stmt s, const map<string, Function> &env, map<string, ShiftRegAlloc> &func_to_regalloc) {
    MinimizeShiftRegs msr(env);
    s = msr.mutate(s);
    func_to_regalloc = msr.func_to_regalloc;
    s = RemoveUnitBoundsOfShiftRegs().mutate(s);
    return s;
}

} // Namespace Internal
} // Namespace Halide

