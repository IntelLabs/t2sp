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
    #define YY          4
    #define XXP         4
    #define COP         4
    #define XX          1
    #define CO          1
    #define KY          4
    #define KX          4
    #define CI          4
    #define YP          4
    #define XP          4
    #define X           1
    #define Y           1
    #define N           4
#elif GPU
    #define CII         8
    #define COO         8
    #define YY          32
    #define XX          32
    #define CO          32
    #define COP         1
    #define XXP         1
    #define KY          3
    #define KX          3
    #define CI          32
    #define Y           2
    #define X           2
    #define YP          1
    #define XP          1
    #define N           4
#else
    #define CII         16
    #define COO         8
    #define YY          10
    #define XXP         30
    #define COP         32
    #define XX          1
    #define CO          1
    #define KY          3
    #define KX          3
    #define CI          16
    #define YP          6
    #define XP          2
    #define X           1
    #define Y           1
    #define N           64
#endif

#define TOTAL_IX        (XX * X + KX - 1)
#define TOTAL_IY        (YY * Y + KY - 1)
#define TOTAL_OX        (XX * X)
#define TOTAL_OY        (YY * Y)
#define TOTAL_CO        (COO * CO)
#define TOTAL_CI        (CII * CI)

#endif
