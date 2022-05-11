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
#ifndef CAPSULE_CONST_PARAMS_H
#define CAPSULE_CONST_PARAMS_H

#define OX              7
#define OY              7
#define KY              3
#define KX              3
#define MY              4
#define MX              4
#define MK              4

// Inner loop bounds, which are static constant parameters of the design
#ifdef GPU
    #define CII         4
    #define CI          8
    #define COOO        8
    #define COO         1
    #define CO          4
    #define YYY_XXX     7
    #define YY_XX       7
    #define Y_X         1
    #define NN          16
#else // FPGA
    #ifdef TINY // For verifying correctness only
        #define CII         4
        #define CI          4
        #define COOO        4
        #define COO         4
        #define CO          1
        #define YYY_XXX     7
        #define YY_XX       1
        #define Y_X         7
        #define NN          1
    #elif S10
        #define CII         32
        #define CI          1
        #define COOO        8
        #define COO         4
        #define CO          1
        #define YYY_XXX     14
        #define YY_XX       1
        #define Y_X         4
        #define NN          4
    #else
        #define CII         16
        #define CI          2
        #define COOO        8
        #define COO         4
        #define CO          1
        #define YYY_XXX     10
        #define YY_XX       1
        #define Y_X         5
        #define NN          4
    #endif
#endif

#define TOTAL_IX        (OX * 2 + KX - 2)
#define TOTAL_IY        (OY * 2 + KY - 2)
#define TOTAL_CO        (COOO * COO * CO)
#define TOTAL_CI        (CII * CI)

#endif
