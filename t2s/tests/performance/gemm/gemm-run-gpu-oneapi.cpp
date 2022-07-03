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
#include "util.h"

// The only header file needed for including T2S.
#include "HalideBuffer.h"

// Standard includes
#include <iostream>
#include <vector>
#include <assert.h>

// Constant parameters (inner loop bounds) of the design
#include "oneapi-target-4-parameters.h"

// Outer loop bounds for testing
#ifdef TINY // For verifying correctness only
    #define K           4
    #define J           4
    #define I           4
#else
    #define K           32
    #define J           32
    #define I           32
#endif





int main(){
    const int TOTAL_I = III * II * I;
    const int TOTAL_J = JJJ * JJ * J;
    const int TOTAL_K = KKK * KK * K;

    float *a = new float[TOTAL_K * TOTAL_I];
    float *b = new float[TOTAL_J * TOTAL_K];
    float *c = new float[JJJ * III * JJ * II * J * I];
    
    for(unsigned int i = 0; i < (TOTAL_K * TOTAL_I); i++){ a[i] = random(); }
    for(unsigned int i = 0; i < (TOTAL_J * TOTAL_K); i++){ b[i] = random(); }
    for(unsigned int i = 0; i < (JJJ * III * JJ * II * J * I); i++){ c[i] = 0.0f; }


    // Specifications to be preprocessed
    // std::vector<int> a_dim{TOTAL_K, TOTAL_I};
    // std::vector<int> b_dim{TOTAL_J, TOTAL_K};
    // std::vector<int> c_dim{JJJ, III, JJ, II, J, I};

    int a_dim[2] = {TOTAL_K, TOTAL_I};
    int b_dim[2] = {TOTAL_J, TOTAL_K};
    //int c_dim[6] = {JJJ, III, JJ, II, J, I};
    int _B_extent_0
}