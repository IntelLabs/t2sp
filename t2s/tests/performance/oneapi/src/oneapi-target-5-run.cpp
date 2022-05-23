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
#include "conv.generated_oneapi_header.h"

// Constant parameters (inner loop bounds) of the design
#include "oneapi-target-5-parameters.h"

// Outer loop bounds for testing
#ifdef TINY // For verifying correctness only
    #define N       4
#else
    #define N       64
#endif


int main(void){
    std::cout << "!!!CONV!!!\n";

    float *i = new float[TOTAL_IY * TOTAL_IX * TOTAL_CI * N];
    float *k = new float[KY * KX * TOTAL_CI * TOTAL_CO];
    float *o = new float[COOO * YYY * XXX * COO * YY * XX * Y * X * CO * N];


    for (size_t n = 0; n < N; n++) { // 4
        for (size_t ci = 0; ci < TOTAL_CI; ci++) { // 16
            for (size_t x = 0; x < TOTAL_IX; x++) { // 6 
                for (size_t y = 0; y < TOTAL_IY; y++) { // 6 
                    // i_h(y, x, ci, n) = random();
                    int i_index = (
                        y +
                        (TOTAL_IY * x) +
                        (TOTAL_IX * TOTAL_IY * ci) +
                        (TOTAL_IX * TOTAL_IY * TOTAL_CI * n)
                    );
                    i[i_index] = random();
                }
            }
        }
    }



    for (size_t co = 0; co < TOTAL_CO; co++) {
        for (size_t ci = 0; ci < TOTAL_CI; ci++) {
            for (size_t kx = 0; kx < KX; kx++) {
                for (size_t ky = 0; ky < KY; ky++) {
                    // k_h(ky, kx, ci, co) = random();
                    // // k[(ky*KY) + (kx*KX) + (ci*TOTAL_CI) + co] = 2.0f; //random();
                    int k_index = (
                        ky + 
                        (kx * KY) + 
                        (ci * KX * KY) + 
                        (co * TOTAL_CI * KX * KY)
                    );
                    k[k_index] = random();
                }
            }
        }
    }




    // int i_dims[] = {TOTAL_IY, TOTAL_IX, TOTAL_CI, N};
    // int k_dims[] = {KY, KX, TOTAL_CI, TOTAL_CO};
    // int o_dims[] = {COOO, YYY, XXX, COO, YY, XX, Y, X, CO, N};
    std::vector<int> i_dims{TOTAL_IY, TOTAL_IX, TOTAL_CI, N};
    std::vector<int> k_dims{KY, KX, TOTAL_CI, TOTAL_CO};
    std::vector<int> o_dims{COOO, YYY, XXX, COO, YY, XX, Y, X, CO, N};
    // Halide::Runtime::Buffer<float> i_h(TOTAL_IY, TOTAL_IX, TOTAL_CI, N);
    // Halide::Runtime::Buffer<float> k_h(KY, KX, TOTAL_CI, TOTAL_CO);
    // Halide::Runtime::Buffer<float> o_h(COOO, YYY, XXX, COO, YY, XX, Y, X, CO, N);

    Halide::Runtime::Buffer<float, 4> i_h(i, i_dims);
    Halide::Runtime::Buffer<float, 4> k_h(k, k_dims);
    Halide::Runtime::Buffer<float, 10> o_h(o, o_dims);


    std::cout << "i_dims[total:" << (TOTAL_IY * TOTAL_IX * TOTAL_CI * N) << "]: " << TOTAL_IY << ", " << TOTAL_IX << ", " << TOTAL_CI << ", " << N << "\n";
    std::cout << "k_dims[total:" << (KY * KX * TOTAL_CI * TOTAL_CO) << "]: " << KY << ", " <<  KX << ", " <<  TOTAL_CI << ", " <<  TOTAL_CO << "\n";
    std::cout << "o_dims[total:" << (COOO * YYY * XXX * COO * YY * XX * Y * X * CO * N) <<"]: " << COOO << ", " << YYY << ", " << XXX << ", " << COO << ", " << YY << ", " << XX << ", " << Y << ", " << X << ", " << CO << ", " << N << "\n";


#if defined(FPGA_EMULATOR)
  std::cout << "USING FPGA EMULATOR\n";
  //   assert( setenv("INTEL_FPGA_OCL_PLATFORM_NAME", "Intel(R) FPGA Emulation Platform for OpenCL(TM)", 1) == 0);
  //   assert( setenv("BITSTREAM", "a.h", 1) == 0);
  sycl::ext::intel::fpga_emulator_selector device_selector; // (NOTE) for emulation
#else
  std::cout << "USING REAL FPGA\n";
  //   assert( setenv("INTEL_FPGA_OCL_PLATFORM_NAME", "Intel(R) FPGA SDK for OpenCL(TM)", 1) == 0);
  //   assert( setenv("BITSTREAM", "a.h", 1) == 0);
  sycl::ext::intel::fpga_selector device_selector; // (NOTE) for full compile and hardware profiling 
#endif
    // sycl::queue q( device_selector ,  dpc_common::exception_handler, sycl::property::queue::enable_profiling());
    // sycl::queue q( device_selector ,  sycl::property::queue::enable_profiling());

    std::cout << "Creating input sycl::buffers\n";


    // conv(i, k, o);

    std::cout << "Start Run\n";
    double exec_time = 0;
    // exec_time = conv( device_selector, &i_h, &k_h, &o_h );
    exec_time = conv( device_selector, i_h.raw_buffer() , k_h.raw_buffer()  , o_h.raw_buffer()  );
    std::cout << "Run completed!\n";
    std::cout << "kernel exec time: " << exec_time << "\n";


    // Write out execution time
    const char *exec_time_file = "./exec_time.txt";
    FILE *fp = fopen(exec_time_file, "w");
    if (fp == NULL) {
        std::cout << "Failed to open "<< exec_time_file <<" for writing.\n";
    } else {
        fprintf(fp, "%f\n", exec_time);
        fclose(fp);
    }

#ifdef TINY
    // Validate the results
    for (int n = 0; n < N; n++)
    for (int x = 0; x < X; x++)
    for (int y = 0; y < Y; y++)
    for (int xx = 0; xx < XX; xx++)
    for (int yy = 0; yy < YY; yy++)
    for (int xxx = 0; xxx < XXX; xxx++)
    for (int yyy = 0; yyy < YYY; yyy++)
    for (int co = 0; co < CO; co++)
    for (int coo = 0; coo < COO; coo++)
    for (int cooo = 0; cooo < COOO; cooo++) {
        float golden = 0.0f;
        for (int ci = 0; ci < TOTAL_CI; ci++)
        for (int kx = 0; kx < KX; kx++)
        for (int ky = 0; ky < KY; ky++) {
            size_t total_iy = (yyy + YYY*yy + YYY*YY*y + ky);
            size_t total_ix = (xxx + XXX*xx + XXX*XX*x + kx);
            size_t total_co = (cooo + COOO*coo + COOO*COO*co);

            golden += i_h(total_iy, total_ix, ci, n) * k_h(ky, kx, ci, total_co);
        }
        auto o_val = o_h(cooo, yyy, xxx, coo, yy, xx, y, x, co, n);
        int o_index = (
            cooo +
            (COOO * yyy) + 
            (COOO * YYY * xxx) + 
            (COOO * YYY * XXX * coo) + 
            (COOO * YYY * XXX * COO * yy) + 
            (COOO * YYY * XXX * COO * YY * xx) + 
            (COOO * YYY * XXX * COO * YY * XX * y) + 
            (COOO * YYY * XXX * COO * YY * XX * Y * x) + 
            (COOO * YYY * XXX * COO * YY * XX * Y * X * co) + 
            (COOO * YYY * XXX * COO * YY * XX * Y * X * CO * n) 
        );
        if(fabs(golden - o_val) > 0.005*fabs(golden)){
            std::cout << "golden["<< o_index <<"]: " << golden << " <--> o: " << o_val << "\n";
            std::cout << "\t!!!ERROR!!!\n";
        }
        assert(fabs(golden - o_val) <= 0.005*fabs(golden));

    }
#else
    // // Report performance. DSPs, FMax and ExecTime are automatically figured out from the static analysis
    // // during FPGA synthesis and and the dynamic profile during the FGPA execution.
    // // A10PAC on DevCloud has 33GB/s memory bandwidth
    // double mem_bandwidth = 33;
    // double compute_roof = 2 * DSPs_oneapi() * FMax_oneapi();
    //  // Total operations (GFLOP for CONV), independent of designs
    // double number_ops = 2 * (long)(N * TOTAL_CO * TOTAL_OY * TOTAL_OX) * (long)(TOTAL_CI * KX * KY);
    // double number_bytes = (long)(TOTAL_IY * TOTAL_IX * TOTAL_CI * N) * 4 + (long)(KY * KX * TOTAL_CI * TOTAL_CO) * 4
    //                     + (long)(TOTAL_OY * TOTAL_OX * TOTAL_CO * N) * 4;
    // // double exec_time = ExecTime();
    // roofline(mem_bandwidth, compute_roof, number_ops, number_bytes, exec_time);
    // if (fopen("roofline.png", "r") == NULL) {
    //     std::cout << "Failed to draw roofline!\n";
    //     return 1;
    // }
    std::cout << "Size of tensor I: " << N << ", " << TOTAL_CI << ", " << TOTAL_IX << ", " << TOTAL_IY << "\n";
    std::cout << "Size of tensor K: " << TOTAL_CI << ", " << TOTAL_CO << ", " << KX << ", " << KY << "\n";
#endif



   
    std::cout << "Success!\n";
    exit(0);
}