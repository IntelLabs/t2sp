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
#ifndef GEMM_CONST_PARAMS_H
#define GEMM_CONST_PARAMS_H

// Inner loop bounds, which are static constant parameters of the design
#ifdef GPU
    #define KKK         8
    #define JJJ         8
    #define III         16
    #define JJ          2
    #define II          4
    #define KK          4
#else // FPGA
    #ifdef TINY // For verifying correctness only
        #define KKK         4
        #define JJJ         2
        #define III         2
        #define JJ          2
        #define II          2
        #define KK          2
    #elif S10
        #define KKK         16
        #define JJJ         16
        #define III         16
        #define JJ          16
        #define II          16
        #define KK          16
    #else   // For A10
        #define KKK         32
        #define JJJ         8
        #define III         1
        #define JJ          16
        #define II          16
        #define KK          16
    #endif
#endif

#endif
