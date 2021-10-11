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
// The generated interface for the matrix multiply kernel
#include "gemm-interface.h"

// Loop bounds
#include "sizes.h"

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

int main() {
    const int TOTAL_I = III * II * I;
    const int TOTAL_J = JJJ * JJ * J;
    const int TOTAL_K = KKK * KK * K;
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

    Halide::Runtime::Buffer<float> result(JJJ, III, JJ, II, J, I);
    GEMM(ina, inb, result);

#ifdef TINY
    // Validate the results
    for (size_t i = 0; i < I; i++) {
        for (size_t j = 0; j < J; j++) {
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
#else
    // Report performance. DSPs, FMax and ExecTime are automatically figured out from the static analysis
    // during FPGA synthesis and and the dynamic profile during the FGPA execution.
    double mem_bandwidth = 33; // A10PAC on DevCloud has 33GB/s memory bandwidth
    double compute_roof = 2 * DSPs() * FMax();
    double number_ops = 2 * (long)(III * II * I) * (long)(JJJ * JJ * J) * (long)(KKK * KK * K); // Total operations (GFLOP for GEMM), independent of designs
    double number_bytes = (long)(KKK * III * KK * II * K * J * I) * 4 + (long)(KKK * JJJ * KK * JJ * K * J * I) * 4 + (long)(III * II * I * JJJ * JJ * J) * 4;
    double exec_time=ExecTime();
    roofline(mem_bandwidth, compute_roof, number_ops, number_bytes,exec_time);
    if (fopen("roofline.png", "r") == NULL) {
        cout << "Failed to draw roofline!\n";
        return 1;
    }
#endif

    cout << "Success!\n";
    return 0;
}



