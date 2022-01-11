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
#ifndef T2S_PRE_PROCESS_BEFORE_LOWER_H
#define T2S_PRE_PROCESS_BEFORE_LOWER_H

/** \file
 *
 * Defines a pass where all Functions are given a chance to be preprocessed before lower().
 *
 */

#include "../../Halide/src/Func.h"
using std::map;
using std::string;

namespace Halide {
namespace Internal {

/** Any pre-processing of the Functions in the environment during compile/realize before reaching lower(). */
extern void t2s_preprocess_before_lower(map<string, Func> &env, const Target &target);
extern void reorder_gpu_loops(Func func, int &num_gpu_vars);

}
}

#endif
