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
#ifndef T2S_CHANNEL_PROMOTION_H
#define T2S_CHANNEL_PROMOTION_H

/** \file
 *
 * Defines a pass to move a channel read/write above a loop to convert "an array of channels" into
 * "a channel of array", since too many asynchronous channels may consume resources and lower frequency.
 * For example, channel float A[I] is converted into channel float[I] A, and transform code into:
 * float[I] arrA = read_channel("A")
 * unrolled for (i, 0, I) {
 *   // original: float a = read_channel("A", i)
 *   float a = arrA[i]
 * }
 */

#include "../../Halide/src/IR.h"

namespace Halide {
namespace Internal {

/* Promote channels */
extern Stmt channel_promotion(Stmt s);

}
}

#endif
