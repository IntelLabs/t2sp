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

#include "../../Halide/src/Func.h"
#include "../../Halide/src/Function.h"
#include "./DebugPrint.h"
#include "./CheckFuncConstraints.h"

namespace Halide {

using namespace Internal;

using std::vector;
using std::map;
using std::reverse;

struct FuncCompare
{
   bool operator() (const Func& lhs, const Func& rhs) const
   {
       return lhs.name() < rhs.name();
   }
};

// Clone the dependee. Build a map from a dependee to its clone.
void clone_dependee(const Func &dependee,
                    Func &clone,
                    map<Func, Func, FuncCompare> &originals_to_clones) {
    map<FunctionPtr, FunctionPtr> empty; // No existing clone.
    dependee.function().deep_copy(dependee.name() + "_clone", clone.function().get_contents(), empty);
    originals_to_clones[dependee] = clone;
}

// In Func f, substitute every call to a Func in the map to a call to
// its mapped Func.
void substitute_calls_for_merge(Func &f,
                      const map<Func, Func, FuncCompare> &old_to_new) {
    for (auto element : old_to_new) {
        Func original = element.first;
        Func substitute = element.second;
        f.function().substitute_calls(original.function(), substitute.function());
    }
}

Func &Func::merge_ures(std::vector<Func> &ures, bool isolate) {
    if (ures.size() == 0) {
        return *this;
    }

    Var innermost_loop = this->args()[0];

    for (auto it : ures) {
        it.compute_root();
    }
    this->compute_root();

    for (auto it = ures.rbegin(); it != ures.rend() - 1; it++) {
        // Use compute_with iteratively to achieve merge_ure
        it->compute_with(*(it + 1), innermost_loop);
    }

    Func& first = ures[0];
    vector<Func>& merge_ures_vector = func.definition().schedule().merged_ures();
    if (func.has_merged_defs()) {
        // Already call merge_ures() in the past
        Func& last = merge_ures_vector.back();
        first.compute_with(last, innermost_loop);
    } else {
        first.compute_with(*this, innermost_loop);
    }
   
    for (auto f : ures) {
        merge_ures_vector.push_back(f);
        f.func.definition().schedule().is_merged() = true;
    }

    // Set the last URE to be the output
    for (auto f : merge_ures_vector) {
        f.func.definition().schedule().is_output() = false;
    }
    ures.back().func.definition().schedule().is_output() = true;

    const vector<Dim> &output_dims = ures.back().func.definition().schedule().dims();
    const vector<Dim> &all_dims = func.definition().schedule().dims();
    // Exetended ure syntax
    if (output_dims.size() != all_dims.size()) {
        ures.back().function().definition().schedule().is_extended_ure() = true;
    }
    if (!isolate)
        CheckFuncConstraints::check_merge_ures(*this, ures);

    debug(4) << "After merging " << this->name() << " with " << names_to_string(ures)   << ":\n"
             << to_string(*this, false, true) << "\n";

    return *this;
}

Func &Func::merge_ures(std::vector<Func> ures, std::vector<Func> output_ures, bool isolate) {
    if (ures.size() == 0 && output_ures.size() == 0) {
        return *this;
    }

    Var innermost_loop = this->args()[0];

    for (auto it : ures) {
        it.compute_root();
    }
    for (auto it : output_ures) {
        it.compute_root();
    }
    this->compute_root();

    for (auto it = output_ures.rbegin(); it != output_ures.rend(); it++) {
        // Use compute_with iteratively to achieve merge_ure
        it->compute_with(*(ures.rbegin()), innermost_loop);
    }
    for (auto it = ures.rbegin(); it != ures.rend() - 1; it++) {
        // Use compute_with iteratively to achieve merge_ure
        it->compute_with(*(it + 1), innermost_loop);
    }

    Func& first = ures[0];
    vector<Func>& merge_ures_vector = func.definition().schedule().merged_ures();
    if (func.has_merged_defs()) {
        // Already call merge_ures() in the past
        Func& last = merge_ures_vector.back();
        first.compute_with(last, innermost_loop);
    } else {
        first.compute_with(*this, innermost_loop);
    }
   
    for (auto f : ures) {
        merge_ures_vector.push_back(f);
        f.func.definition().schedule().is_merged() = true;
    }
    for (auto f : output_ures) {
        merge_ures_vector.push_back(f);
        f.func.definition().schedule().is_merged() = true;
    }

    for (auto f : merge_ures_vector) {
        f.func.definition().schedule().is_output() = false;
    }
    const vector<Dim> &all_dims = func.definition().schedule().dims();
    for (auto f : output_ures) {
        f.func.definition().schedule().is_output() = true;
        const vector<Dim> &output_dims = f.func.definition().schedule().dims();
        // Exetended ure syntax
        if (output_dims.size() != all_dims.size()) {
            f.function().definition().schedule().is_extended_ure() = true;
        }
    }

    if (!isolate)
        CheckFuncConstraints::check_merge_ures(*this, merge_ures_vector);

    debug(4) << "After merging " << this->name() << " with " << names_to_string(ures)   << ":\n"
             << to_string(*this, false, true) << "\n";

    return *this;
}

Func &Func::set_bounds(const std::vector<Var> &vars, const std::vector<Expr> &mins, const std::vector<Expr> &extents) {
    user_assert(vars.size() == mins.size()) << "Number of variables (" << vars.size()
            << ") not equal to number of minimums (" << mins.size() << ").";
    user_assert(vars.size() == extents.size()) << "Number of variables (" << vars.size()
            << ") not equal to number of extents (" << extents.size() << ").";
    std::vector<string> var_names;
    for (auto &v : vars) {
        var_names.push_back(v.name());
    }
    func.set_bounds(var_names, mins, extents);
    return *this;
}

}
