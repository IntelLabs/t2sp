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
#include "host.h"

// The only header file needed for including T2S.
#include "HalideBuffer.h"

#include <math.h>
// For printing output
#include <stdio.h>
#include <iostream>

// For validation of results.
#include <assert.h>

// using namespace Halide;
using namespace std;

#define OUTERMOST_I 2
#define OUTERMOST_J 2
#define OUTERMOST_K 2
#define II   4
#define JJ   4
#define KK   256
#define III  2
#define JJJ  4
#define KKK  4

int main() {
    const int TOTAL_I = III * II * OUTERMOST_I;
    const int TOTAL_J = JJJ * JJ * OUTERMOST_J;
    const int TOTAL_K = KKK * KK * OUTERMOST_K;
    Halide::Runtime::Buffer<float> ina(TOTAL_K, TOTAL_I), inb(TOTAL_J, TOTAL_K);
    for (size_t i = 0; i < TOTAL_I; i++) {
        for (size_t k = 0; k < TOTAL_K; k++) {
            ina(k, i) = k + i;
        }
    }
    for (size_t k = 0; k < TOTAL_K; k++) {
        for (size_t j = 0; j < TOTAL_J; j++) {
            inb(j, k) = j - k;
        }
    }

    Halide::Runtime::Buffer<float> result(JJJ, III, JJ, II, OUTERMOST_J, OUTERMOST_I);
    GEMM(ina, inb, result);

    // Step 3: Validate the results
    for (size_t i = 0; i < OUTERMOST_I; i++) {
        for (size_t j = 0; j < OUTERMOST_J; j++) {
            for (size_t ii = 0; ii < II; ii++) {
                for (size_t jj = 0; jj < JJ; jj++) {
                    for (size_t iii = 0; iii < III; iii++) {
                        for (size_t jjj = 0; jjj < JJJ; jjj++) {
                            size_t i1 = iii + III * ii + III * II * i;
                            size_t j1 = jjj + JJJ * jj + JJJ * JJ * j;
                            float golden = 0.0f;
                            for (size_t k1 = 0; k1 < TOTAL_K; k1++) {
                                golden += ina(k1, i1) * inb(j1, k1);
                            }
                            // cout << "(" << j1 << ", " << i1 << ") = " << golden << " " << result(jjj, iii, jj, ii, j, i) << endl;
                            assert(fabs(golden - result(jjj, iii, jj, ii, j, i)) < 0.005*fabs(golden));
                        }
                    }
                }
            }
        }
    }
    cout << "Success!\n";
    return 0;
}



