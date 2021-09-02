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
#ifndef T2S_AUTORUNKERNELS_H
#define T2S_AUTORUNKERNELS_H

/** \file
 *
 * Defines pass to create spatial ASTs.
 */

#include <map>

namespace Halide {
namespace Internal {

// Are the kernels in the environment autorunnable? If not, why?
extern void are_kernels_autorunnable(Stmt s, const std::map<std::string, Function> &env, std::map<std::string, std::string> &non_autorunnable_funcs_and_why);

// Make kernels autorunnable if possible.
extern Stmt autorun_kernels(Stmt s, const std::map<std::string, Function> &env);

}
}

#endif
