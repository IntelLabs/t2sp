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

// // Roofline utilities
// #include "../../../src/Roofline.h"

// For printing output
#include <stdio.h>
#include <iostream>

// For validation of results.
#include <assert.h>

// OneAPI Kernel Implementations
#include "gemm.generated_oneapi_header.h"

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



int main(void){
    std::cout << "!!!GEMM!!!\n";

    const int TOTAL_I = III * II * I;
    const int TOTAL_J = JJJ * JJ * J;
    const int TOTAL_K = KKK * KK * K;

    float *a = new float[TOTAL_K * TOTAL_I];
    float *b = new float[TOTAL_J * TOTAL_K];
    for (size_t i = 0; i < TOTAL_I; i++) {
        for (size_t k = 0; k < TOTAL_K; k++) {
            // a(k, i) = random();
            int a_index = k + (i*TOTAL_K);
            a[a_index] = random();
        }
    }
    for (size_t k = 0; k < TOTAL_K; k++) {
        for (size_t j = 0; j < TOTAL_J; j++) {
            // b(j, k) = random();
            int b_index = j + (k*TOTAL_J);
            b[b_index] = random();
        }
    }
    float *c = new float[JJJ * III * JJ * II * J * I];

    std::vector<int> a_dim{TOTAL_K, TOTAL_I};
    std::vector<int> b_dim{TOTAL_J, TOTAL_K};
    std::vector<int> c_dim{JJJ, III, JJ, II, J, I};

    Halide::Runtime::Buffer<float, 2> a_h(a, a_dim);
    Halide::Runtime::Buffer<float, 2> b_h(b, b_dim);
    Halide::Runtime::Buffer<float, 6> c_h(c, c_dim);



#if defined(FPGA_EMULATOR)
  std::cout << "USING FPGA EMULATOR\n";
  sycl::ext::intel::fpga_emulator_selector device_selector; // (NOTE) for emulation
#else
  std::cout << "USING REAL FPGA\n";
  sycl::ext::intel::fpga_selector device_selector; // (NOTE) for full compile and hardware profiling 
#endif

    // sycl::queue q( device_selector ,  dpc_common::exception_handler, sycl::property::queue::enable_profiling());

    std::cout << "Start Run\n";
    double exec_time = 0;
    // exec_time = gemm(device_selector, &a_h, &b_h, &c_h);
    exec_time = gemm(device_selector, a_h.raw_buffer()  , b_h.raw_buffer() , c_h.raw_buffer() );
    std::cout << "Run completed!\n";
    std::cout << "kernel exec time: " << exec_time << "\n";

    // Write out execution time
    const char *exec_time_file = "./exec_time.txt";
    FILE *fp = fopen(exec_time_file, "w");
    if (fp == NULL) {
        std::cout << "Failed to open "<< exec_time_file <<" for writing.\n";
        return -1;
    }
    fprintf(fp, "%f\n", exec_time);
    fclose(fp);

#ifdef TINY
    // Validate the results
    for (int i = 0; i < I; i++)
    for (int j = 0; j < J; j++)
        for (int ii = 0; ii < II; ii++)
        for (int jj = 0; jj < JJ; jj++)
            for (int iii = 0; iii < III; iii++)
            for (int jjj = 0; jjj < JJJ; jjj++) {
                size_t total_i = iii + III * ii + III * II * i;
                size_t total_j = jjj + JJJ * jj + JJJ * JJ * j;
                float golden = 0.0f;
                for (int k = 0; k < TOTAL_K; k++){
                    golden += a_h(k, total_i) * b_h(total_j, k);
                }

                if(fabs(golden -  c_h(jjj, iii, jj, ii, j, i)) > 0.005*fabs(golden)){
                    std::cout << "golden: " << golden << " <--> c: " << c_h(jjj, iii, jj, ii, j, i) << "\n";
                    std::cout << "\t!!!ERROR!!!\n";
                }
                assert(fabs(golden - c_h(jjj, iii, jj, ii, j, i)) < 0.005*fabs(golden));
            }
#else
    // Report performance. DSPs, FMax and ExecTime are automatically figured out from the static analysis
    // during FPGA synthesis and and the dynamic profile during the FGPA execution.
    // double mem_bandwidth = 34; // pac_a10 on DevCloud has 34GB/s memory bandwidth
    // double compute_roof = 2 * DSPs_oneapi() * FMax_oneapi();
    // double number_ops = 2 * (double)(III * II * I) * (double)(JJJ * JJ * J) * (double)(KKK * KK * K); // Total operations (GFLOP for GEMM), independent of designs
    // double number_bytes = (double)(KKK * III) * (double)(KK * II) * (double)(K * J * I) * 4 +
    //                       (double)(KKK * JJJ) * (double)(KK * JJ) * (double)(K * J * I) * 4 +
    //                       (double)(III * II * I) * (double)(JJJ * JJ * J) * 4;
    // // double exec_time= ExecTime();
    // roofline(mem_bandwidth, compute_roof, number_ops, number_bytes,exec_time);
    // if (fopen("roofline.png", "r") == NULL) {
    //     std::cout << "Failed to draw roofline!\n";
    //     return 1;
    // }
    
    std::cout << "Size of matrix A: " << TOTAL_I << " * " << TOTAL_K << "\n";
    std::cout << "Size of matrix B: " << TOTAL_K << " * " << TOTAL_J << "\n";
#endif

    std::cout << "Exec time: " << exec_time << "\n";
    std::cout << "Success!\n";
    exit(0);
}