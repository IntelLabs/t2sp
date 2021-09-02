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
#ifndef T2S_NOIFSIMPLIFY_H
#define T2S_NOIFSIMPLIFY_H

/** \file
 *
 * Defines a pass that does not simplify IfThenElse
 *
 */

#include "../../Halide/src/IR.h"

namespace Halide {
namespace Internal {

// Resolve and reduce the number of IfThenElse. If keep_loops is true,
// loops with extent equal to 1 will be kept, unless the loop is a fused loop.
Stmt no_if_simplify(Stmt s, bool keep_loops=false);

}
}

#endif
