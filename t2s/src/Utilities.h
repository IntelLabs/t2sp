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
#ifndef HALIDE_UTILITIES_H
#define HALIDE_UTILITIES_H

/** \file
 *
 * Defines commonly used functions.
 */

#include "../../Halide/src/Function.h"
#include "../../Halide/src/IRVisitor.h"

namespace Halide {
namespace Internal {

// If the environment contains the given function name, set func and return true.
bool function_is_in_environment(const std::string &func_name, const std::map<std::string, Function> &env, Function &func);

// Return the substring before '.', or the whole string if there is no '.'.
std::string extract_first_token(const std::string &str);

// Return the substring after the last '.', or the whole string if there is no '.'.
std::string extract_last_token(const std::string &str);

// Return the substring before the given number of `.`. Error if the number of `.` are less than expected.
std::string extract_before_tokens(const std::string &str, int num_tokens);

// Return the substring after the given number of `.`. Error if the number of `.` are less than expected.
std::string extract_after_tokens(const std::string &str, int num_tokens);

// Remove the given postfix. Error if the original string does not end with that postfix.
std::string remove_postfix(const std::string &str, const std::string &postfix);

// In the expression, for the variables that have the old prefix, replace the old prefix with with the new one.
Expr replace_prefix(const std::string &old_prefix, const std::string &new_prefix, const Expr &e);

// Return the region defined by the loops' bounds. If bounds_must_be_const is set, check the bounds and throw and error
// when any bound is not constant.
Region loop_region(const std::vector<const For *> &loops, bool bounds_must_be_const = false);

// Return the extents of the loops. If extents_must_be_const is set, check the extents and throw and error
// when any extent is not constant.
std::vector<Expr> loop_extents(const std::vector<const For *> &loops, bool extents_must_be_const = false);

// Return the loops' indices
std::vector<Expr> loop_indices(const std::vector<const For *> &loops);

// Break the condition into conjuction terms. For example, given (i==0) && (j>1), return a vector of {i==0, j>1}.
std::vector<Expr> break_logic_into_conjunction(const Expr &cond);

// Check if the condition contains terms like u==a constant for every unrolled loop u.
// Conjuct the terms into single_PE_cond. If some unrolled loops do not have such terms,
// record these loops in unrolled_loops_without_terms.
bool check_is_single_PE(bool on_device, const Expr &cond, const std::vector<std::string> &unrolled_loops,
                        const std::vector<std::string> &loops_to_serialize,
                        Expr &single_PE_cond, std::vector<std::string> &unrolled_loops_without_terms);

// Check if a loop variable is a constant in the condition in the form of "loop var == a static constant".
bool loop_var_is_constant_in_condition(const std::string &loop_name, const Expr &cond);

// Check if a variable is used.
class CheckVarUsage : public IRVisitor {
    using IRVisitor::visit;
    const std::string loop_var;
    void visit(const Variable *var) override {
        if (!used && loop_var == var->name) {
            used = true;
            return;
        }
    }
    public:
        bool used;
        CheckVarUsage(std::string _loop_var) : loop_var(_loop_var), used(false) {}
};

// Is n a power of 2? n is required to be a positive number.
bool is_power_of_two(uint32_t n);

// The power of two closest to n. n is required to be a positive number.
uint32_t closest_power_of_two(uint32_t n);

}
}

#endif


