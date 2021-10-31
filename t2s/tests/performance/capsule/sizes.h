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
#ifndef CONV_SIZES_H
#define CONV_SIZES_H

#ifdef TINY // For verifying correctness only
    #define CII         4
    #define COO         4
    #define BB          4
    #define MW          4
    #define MH          4
    #define MK          4
    #define W           4
    #define H           4
    #define CO          4
    #define CI          4
    #define KW          4
    #define KH          4
    #define B           1
#elif GPU
    #define CII         8
    #define COO         8
    #define BB          32
    #define MW          4
    #define MH          4
    #define MK          4
    #define W           7
    #define H           7
    #define CO          4
    #define CI          4
    #define KW          3
    #define KH          3
    #define B           2
#else
    #define CII         16
    #define COO         8
    #define BB          10
    #define MW          4
    #define MH          4
    #define MK          4
    #define W           7
    #define H           7
    #define CO          4
    #define CI          2
    #define KW          3
    #define KH          3
    #define B           16
#endif

#define TOTAL_IH        (H * 2 + KH - 1)
#define TOTAL_IW        (W * 2 + KW - 1)
#define TOTAL_CO        (COO * CO)
#define TOTAL_CI        (CII * CI)
#define TOTAL_B         (BB * B)

#endif
