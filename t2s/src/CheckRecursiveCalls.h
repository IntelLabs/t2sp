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
#ifndef T2S__INTERNAL_CHECK_RECURSIVE_CALLS_H
#define T2S__INTERNAL_CHECK_RECURSIVE_CALLS_H

#include "../../Halide/src/IR.h"

#include <map>

namespace Halide {

namespace Internal {

/** Insert checks to find the recursive functions that without initial condition or base value.
 *  This method can find some obvious case like: f(i) = g(i - 1); g(i) = f(i - 1);
 *  But can't analyze complex case like f(i) = select(i == i, f(i - 1), 0);
 */
void check_recursive_calls(const std::map<std::string, Function> &env);

}  // namespace Internal
}  // namespace Halide

#endif
