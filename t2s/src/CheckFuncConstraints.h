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
#ifndef T2S_CHECK_FUNC_H
#define T2S_CHECK_FUNC_H

#include <vector>

#include "../../Halide/src/Func.h"
#include "../../Halide/src/Function.h"
#include "../../Halide/src/Util.h"

/** \file
 * Check whether T2S constraints are satisfied in various operations on Func.
 * The checking is basically for the additional constraints that are imposed by T2S.
 * For example, check declaration, check URE definition constrains, check merge_ures etc.
 */

namespace Halide {
namespace Internal {

using std::vector;

class CheckFuncConstraints {
private:
    CheckFuncConstraints() {}
    CheckFuncConstraints(CheckFuncConstraints&) = delete;
    CheckFuncConstraints& operator=(const CheckFuncConstraints&);
public:
    /** Return a singleton of CheckFuncConstraints.
     * Use singleton pattern to achieve some checking based on context.
     * Not sure whether this can be used. */
    static CheckFuncConstraints& get_instance() {
        static CheckFuncConstraints one;
        return one;
    }

    /** Check immediately after Func declaration. */
    static void check_declare(const Function &func);

    static void check_ures(const std::vector<Func> &funcs);

    /** Check if the Funcs satisfy the URE constraints */
    template <typename... Args>
    static void check_ures(Func f, Args... args) {
        std::vector<Func> collected_args{f, std::forward<Args>(args)...};
        return CheckFuncConstraints::check_ures(collected_args);
    }

    static void check_ure(const Func& f, const std::vector<Func> &funcs, const vector<Var>& args = vector<Var>());

    static void check_merge_ures(const Func &control, const std::vector<Func> &funcs);

    /** Check immediately after merge_ures() is called */
    template <typename... Args>
    static void check_merge_ures(Func f, Args... args) {
        std::vector<Func> collected_args{f, std::forward<Args>(args)...};
        return CheckFuncConstraints::check_ures(collected_args);
    }

    static void check_merge_ure(const Func& f);
};

} // namespace Internal
} // namespace Halide

#endif
