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
#ifndef T2S_SPACE_TIME_TRANSFORM_H
#define T2S_SPACE_TIME_TRANSFORM_H

/** \file
 *
 * Defines a pass to apply space time transformation to the IR..
 *
 */

#include "../../Halide/src/IR.h"
#include "ComputeLoopBounds.h"

namespace Halide {
namespace Internal {

/* Info for shift registers */
struct RegBound {
    std::vector<Expr> mins;
    std::vector<Expr> maxs;
};

/* Transform the incoming loop by applying the space time transformation. Record the shift register info into reg_size_map. */
extern Stmt apply_space_time_transform(Stmt s,
                                       std::map<std::string, Function> &env,
                                       const Target &target,
                                       std::map<std::string, RegBound > &reg_size_map);

}
}

#endif
