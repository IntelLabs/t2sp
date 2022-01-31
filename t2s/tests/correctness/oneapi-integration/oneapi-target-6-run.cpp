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
// The only header file needed for including T2S.
#include "HalideBuffer.h"

// Roofline utilities
#include "../../../src/Roofline.h"

// For printing output
#include <stdio.h>
#include <iostream>

// For validation of results.
#include <assert.h>

// OneAPI Kernel Implementations
#include "capsule.generated_oneapi_header.h"

// Constant parameters (inner loop bounds) of the design
#include "oneapi-target-6-parameters.h"



#ifdef TINY // For verifying correctness only
    #define N       4
#else
    #define N       64
#endif

using namespace std;

int main()
{
    std::cout << "!!!CAPSULE!!!\n";
    
    float *P = new float[MK * MX * TOTAL_CI * TOTAL_IY * TOTAL_IX * N];
    float *W = new float[MY * MK * TOTAL_CI * TOTAL_CO * KY * KX];

    for (size_t n = 0; n < N; n++)
    for (size_t x = 0; x < TOTAL_IX; x++)
    for (size_t y = 0; y < TOTAL_IY; y++)
    for (size_t ci = 0; ci < TOTAL_CI; ci++) {
        for (size_t mx = 0; mx < MX; mx++) {
            for (size_t mk = 0; mk < MK; mk++) {
                P[(
                    mk +
                    (mx * MK) + 
                    (ci * MX * MK) + 
                    (y * TOTAL_CI * MX * MK) + 
                    (x * TOTAL_IY * TOTAL_CI * MX * MK) +
                    (n * TOTAL_IX * TOTAL_IY * TOTAL_CI * MX * MK)
                )] = random();
            }
        }
    }
    for (size_t kx = 0; kx < KX; kx++)
    for (size_t ky = 0; ky < KY; ky++)
    for (size_t co = 0; co < TOTAL_CO; co++)
    for (size_t ci = 0; ci < TOTAL_CI; ci++) {
        for (size_t mk = 0; mk < MK; mk++) {
            for (size_t my = 0; my < MY; my++) {
                // W(my, mk, ci, co, ky, kx) = random();
                W[(
                    my + 
                    (mk * MY) + 
                    (ci * MK * MY) +
                    (co * TOTAL_CI * MK * MY) + 
                    (ky * TOTAL_CO * TOTAL_CI * MK * MY) + 
                    (kx * KY * TOTAL_CO * TOTAL_CI * MK * MY)
                )] = random();
            }
        }
    }
    float *V = new float[COOO * YYY_XXX * YY_XX * Y_X * MY * MX * COO * CO * N];

    std::vector<int> P_dims{MK, MX, TOTAL_CI, TOTAL_IY, TOTAL_IX, N};
    std::vector<int> W_dims{MY, MK, TOTAL_CI, TOTAL_CO, KY, KX};    
    std::vector<int> V_dims{COOO, YYY_XXX, YY_XX, Y_X, MY, MX, COO, CO, N};


    Halide::Runtime::Buffer<float, 6> P_h(P, P_dims);
    Halide::Runtime::Buffer<float, 6> W_h(W, W_dims);
    Halide::Runtime::Buffer<float, 9> V_h(V, V_dims);

#if defined(FPGA_EMULATOR)
  std::cout << "USING FPGA EMULATOR\n";
  sycl::ext::intel::fpga_emulator_selector device_selector; // (NOTE) for emulation
#else
  std::cout << "USING REAL FPGA\n";
  sycl::ext::intel::fpga_selector device_selector; // (NOTE) for full compile and hardware profiling 
#endif



    std::cout << "Start Run\n";
    // capsule(P, W, V);
    double exec_time;
    // exec_time = capsule(device_selector, &P_h, &W_h, &V_h );
    exec_time = capsule(device_selector, P_h.raw_buffer() , W_h.raw_buffer() , V_h.raw_buffer() );
    std::cout << "Run completed!\n";
    std::cout << "kernel exec time: " << exec_time << "\n";

#ifdef TINY
    // Validate the results
    for (int n = 0; n < N; n++)
    for (int co = 0; co < CO; co++)
    for (int coo = 0; coo < COO; coo++)
    for (int cooo = 0; cooo < COOO; cooo++)
    for (int y_x = 0; y_x < Y_X; y_x++)
    for (int yy_xx = 0; yy_xx < YY_XX; yy_xx++)
    for (int yyy_xxx = 0; yyy_xxx < YYY_XXX; yyy_xxx++)
    for (int mx = 0; mx < MX; mx++)
    for (int my = 0; my < MY; my++) {
        float golden = 0.0f;
        size_t total_oy = (yyy_xxx + YYY_XXX*yy_xx + YYY_XXX*YY_XX*y_x) % OY;
        size_t total_ox = (yyy_xxx + YYY_XXX*yy_xx + YYY_XXX*YY_XX*y_x) / OY;
        for (int kx = 0; kx < KX; kx++)
        for (int ky = 0; ky < KY; ky++)
        for (int mk = 0; mk < MK; mk++)
        for (int ci = 0; ci < TOTAL_CI; ci++) {
            size_t total_iy = total_oy * 2 + ky;
            size_t total_ix = total_ox * 2 + kx;
            size_t total_co = cooo + COOO*coo + COOO*COO*co;

            // int p_index = (
            //     mk +
            //     (mx * MK) + 
            //     (ci * MX * MK) + 
            //     (total_iy * TOTAL_CI * MX * MK) + 
            //     (total_ix * TOTAL_IY * TOTAL_CI * MX * MK) +
            //     (n * TOTAL_IX * TOTAL_IY * TOTAL_CI * MX * MK)
            // );
            // int w_index = (
            //     my + 
            //     (mk * MY) + 
            //     (ci * MK * MY) +
            //     (total_co * TOTAL_CI * MK * MY) + 
            //     (ky * TOTAL_CO * TOTAL_CI * MK * MY) + 
            //     (kx * KY * TOTAL_CO * TOTAL_CI * MK * MY)
            // );
            // auto p_val = P[p_index];
            // auto w_val = W[w_index];

            golden += P_h(mk, mx, ci, total_iy, total_ix, n) * W_h(my, mk, ci, total_co, ky, kx);
        }
        assert(fabs(golden - V_h(cooo, yyy_xxx, yy_xx, y_x, my, mx, coo, co, n)) < 0.005*fabs(golden));


        // assert(fabs(golden - V[(COOO*cooo) + (YYY_XXX*yyy_xxx) + (YY_XX*yy_xx) + (Y_X*y_x) + (MY*my) + (MX*mx) + (COO*coo) + (CO*co) + n] ) < 0.005*fabs(golden));
        // auto v_val = result[(
        //     cooo + 
        //     (yyy_xxx * COOO) + 
        //     (yy_xx * YYY_XXX * COOO) + 
        //     (y_x * YY_XX * YYY_XXX * COOO) + 
        //     (my * Y_X * YY_XX * YYY_XXX * COOO) + 
        //     (mx * MY * Y_X * YY_XX * YYY_XXX * COOO) + 
        //     (coo * MX * MY * Y_X * YY_XX * YYY_XXX * COOO) + 
        //     (co * COO * MX * MY * Y_X * YY_XX * YYY_XXX * COOO) + 
        //     (n * CO * COO * MX * MY * Y_X * YY_XX * YYY_XXX * COOO)
        // )];
        // assert(fabs(golden - v_val) <= 0.005*fabs(golden));
    }
#else
    // Report performance. DSPs, FMax and ExecTime are automatically figured out from the static analysis
    // during FPGA synthesis and and the dynamic profile during the FGPA execution.
    // A10PAC on DevCloud has 33GB/s memory bandwidth
    double mem_bandwidth = 33;
    double compute_roof = 2 * DSPs_oneapi() * FMax_oneapi();
     // Total operations (GFLOP for CONV), independent of designs
    double number_ops = 2 * (long)(N * TOTAL_CO) * (long)(MY * MX * YYY_XXX * YY_XX * Y_X) * (long)(TOTAL_CI * MK * KY * KX);
    double number_bytes = (long)(MX * MK * TOTAL_CI * TOTAL_IY * TOTAL_IX * N) * 4
                        + (long)(MY * MK * TOTAL_CI * TOTAL_CO * KY * KX) * 4
                        + (long)(TOTAL_CO * YYY_XXX * YY_XX * Y_X * MY * MX * N) * 4;
    // double exec_time = ExecTime();
    roofline(mem_bandwidth, compute_roof, number_ops, number_bytes, exec_time);
    if (fopen("roofline.png", "r") == NULL) {
        std::cout << "Failed to draw roofline!\n";
        return 1;
    }
    std::cout << "Size of tensor P: " << N << ", " << TOTAL_CI << ", " << TOTAL_IX << ", " << TOTAL_IY
                                 << ", " << MX << ", " << MK << "\n";
    std::cout << "Size of tensor W: " << TOTAL_CI << ", " << TOTAL_CO << ", " << KX << ", " << KY
                                 << ", " << MK << ", " << MY << "\n";
#endif

    printf("Success\n");
    return 0;
}