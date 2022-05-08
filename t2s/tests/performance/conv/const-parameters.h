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
#ifndef CONV_CONST_PARAMS_H
#define CONV_CONST_PARAMS_H

#define KY              3
#define KX              3

// Inner loop bounds, which are static constant parameters of the design
#ifdef GPU
    #define CII         8
    #define CI          32
    #define COOO        8
    #define COO         1
    #define CO          32
    #define YYY         32
    #define XXX         1
    #define YY          2
    #define XX          2
    #define Y           1
    #define X           32
#else // FPGA
    #ifdef TINY // For verifying correctness only
        #define CII         4
        #define CI          4
        #define COOO        4
        #define COO         4
        #define CO          4
        #define YYY         4
        #define XXX         4
        #define YY          1
        #define XX          1
        #define Y           1
        #define X           1
    #elif S10
        #define CII         16
        #define CI          16
        #define COOO        16
        #define COO         16
        #define CO          1
        #define YYY         14
        #define XXX         16
        #define YY          2
        #define XX          2
        #define Y           2
        #define X           2
    #else
        #define CII         16
        #define CI          16
        #define COOO        8
        #define COO         16
        #define CO          2
        #define YYY         10
        #define XXX         16
        #define YY          2
        #define XX          2
        #define X           2
        #define Y           3
    #endif
#endif

#define TOTAL_IX        (XXX * XX * X + KX - 1)
#define TOTAL_IY        (YYY * YY * Y + KY - 1)
#define TOTAL_OX        (XXX * XX * X)
#define TOTAL_OY        (YYY * YY * Y)
#define TOTAL_CO        (COOO * COO * CO)
#define TOTAL_CI        (CII * CI)

#endif
