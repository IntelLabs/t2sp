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
#ifndef T2S_SCATTERANDBUFFER_H
#define T2S_SCATTERANDBUFFER_H

#include "../../Halide/src/IR.h"

namespace Halide {

namespace Internal{
/* Find loops that will be serialized due to scattering. If Func p is isolated out of Func c as a producer,
 * and Func c scatters data along loop l, then in Func p, loop l will be serialized during scattering.
 */
extern void find_loops_to_serialize_by_scattering(const std::map<std::string, Function> &env, std::vector<std::string> &loops_to_serialize);

/* Scatter and/or buffer data */
extern Stmt scatter_buffer(Stmt s, const std::map<std::string, Function> &env);
}
}
#endif

