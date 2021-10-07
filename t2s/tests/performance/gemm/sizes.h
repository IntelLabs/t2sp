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
#ifndef GEMM_SIZES_H
#define GEMM_SIZES_H

#ifdef TINY
    #define II          4
    #define JJ          4
    #define KK          4
    #define III         4
    #define JJJ         4
    #define KKK         4
    #define O_I         4
    #define O_J         4
    #define O_K         4
#elif S10
    #define II          32
    #define JJ          32
    #define KK          32
    #define III         12
    #define JJJ         16
    #define KKK         16
    #define O_I         32
    #define O_J         32
    #define O_K         4
#else
    #define II          32
    #define JJ          32
    #define KK          32
    #define III         10
    #define JJJ         8
    #define KKK         16
    #define O_I         32
    #define O_J         32
    #define O_K         4
#endif

#endif
