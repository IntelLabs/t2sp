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
    #define P               kkk,       ii,      kk,      k,   i
    #define P_ii_minus_1    kkk,       ii-1,    kk,      k,   i
    #define P_i_minus_1     kkk,       ii,      kk,      k,   i-1
    #define P_kkk_minus_1   kkk-1,     ii,      kk,      k,   i
    #define P_kk_minus_1    kkk+KKK-1, ii,      kk-1,    k,   i
    #define P_k_minus_1     kkk+KKK-1, ii,      kk+KK-1, k-1, i
    #define P_Out           kkk,                kk,      k

    // Linearized addresses
    #define total_i         (ii + II * i)
    #define total_k         (kkk + KKK * kk + KKK * KK * k)

    // Outer loop bounds, which are determined by input sizes
    #define I 4
    #define II 4
    #define K (A.dim(0).extent() / (KKK * KK))

    // Type of the data to process in C and T2S
    #define CTYPE float
    #define TTYPE Float(32)

    // Inputs
    Param<float> Alpha, Beta;
    ImageParam A("A", TTYPE, 2), X("X", TTYPE, 1), Y("Y", TTYPE, 1);

    // UREs
    Var kkk("kkk"), kk("kk"), ii("ii"), k("k"), i("i");
    URE uA("uA", TTYPE, {P}), uXBot("uXBot", TTYPE, {P}), uXTop("uXTop", TTYPE, {P}), uZBot("uZBot", TTYPE, {P}), uZTop("uZTop", TTYPE, {P}), uZTop2("uZTop2", TTYPE, {P}), Out("Out");
    uA(P) = A(total_k, total_i);
    uXBot(P) = select(ii == 0, X(total_k), uXBot(P_ii_minus_1));
    uXTop(P) = select(kkk == 0, X(total_i), uXTop(P_kkk_minus_1));
    uZBot(P) = select(total_i == 0, 0,
                    select(kkk == 0, 
                        select(kk == 0, uZBot(P_k_minus_1), uZBot(P_kk_minus_1)),
                        uZBot(P_kkk_minus_1))) + 
                select(total_i < total_k, 0, uA(P) * uXBot(P));
    uZTop(P) = select(total_i == total_k, uZBot(P),
                    select(ii == 0, 0, uZTop(P_ii_minus_1)) + uA(P) * uXTop(P));
    uZTop2(P) = select(ii == II - 1,
                    select(i == 0, 0, uZTop2(P_i_minus_1)) + uZTop(P),
                    0);

    Out(P_Out) = select(ii == II - 1 && i == I-1, Alpha * uZTop2(P) + Beta * Y(total_k));

    // Put all the UREs inside the same loop nest of X.
    uA.merge_ures(uXBot, uXTop, uZBot, uZTop, uZTop2, Out);

    // Explicitly set the loop bounds
    uA.set_bounds(kkk, 0, KKK)
      .set_bounds(kk,  0, KK,  ii,  0, II)
      .set_bounds(k,   0, K,   i,   0, I);

    // Create a systolic array
    uA.space_time_transform(i);

    // I/O network
    Func aSerializer("aSerializer", Place::Host), aLoader("aLoader", Place::Device);
    Func xSerializer1("xSerializer1", Place::Host), xLoader1("xLoader1", Place::Device);
    Func xSerializer2("xSerializer2", Place::Host), xLoader2("xLoader2", Place::Device);
    Func ySerializer("ySerializer", Place::Host), yLoader("yLoader", Place::Device);
    Func unloader("unloader", Place::Device);
    Func deserializer("deserializer", Place::Host);

    uA.isolate_producer_chain(A, aSerializer, aLoader);
    uA.isolate_producer_chain(X(total_i), xSerializer1, xLoader1);
    uA.isolate_producer_chain(X(total_k), xSerializer2, xLoader2);
    uA.isolate_producer_chain(Y, ySerializer, yLoader);

    // xLoader.vectorize(kkk);
    // aLoader.min_depth(1024);
    // xLoader.min_depth(1024);
    // V.min_depth(1024);
    // yLoader.min_depth(1024);
    // Out.min_depth(1024);
    // unloader.min_depth(1024);

    Out.isolate_consumer_chain(unloader, deserializer);
    Target acc = get_host_target();
    acc.set_feature(Target::IntelFPGA);
    acc.set_feature(Target::EnableSynthesis);
    deserializer.compile_to_host("symv-interface", { Alpha, Beta, A, X, Y }, "symv", acc);
    printf("Success\n");
    return 0;
}
