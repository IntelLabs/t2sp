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
#define GPU 1
#include "HalideBuffer.h"
#include "util.h"
#include "const-parameter.h"

#ifdef FPGA
#include "HalideBuffer.h"
#include "gemm.generated_oneapi_header.h"
#endif
#ifdef GPU
#include "../../../../install/llvm-test-suite/SYCL/ESIMD/esimd_test_utils.hpp"
#include <iostream>
#include <sycl/ext/intel/esimd.hpp>
#include <CL/sycl.hpp>
#endif

// Constant parameters (inner loop bounds) of the design
using namespace Halide;
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
    float *c = new float[TOTAL_J * TOTAL_I];
    
    for(unsigned int i = 0; i < (TOTAL_K * TOTAL_I); i++){ a[i] = random(); }
    for(unsigned int i = 0; i < (TOTAL_J * TOTAL_K); i++){ b[i] = random(); }
    for(unsigned int i = 0; i < (TOTAL_J * TOTAL_I); i++){ c[i] = 0.0f; }


    // Specifications to be preprocessed
    // std::vector<int> a_dim{TOTAL_K, TOTAL_I};
    // std::vector<int> b_dim{TOTAL_J, TOTAL_K};
    // std::vector<int> c_dim{JJJ, III, JJ, II, J, I};

    size_t a_dim[2] = {TOTAL_K, TOTAL_I};
    size_t b_dim[2] = {TOTAL_J, TOTAL_K};
    size_t c_dim[2] = {TOTAL_J, TOTAL_I};
    




#ifdef FPGA
Halide::Runtime::Buffer<float, (sizeof(a_dim)/sizeof(size_t))> A_h(a, std::vector<size_t>(std::begin(a_dim), std::end(a_dim)));
#endif
#ifdef GPU
int _A_extent_0 = a_dim[0];
int _A_extent_1 = a_dim[1];
sycl::image<2> img_A(a, image_channel_order::rgba, image_channel_type::fp32,
range<2>{a_dim[0]/4 , a_dim[1]});
#endif
#ifdef FPGA
Halide::Runtime::Buffer<float, (sizeof(b_dim)/sizeof(size_t))> B_h(b, std::vector<size_t>(std::begin(b_dim), std::end(b_dim)));
#endif
#ifdef GPU
int _B_extent_0 = b_dim[0];
int _B_extent_1 = b_dim[1];
sycl::image<2> img_B(b, image_channel_order::rgba, image_channel_type::fp32,
range<2>{b_dim[0]/4 , b_dim[1]});
#endif
#ifdef FPGA
Halide::Runtime::Buffer<float, (sizeof(c_dim)/sizeof(size_t))> C_h(c, std::vector<size_t>(std::begin(c_dim), std::end(c_dim)));
#endif
#ifdef GPU
int _C_extent_0 = c_dim[0];
int _C_extent_1 = c_dim[1];
sycl::image<2> img_C(c, image_channel_order::rgba, image_channel_type::fp32,
range<2>{c_dim[0]/4 , c_dim[1]});
#endif
#ifdef FPGA
#if defined(FPGA_EMULATOR)
std::cout << "USING FPGA EMULATOR\n";
sycl::ext::intel::fpga_emulator_selector device_selector; // (NOTE) for emulation
#else
std::cout << "USING REAL FPGA\n";
sycl::ext::intel::fpga_selector device_selector; // (NOTE) for full compile and hardware profiling 
#endif
std::cout << "Start Run\n";
double exec_time = 0;
exec_time = gemm(device_selector, A_h.raw_buffer(), B_h.raw_buffer(), C_h.raw_buffer());
std::cout << "Run completed!\n";
std::cout << "kernel exec time: " << exec_time << "\n";
#endif






}
