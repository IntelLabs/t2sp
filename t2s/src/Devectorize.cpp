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
#include <vector>
#include <algorithm>
#include "../../Halide/src/IR.h"
#include "../../Halide/src/IRPrinter.h"
#include "../../Halide/src/IRMutator.h"
#include "../../Halide/src/IRVisitor.h"
#include "../../Halide/src/Simplify.h"
#include "../../Halide/src/Simplify_Internal.h"
#include "./DebugPrint.h"
#include "./Devectorize.h"

namespace Halide {
using namespace Internal;
namespace Internal {
namespace {
class CheckNoLoopInside : public IRVisitor {
    using IRVisitor::visit;
public:
    bool no_loop;
    CheckNoLoopInside() { no_loop = true; }

    void visit(const For *op) override {
        no_loop = false;
    }
};

// This class gathers statements in the first level of scope. It does not really mutate IR.
class GatherStmts : public IRMutator {
    using IRMutator::visit;
public:
    vector<Stmt> stmts;

    Expr mutate(const Expr &e) override {
        return e;
    }

    Stmt mutate(const Stmt &s) override {
        if (s.as<Block>()) {
            stmts.push_back(s.as<Block>()->first);
            mutate(s.as<Block>()->rest);
        } else if (s.as<Fork>() ) {
            stmts.push_back(s.as<Fork>()->first);
            mutate(s.as<Fork>()->rest);
        } else {
            stmts.push_back(s);
        }
        // We collect only the first level of scope, and do not go deeper into the statement.
        return s;
    }
};

// A read or write.
struct VariableAccess {
    string name;
    vector<Expr> args;
};

class FindWritesReads : public IRVisitor {
    using IRVisitor::visit;
public:
    vector<VariableAccess> writes;
    vector<VariableAccess> reads;

    FindWritesReads() {}

    void visit(const Call *op) override {
        if (op->is_intrinsic(Call::read_shift_reg)) {
            const string &name = op->args[0].as<StringImm>()->value;
            VariableAccess access = {name, vector<Expr>(op->args.begin() + 1, op->args.end())};
            reads.push_back(access);
        } else if (op->is_intrinsic(Call::write_shift_reg)) {
            const string &name = op->args[0].as<StringImm>()->value;
            VariableAccess access = {name, vector<Expr>(op->args.begin() + 1, op->args.end() - 1)};
            writes.push_back(access);
        } else if (ends_with(op->name, ".temp")) {
            VariableAccess access = {op->name, op->args};
            reads.push_back(access);
        } else if (!op->is_intrinsic()) {
            VariableAccess access = {op->name, op->args};
            reads.push_back(access);
        }
        IRVisitor::visit(op);
    }

    void visit(const Provide *op) override {
        VariableAccess access = {op->name, op->args};
        writes.push_back(access);
        IRVisitor::visit(op);
    }

    void visit(const Load *op) override {
        VariableAccess access = {op->name, {op->index}};
        reads.push_back(access);
        IRVisitor::visit(op);
    }

    void visit(const Store *op) override {
        VariableAccess access = {op->name, {op->index}};
        writes.push_back(access);
        IRVisitor::visit(op);
    }
};

class FindReference : public IRVisitor {
    using IRVisitor::visit;
private:
    const string &var;
public:
    bool found;

    FindReference(const string &var) : var(var) {found = false;}

    void visit(const Variable *op) override {
        if (op->name == var) {
            found = true;
        }
    }
};

bool refers_var(const Expr &e, const string &var) {
    FindReference finder(var);
    e.accept(&finder);
    return finder.found;
}

// Caculate distance between two accesses. Note that the distance may be incomplete if it is not a constant.
void calculate_distance_between_accesses(const vector<Expr> &source_args, const vector<Expr> &sink_args,
                                         vector<int> &distance, bool &is_constant_distance) {
    internal_assert(source_args.size() == sink_args.size());
    is_constant_distance = true;
    for (size_t k = 0; k < source_args.size(); k++) {
        Expr dk = simplify(source_args[k] - sink_args[k]);
        if (!dk.as<IntImm>()) {
            // Not a constant distance.
            is_constant_distance = false;
            return;
        }
        distance.push_back(dk.as<IntImm>()->value);
    }
}

// Is there any dependence from the write in an iteration of the vectorized loop to the read in some later iteration of
// the vectorized loop?
bool has_loop_carried_dependence(const string &vectorized_loop, const VariableAccess &w, const VariableAccess &r) {
    if (w.name != r.name) {
        return false;
    }
    if (w.args.size() != r.args.size()) {
        return false;
    }

    // If w does not refer the vectorized loop, then it is writing to the same location every iteration of the
    // vectorized loop. That is write-write dependence.
    bool vectorized_loop_referred_in_write = false;
    for (size_t i = 0; i < w.args.size(); i++) {
        if (refers_var(w.args[i], vectorized_loop)) {
            vectorized_loop_referred_in_write = true;
            break;
        }
    }
    if (!vectorized_loop_referred_in_write) {
        return true;
    }

    // Vectorized loop is referred in the write. We check for several special cases that we can safely handle. For
    // other cases, we will say there is a dependence, to be conservative.
    // Check if the args of the accesses are like this:
    //    (vectorized loop, outer args without referrence to the vectorized loop)
    // If so, the is no dependence from one iteration of the vectorized loop to the next.
    bool innermost_arg_is_vectorized_loop = false;
    bool outer_args_refer_vectorized_loop = false;
    for (size_t i = 0; i < w.args.size(); i++) {
        if (i == 0) {
            if (w.args[i].as<Variable>() && (w.args[i].as<Variable>()->name == vectorized_loop) &&
                r.args[i].as<Variable>() && (r.args[i].as<Variable>()->name == vectorized_loop)) {
                innermost_arg_is_vectorized_loop = true;
            }
        } else {
            if (refers_var(w.args[i], vectorized_loop) || refers_var(r.args[i], vectorized_loop)) {
                outer_args_refer_vectorized_loop = true;
                break;
            }
        }
    }
    if (innermost_arg_is_vectorized_loop && !outer_args_refer_vectorized_loop) {
        return false;
    }

    // Check if the args of the write and read access are like these, respectively:
    //    (vectorized loop,     outer args)
    //    (vectorized loop - x, outer args) where x is a positive constant.
    // and the outer args of the accesses are the same.
    // If so, the is a dependence from one iteration of the vectorized loop to the next x'th iteration.
    bool innermost_distance_is_positive = false;
    bool outer_args_equal = true;
    for (size_t i = 0; i < w.args.size(); i++) {
        if (i == 0) {
            Expr distance = simplify(w.args[i] - r.args[i]);
            if (is_positive_const(distance)) {
                innermost_distance_is_positive = true;
            }
        } else {
            if (!equal(w.args[i], r.args[i])) {
                outer_args_equal = false;
                break;
            }
        }
    }
    if (innermost_distance_is_positive && outer_args_equal) {
        return true;
    }

     // We cannot decide. To be conservative, assume there is dependence.
     return true;
}

bool has_loop_carried_dependence(const string &vectorized_loop, const vector<VariableAccess> &writes,
                                 const vector<VariableAccess> &reads) {
    for (const auto &w: writes) {
        for (const auto &r : reads) {
            if (has_loop_carried_dependence(vectorized_loop, w, r)) {
                return true;
            }
        }
    }
    return false;
}

class FindVectorCond : public IRVisitor {
    using IRVisitor::visit;
private:
    string vectorized_loop;
public:
    bool found;

    FindVectorCond(const string &s) 
        : vectorized_loop(s), found(false) { }

    void visit(const Select *op) override {
        if (refers_var(op->condition, vectorized_loop))
            found = true;
    }
};

// Build a data dependent graph between statements. Here we build only flow
// dependences from a statement to a previous statement, or from the LHS of
// a statement to its RHS. Each element of DDG: index of a statement that
// writes (to a location) to index of a statement that reads (from the same
// location except in a later iteration of the vectorized loop). That is,
// every dependence is a dependence carried by the vectorized loop.
void build_DDG(const string &vectorized_loop, const vector<Stmt> &stmts, vector<pair<size_t, size_t>> &DDG) {
    // For a statement, find all writes (i.e. LHS) and reads (i.e. RHS) in it.
    vector<pair<vector<VariableAccess>, vector<VariableAccess>>> LRHS;
    for (size_t i = 0; i < stmts.size(); i++) {
        FindWritesReads finder;
        stmts[i].accept(&finder);
        LRHS.push_back(pair<vector<VariableAccess>, vector<VariableAccess>>(finder.writes, finder.reads));
    }

    for (size_t i = 0; i < stmts.size(); i++) {
        FindVectorCond cvc(vectorized_loop);
        stmts[i].accept(&cvc);
        if (cvc.found)
            DDG.push_back(pair<size_t, size_t>(i, i));
    }

    // Check if there is flow dependence from a statement's LHS
    // to its RHS and any previous statement' RHS
    for (size_t i = 0; i < stmts.size(); i++) {
        const vector<VariableAccess> &writes = LRHS[i].first;
        for (size_t j = 0; j <= i; j++) {
            const vector<VariableAccess> &reads = LRHS[j].second;
            if (has_loop_carried_dependence(vectorized_loop, writes, reads)) {
                DDG.push_back(pair<size_t, size_t>(i, j));
            }
        }
    }

    debug(4) << "****************** Devectorization for loop " << vectorized_loop << "*********************\n";
    debug(4) << "Statements:\n";
    for (size_t i = 0; i < stmts.size(); i++) {
        debug(4) << "  Stmt " << i << ": " << stmts[i];
        const vector<VariableAccess> &writes = LRHS[i].first;
        const vector<VariableAccess> &reads = LRHS[i].second;
        debug(4) << "      Writes: ";
        for (auto &w : writes) {
            debug(4) << "\n       " << w.name << "(" << to_string<Expr>(w.args) << ")";
        }
        debug(4) << "\n      Reads: ";
        for (auto &r : reads) {
            debug(4) << "\n       " << r.name << "(" << to_string<Expr>(r.args) << ")";
        }
        debug(4) << "\n";
    }
    debug(4) << "Data dependence graph:\n";
    for (auto &d : DDG) {
        debug(4) << "  Stmt " << d.first << " --> Stmt " << d.second << "\n";
    }
}

void divide_stmts_into_groups(const vector<Stmt> &stmts, const vector<pair<size_t, size_t>> &DDG,
                              vector<tuple<size_t, size_t, bool>> &groups) {
    if (DDG.empty()) {
        // All statements are in the single group
        groups.push_back(tuple<size_t, size_t, bool>(0, stmts.size() - 1, false));
        return;
    }

    size_t start = 0; // The start of a new group
    size_t end = 0;   // The end of a new group
    while (start < stmts.size()) {
        // There are two cases: (1) the current range [start, end] does not
        // overlap with any range in the DDG. Extend end to the maximum value
        // right before the next range's start in the DDG. The resulting range
        // has no dependence among the statements in it. (2) the current
        // range starts before, and overlaps with, some range in the DDG.
        // Merge the two ranges together. The resulting range has dependence
        // among the statements in the range.
        bool has_dependence = false;
        for (size_t i = 0; i < DDG.size(); i++) {
            size_t min = DDG[i].second; // The index of the statement that reads
            size_t max = DDG[i].first; // The index of the statement that writes
            if ((start <= min && min <= end) && (min <= end && end <= max)) {
                // The current range [start, end] overlaps with the new range [min, max]. Merge them
                end = max;
                has_dependence = true;
            }
        }
        if (!has_dependence) {
            // Extend end to the maximum value right before the next range's start in the DDG.
            end = stmts.size() - 1;
            for (size_t i = 0; i < DDG.size(); i++) {
                size_t min = DDG[i].second; // The index of the statement that reads
                if (start < min && min <= end) {
                    end = min - 1;
                }
            }
        }
        groups.push_back(tuple<size_t, size_t, bool>(start, end, has_dependence));

        // Initialize for the next group:
        start = end + 1;
        end = end + 1;
    }
    debug(4) << "Groups:\n";
    for (size_t i = 0; i < groups.size(); i++) {
        const auto &g = groups[i];
        debug(4) << "  Group " << i << ": Stmt " << std::get<0>(g) << " ~ Stmt "
                << std::get<1>(g) << (std::get<2>(g) ? " Has dependence" : " No dependence") << "\n";
    }
}

Stmt make_a_stmt_for_group(const vector<Stmt> &stmts, const tuple<size_t, size_t, bool> &group, const For *op) {
    vector<Stmt> group_stmts;
    for (size_t i = std::get<0>(group); i <= std::get<1>(group); i++) {
        group_stmts.push_back(stmts[i]);
    }
    Stmt s;
    if (group_stmts.size() == 1) {
        s = group_stmts[0];
    } else {
        s = Block::make(group_stmts);
    }
    ForType new_for_type = std::get<2>(group) /*has self dependence*/ ? ForType::Unrolled : ForType::Vectorized;
    s = For::make(op->name, op->min, op->extent, new_for_type, op->device_api, s);
    return s;
}

class Devectorizer : public IRMutator {
    using IRMutator::visit;
private:
    string func_name; // The current Func
    string vectorized_loop; // The (only) vectorize loop of the current Func

public:
    Devectorizer() {}

    Stmt visit(const ProducerConsumer *op) override {
        if (op->is_producer) {
            func_name = op->name;
            vectorized_loop.clear();
        }
        return IRMutator::visit(op);
    }

    Stmt visit (const For *op) override {
        if (op->for_type != ForType::Vectorized) {
            // user_assert(vectorized_loop.empty()) << "Func " << func_name
            //         << " has a non-vectorized loop " << op->name << " under a vectorized loop: " << vectorized_loop
            //         << ". Currently the vectorized loop must be at the innermost level.\n";
            return IRMutator::visit(op);
        }
        // user_assert(vectorized_loop.empty()) << "Func " << func_name
        //         << " has more than 1 vectorized loop: " << vectorized_loop
        //         << " and " << op->name << ". Currently only 1 vectorized loop allowed in a loop nest.\n";
        vectorized_loop = op->name;

        // Check there is no loop below the vectorized loop
        CheckNoLoopInside checker;
        op->body.accept(&checker);
        user_assert(checker.no_loop) << "Func " << func_name
                << ": vectorized loop " << op->name << " expected to be at the innermost level.\n";

        // Collect all the statements under the vectorized loop
        GatherStmts gatherer;
        gatherer.mutate(op->body);

        // Build data dependence graph among the statements.
        vector<pair<size_t, size_t>> DDG;
        build_DDG(vectorized_loop, gatherer.stmts, DDG);

        // Divide the statements into groups so that there is no backward dependence
        // from a group to a group above it.
        vector<tuple<size_t, size_t, bool>> groups; // Each element: start and end index of the stmts
                                                    // in the same group, and whether the statements in
                                                    // the group have dependences among them.
        divide_stmts_into_groups(gatherer.stmts, DDG, groups);
        internal_assert(groups.size() >= 1);

        if (groups.size() == 1) {
            // Nothing to change.
            return op;
        } else {
            vector<Stmt> stmts;
            for (auto g : groups) {
                Stmt s = make_a_stmt_for_group(gatherer.stmts, g, op);
                stmts.push_back(s);
            }
            Stmt stmt = Block::make(stmts);
            return stmt;
        }
    }
};

};  // anonymous namespace

Stmt devectorize(Stmt s) {
    Devectorizer devectorizer;
    s = devectorizer.mutate(s);
    return s;
}

};  // namespace Internal
}  // namespce Halide
