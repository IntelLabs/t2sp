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
#include "Halide.h"
#include "util.h"

// Constant parameters (inner loop bounds) of the design
#include "const-parameters.h"

using namespace Halide;

int main()
{
    // Dependences
    #define P               kkk,      iii,  ii, kk,     k,  i
    #define P_kkk_minus_1   kkk-1,    iii,  ii, kk,     k,  i
    #define P_kk_minus_1    kkk+KKK-1,iii,  ii, kk-1,   k,  i
    #define P_k_minus_1     kkk+KKK-1,iii,  ii, kk+KK-1,k-1,i
    #define P_jjj_minus_1   kkk,      iii,  ii, kk,     k,  i
    #define P_iii_minus_1   kkk,      iii-1,ii, kk,     k,  i
    #define P_Out           kkk,      iii,  ii, kk,     k,  i

    // Linearized addresses
    #define total_i         (iii + III * ii + III * II * i)
    #define total_j         (jjj + JJJ * jj + JJJ * JJ * j)
    #define total_k         (kkk + KKK * kk + KKK * KK * k)

    // Outer loop bounds, which are determined by input sizes
    #define I (A.dim(1).extent() / (III * II))
    #define J (B.dim(0).extent() / (JJJ * JJ))
    #define K (A.dim(0).extent() / (KKK * KK))

    // Type of the data to process in C and T2S
    #define CTYPE float
    #define TTYPE Float(32)

    // Inputs
    ImageParam A("A", TTYPE, 2), X("X", TTYPE, 1), Y("Y", TTYPE, 1);

    // UREs
    Var kkk("kkk"), iii("iii"), ii("ii"), kk("kk"), k("k"), i("i");
    URE uA("uA", TTYPE, {P}), uX("uX", TTYPE, {P}), uY("uY", TTYPE, {P}), Out("Out");
    uA(P) = A(total_k, total_i);
    uX(P) = select(kkk == 0, X(total_i), uX(P_kkk_minus_1));
    uY(P) = select(iii == 0, Y(total_k), uY(P_iii_minus_1));
    Out(P_Out) = uA(P) + uX(P) * uY(P);

    // Put all the UREs inside the same loop nest of X.
    uA.merge_ures(uX, uY, Out);

    // Explicitly set the loop bounds
    uA.set_bounds(iii, 0, III, kkk, 0, KKK)
      .set_bounds(ii,  0, II,  kk,  0, KK)
      .set_bounds(i,   0, I,   k,   0, K);

    // Create a systolic array
    uA.space_time_transform(iii);
    uA.vectorize(kkk);

    // GPU can have many threads running in parallel.
#ifdef GPU
    uA.gpu_blocks(j, i).gpu_threads(jj, ii);
#endif

    // I/O network
    // Stensor DA("aLoader", DRAM), SA("aFeeder", SRAM), DB("bLoader", DRAM), SB("bFeeder", SRAM);
    // Stensor RC("collector", REG), DC("unloader", DRAM), C("deserializer");
    // A >> DA.out(kkk)                >> FIFO(256)
    //   >> SA.scope(k).out(kkk, iii)  >> FIFO(256);
    // B >> DB.out(kkk)                >> FIFO(256)
    //   >> SB.scope(k).out(kkk, jjj)  >> FIFO(256);
    // Out >> RC.scope(iii).out(jjj)   >> FIFO(256)
    //     >> DC >> C(total_j, total_i);
    // Stensor C("deserializer");
    // Out >> C(total_i, total_k);
    Func aSerializer("aSerializer", Place::Host), aLoader("aLoader", Place::Device), aFeeder("aFeeder", Place::Device);
    Func xSerializer("xSerializer", Place::Host), xLoader("xLoader", Place::Device), xFeeder("xFeeder", Place::Device);
    Func ySerializer("ySerializer", Place::Host), yLoader("yLoader", Place::Device), yFeeder("yFeeder", Place::Device);
    Func drainer("drainer", Place::Device), collector("collector", Place::Device), unloader("unloader", Place::Device);
    Func deserializer("deserializer", Place::Host);

    uA.isolate_producer_chain(A, aSerializer, aLoader, aFeeder);
    uA.isolate_producer_chain(X, xSerializer, xLoader, xFeeder);
    uA.isolate_producer_chain(Y, ySerializer, yLoader, yFeeder);

    aLoader.min_depth(1024);
    aFeeder.min_depth(1024);
    xLoader.min_depth(1024);
    xFeeder.min_depth(1024);
    yLoader.min_depth(1024);
    yFeeder.min_depth(1024);
    Out.min_depth(1024);
    drainer.min_depth(1024);
    collector.min_depth(1024);
    unloader.min_depth(1024);

    Out.isolate_consumer_chain(drainer, collector, unloader, deserializer);
    Target acc = get_host_target();
    acc.set_feature(Target::IntelFPGA);
    acc.set_feature(Target::EnableSynthesis);
    deserializer.compile_to_host("ger-interface", { A, X, Y }, "ger", acc);

    // Compile the kernel to an FPGA bitstream, and expose a C interface for the host to invoke
// #ifdef GPU
//     C.compile_to_host("ger-interface", { A, X, Y }, "ger", IntelGPU);
// #else
//     C.compile_to_host("ger-interface", { A, X, Y }, "ger", IntelFPGA);
// #endif
    printf("Success\n");
    return 0;
}
