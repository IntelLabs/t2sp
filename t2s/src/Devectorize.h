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
#ifndef T2S_DEVECTORIZE_H
#define T2S_DEVECTORIZE_H

/** \file
 *
 * Defines a pass to break a vectorized loop carring dependences into multiple (vectorized or unrolled) loops.
 */

#include "../../Halide/src/IR.h"

namespace Halide {
namespace Internal {

/* Break the statements under a loop marked as Vectorize into sequentially executed groups, so that there is no
 * flow dependence from a group to another group above it. If there is a flow dependence from a group to itself,
 * the group has to be devectorized (remarked as Unrolled). Otherwise, the group is marked as Vectorize.
 * Assumption: there is only 1 Vectorize loop in a loop nest, and it must be at the innermost level.
 * */
extern Stmt devectorize(Stmt s);

}
}

#endif  // T2S_DEVECTORIZE_H
