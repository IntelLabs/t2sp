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
    #define WW          4
    #define HH          4
    #define CO          4
    #define KW          4
    #define KH          4
    #define CI          4
    #define W           4
    #define H           4
    #define B           4
#elif GPU
    #define CII         8
    #define COO         8
    #define WW          32
    #define HH          32
    #define CO          32
    #define KW          3
    #define KH          3
    #define CI          32
    #define W           2
    #define H           2
    #define B           4
#else
    #define CII         16
    #define COO         8
    #define WW          10
    #define HH          10
    #define CO          32
    #define KW          3
    #define KH          3
    #define CI          16
    #define W           6
    #define H           6
    #define B           64
#endif

#define TOTAL_IH        (HH * H + KH - 1)
#define TOTAL_IW        (WW * W + KW - 1)
#define TOTAL_OH        (HH * H)
#define TOTAL_OW        (WW * W)
#define TOTAL_CO        (COO * CO)
#define TOTAL_CI        (CII * CI)

#endif
