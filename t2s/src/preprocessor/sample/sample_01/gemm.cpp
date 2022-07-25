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
#include "Halide.h"

// Standard includes
#include <iostream>
#include <vector>
#include <assert.h>

// Constant parameters (inner loop bounds) of the design
#include "const-parameter.h"
#define FPGA
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
    int c_dim[6] = {JJJ, III, JJ, II, J, I};


#pragma t2s_spec_start

        // Dependences
        #define P               kkk,      jjj,  iii,  jj, ii, kk,     k,  j,i
        #define P_kkk_minus_1   kkk-1,    jjj,  iii,  jj, ii, kk,     k,  j,i
        #define P_kk_minus_1    kkk+KKK-1,jjj,  iii,  jj, ii, kk-1,   k,  j,i
        #define P_k_minus_1     kkk+KKK-1,jjj,  iii,  jj, ii, kk+KK-1,k-1,j,i
        #define P_jjj_minus_1   kkk,      jjj-1,iii,  jj, ii, kk,     k,  j,i
        #define P_iii_minus_1   kkk,      jjj,  iii-1,jj, ii, kk,     k,  j,i
        #define P_Out                     jjj,  iii,  jj, ii,             j,i

        // Linearized addresses
        #define total_i         (iii + III * ii + III * II * i)
        #define total_j         (jjj + JJJ * jj + JJJ * JJ * j)
        #define total_k         (kkk + KKK * kk + KKK * KK * k)

        // Outer loop bounds, which are determined by input sizes
        #define I_T2S (A.dim(1).extent() / (III * II))
        #define J_T2S (B.dim(0).extent() / (JJJ * JJ))
        #define K_T2S (A.dim(0).extent() / (KKK * KK))

        // Type of the data to process in C and T2S
        #define CTYPE float
        #define TTYPE Float(32)


        // Inputs
        ImageParam A("A", TTYPE, 2), B("B", TTYPE, 2);

        // UREs
        Var kkk("kkk"), jjj("jjj"), iii("iii"), jj("jj"), ii("ii"), kk("kk"), k("k"), j("j"), i("i");
        URE X("X", TTYPE, {P}), Y("Y", TTYPE, {P}), Z("Z", TTYPE, {P}), Out("Out");
        X(P) = select(jjj == 0, A(total_k, total_i), X(P_jjj_minus_1));
        Y(P) = select(iii == 0, B(total_j, total_k), Y(P_iii_minus_1));
        Z(P) = select(kkk == 0 && kk == 0 && k == 0, 0,
                    select(kkk == 0, select(kk == 0, Z(P_k_minus_1), Z(P_kk_minus_1)), Z(P_kkk_minus_1)))
                    + X(P) * Y(P);
        Out(P_Out) = select(kkk == KKK-1 && kk == KK-1 && k == K_T2S-1, Z(P));

        // Put all the UREs inside the same loop nest of X.
        X.merge_ures(Y, Z, Out);

        // Explicitly set the loop bounds
        X.set_bounds(jjj, 0, JJJ, iii, 0, III, kkk, 0, KKK)
        .set_bounds(jj,  0, JJ,  ii,  0, II,  kk,  0, KK)
        .set_bounds(j,   0, J_T2S,   i,   0, I_T2S,   k,   0, K_T2S);

        // Create a systolic array
        X.space_time_transform(jjj, iii);

        // GPU can have many threads running in parallel.
#ifdef GPU
            X.gpu_blocks(j, i).gpu_threads(jj, ii);
#endif

        // I/O network
        Stensor DA("aLoader", DRAM), SA("aFeeder", SRAM), DB("bLoader", DRAM), SB("bFeeder", SRAM);
        Stensor RC2("drainer", REG), RC1("collector", REG), DC("unloader", DRAM), C("deserializer");
        A >> DA.out(kkk) >> FIFO(128)
        >> SA.scope(k).out(kkk, iii) >> FIFO(128);
        B >> DB.out(kkk) >> FIFO(128)
        >> SB.scope(k).out(kkk, jjj) >> FIFO(128);
        Out >> FIFO(1024) >> RC2.scope(jj).out(jjj, iii)
            >> FIFO(128)  >> RC1.scope(iii).out(jjj)
            >> FIFO(128)  >> DC >> C(total_j, total_i);
#ifdef GPU
    // C.compile_to_host("gemm-interface", { A, B }, "gemm", IntelGPU);
    // C.compile_to_oneapi( { A, B }, "gemm", IntelGPU);
    C.compile_to_oneapi( { A, B }, "gemm", IntelGPU);
#else
    // C.compile_to_host("gemm-interface", { A, B }, "gemm", IntelFPGA);
    C.compile_to_oneapi( { A, B }, "gemm", IntelFPGA);
#endif

#pragma t2s_spec_end


#pragma t2s_submit gemm (A, a, a_dim) (B, b, b_dim) (C, c, c_dim)

printf("Success!\n");

}