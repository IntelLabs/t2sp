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
    #define P               kkk,      jjj,  iii,  jj, ii, kk,     k,  j,i
    #define P_kkk_minus_1   kkk-1,    jjj,  iii,  jj, ii, kk,     k,  j,i
    #define P_kk_minus_1    kkk+KKK-1,jjj,  iii,  jj, ii, kk-1,   k,  j,i
    #define P_k_minus_1     kkk+KKK-1,jjj,  iii,  jj, ii, kk+KK-1,k-1,j,i
    #define P_jjj_minus_1   kkk,      jjj-1,iii,  jj, ii, kk,     k,  j,i
    #define P_iii_minus_1   kkk,      jjj,  iii-1,jj, ii, kk,     k,  j,i
    #define P_Out                     jjj,  iii,  jj, ii,             j,i

    // Linearized addresses
    #define total_i         (iii + III * ii + III * II * i)
    #define total_j         (jjj + III * jj + III * II * j)
    #define total_k         (kkk + KKK * kk + KKK * KK * k)

    #define is_bot          (i >= j)

    #define top_i           (iii + III * ii + III * II * i)
    #define top_j           (jjj + III * jj + III * II * j - III * II)

    #define bot_i           (III * II * I - iii - III * ii - III * II * i - 1)
    #define bot_j           (III * II * I - jjj - III * jj - III * II * j - 1)

    // Type of the data to process in C and T2S
    #define CTYPE float
    #define TTYPE Float(32)

    // Inputs
    Param<int> OpA;
    Param<float> Alpha, Beta;
    ImageParam A("A", TTYPE, 2), C("C", TTYPE, 2);

    // UREs
    Var kkk("kkk"), jjj("jjj"), iii("iii"), jj("jj"), ii("ii"), kk("kk"), k("k"), j("j"), i("i");
    URE uA("uA", TTYPE, {P}), uB("uB", TTYPE, {P}), Z("Z", TTYPE, {P}), V("V"), Out("Out");

    uA(P) = select(jjj == 0, select(is_bot, A(total_k, bot_i), A(total_k, top_i)), uA(P_jjj_minus_1));
    uB(P) = select(iii == 0, select(is_bot, A(total_k, bot_j), A(total_k, top_j)), uB(P_iii_minus_1));

    Z(P) = select(kkk == 0 && kk == 0 && k == 0, 0,
                  select(kkk == 0, 
                         select(kk == 0, Z(P_k_minus_1), Z(P_kk_minus_1)),
                         Z(P_kkk_minus_1)))
                  + uA(P) * uB(P);

    V(P_Out) = select(kkk == KKK-1 && kk == KK-1 && k == K-1, Z(P));

    Out(P_Out) = Alpha * V(P_Out) + Beta * select(is_bot, C(bot_j, bot_i), C(top_j, top_i));

    // Put all the UREs inside the same loop nest of X.
    uA.merge_ures(uB, Z, V);

    // Explicitly set the loop bounds
    uA.set_bounds(jjj, 0, III,   iii, 0, III,   kkk, 0, KKK)
      .set_bounds(jj,  0, II,    ii,  0, II,    kk,  0, KK)
      .set_bounds(j,   0, I + 1, i,   0, I / 2, k,   0, K);

    Out.set_bounds(jjj, 0, III,   iii, 0, III)
       .set_bounds(jj,  0, II,    ii,  0, II)
       .set_bounds(j,   0, I + 1, i,   0, I / 2);

    // Create a systolic array
    uA.space_time_transform(jjj, iii);
    uA.vectorize(kkk);

    // I/O network
    Func aSerializer("aSerializer", Place::Host), aLoader("aLoader", Place::Device), aFeeder("aFeeder", Place::Device);
    Func bSerializer("bSerializer", Place::Host), bLoader("bLoader", Place::Device), bFeeder("bFeeder", Place::Device);
    Func cSerializer("cSerializer", Place::Host), cLoader("cLoader", Place::Device);
    Func drainer("drainer", Place::Device), collector("collector", Place::Device), unloader("unloader", Place::Device);
    Func deserializer("deserializer", Place::Host);

    uA.isolate_producer_chain(select(is_bot, A(total_k, bot_i), A(total_k, top_i)), aSerializer, aLoader, aFeeder);
    uA.isolate_producer_chain(select(is_bot, A(total_k, bot_j), A(total_k, top_j)), bSerializer, bLoader, bFeeder);

    aSerializer.remove(jjj, jj);
    bSerializer.remove(iii, ii);

    aLoader.remove(jjj, jj);
    aFeeder.scatter(aLoader, iii);
    aFeeder.buffer(aLoader, k);
    aLoader.vectorize(kkk);
    aFeeder.vectorize(kkk);
    aLoader.min_depth(256);
    aFeeder.min_depth(256);

    bLoader.remove(iii, ii);
    bFeeder.scatter(bLoader, jjj);
    bFeeder.buffer(bLoader, k);
    bLoader.vectorize(kkk);
    bFeeder.vectorize(kkk);
    bLoader.min_depth(256);
    bFeeder.min_depth(256);

    V.min_depth(128);

    Out.isolate_producer_chain(select(is_bot, C(bot_j, bot_i), C(top_j, top_i)), cSerializer, cLoader);
    cLoader.min_depth(256);
    cSerializer.vectorize(jjj);
    cLoader.vectorize(jjj);

    Out.min_depth(1024);
    drainer.min_depth(256);
    collector.min_depth(256);

    Out.isolate_consumer_chain(drainer, collector, unloader, deserializer);
    Out.vectorize(jjj);
    drainer.vectorize(jjj);
    collector.vectorize(jjj);
    unloader.vectorize(jjj);
    deserializer.vectorize(jjj);
    collector.gather(drainer, jjj);

    Target acc = get_host_target();
    acc.set_feature(Target::IntelFPGA);
    acc.set_feature(Target::EnableSynthesis);
    deserializer.compile_to_host("syrk-interface", { OpA, Alpha, Beta, A, C }, "syrk", acc);

    printf("Success\n");
    return 0;
}
