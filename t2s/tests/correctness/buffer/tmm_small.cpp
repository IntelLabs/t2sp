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

#define OI 2
#define OJ 2
#define OK 2
#define OL 2
#define II 2
#define JJ 2
#define KK 2
#define LL 2
#define III 2
#define JJJ 2
#define KKK 2
#define LLL 2
#define I OI*II*III
#define J OJ*JJ*JJJ
#define K OK*KK*KKK
#define L OL*LL*LLL

int main(void) {
    // Input parameters: a and b are 2D matrices.
    ImageParam a(type_of<float>(), 3);
    ImageParam b(type_of<float>(), 2);

    Var  oi, oj, ok, ol, ii, jj, kk, ll, iii, jjj, kkk, lll;

    // Macros for convenience.
    #define P             lll, jj, kk, ii, kkk, jjj, iii, ll, ol, ok, oj, oi
    #define P_jj_minus_1  lll, jj - 1, kk, ii, kkk, jjj, iii, ll, ol, ok, oj, oi
    #define P_kk_minus_1  lll, jj, kk - 1, ii, kkk, jjj, iii, ll, ol, ok, oj, oi
    #define P_lll_minus_1 lll - 1, jj, kk, ii, kkk, jjj, iii, ll, ol, ok, oj, oi
    #define P_ll_minus_1  lll + LLL - 1, jj, kk, ii, kkk, jjj, iii, ll - 1, ol, ok, oj, oi
    #define i             (oi * II * III + ii * III + iii)
    #define j             (oj * JJ * JJJ + jj * JJJ + jjj)
    #define k             (ok * KK * KKK + kk * KKK + kkk)
    #define l             (ol * LL * LLL + ll * LLL + lll)
    #define P_c           jj, kk, ii, kkk, jjj, iii, ok, oj, oi

    #define control Int(32), {P}, PLACE1
    #define compute Float(32), {P}, PLACE1

    Func A(compute), B(compute), C(compute), c(PLACE1);     // Compute UREs
    A(P)       = select(kk == 0, a(l, i, j), A(P_kk_minus_1));
    B(P)       = select(jj == 0, b(l, k),    B(P_jj_minus_1));
    C(P)       = select((ol == 0) && (ll == 0) && (lll == 0), 0, select(lll == 0, C(P_ll_minus_1), C(P_lll_minus_1))) + A(P) * B(P);
    c(P_c)     = select((ol == (OL-1)) && (ll == (LL-1)) && (lll == (LLL-1)), C(P));

    // Merge UREs
    A.merge_ures(B, C, c);
    A.set_bounds(kkk, 0, KKK,
                 jjj, 0, JJJ,
                 iii, 0, III,
                 lll, 0, LLL)
     .set_bounds(kk,  0, KK,
                 jj,  0, JJ,
                 ii,  0, II,
                 ll,  0, LL)
     .set_bounds(ok,  0, OK,
                 oj,  0, OJ,
                 oi,  0, OI,
                 ol,  0, OL);
    A.space_time_transform(lll, jj, kk);
    A.vectorize(lll);

    Func feederA(PLACE1), feederB(PLACE1), loaderA(PLACE1), loaderB(PLACE1), serializerA(PLACE0), serializerB(PLACE0);
    A.isolate_producer_chain(a, serializerA, loaderA, feederA);
    A.isolate_producer_chain(b, serializerB, loaderB, feederB);
    feederA.scatter(loaderA, jj);
    feederB.scatter(loaderB, kk);
    serializerA.remove(kkk);
    serializerB.remove(iii);
    serializerB.remove(jjj);
    loaderA.remove(kkk);
    loaderB.remove(iii);
    loaderB.remove(jjj);
    feederA.buffer(loaderA, iii, BufferStrategy::Double);
    feederB.buffer(loaderB, ll, BufferStrategy::Double);

    Func drainer(PLACE1), collector(PLACE1), unloader(PLACE1);
    c.isolate_consumer_chain(drainer);
    drainer.space_time_transform(jj, kk);
    drainer.set_bounds(jjj, 0, JJJ,
                       iii, 0, III,
                       kkk, 0, KKK)
           .set_bounds(jj,  0, JJ,
                       ii,  0, II,
                       kk,  0, KK)
           .set_bounds(oj,  0, OJ,
                       oi,  0, OI,
                       ok,  0, OK);
    drainer.isolate_consumer_chain(collector, unloader);
    collector.vectorize(jj);
    unloader.vectorize(jj);
    drainer.gather(c, kk);
    collector.gather(drainer, jj);

    // Generate input and run.
    a.dim(0).set_bounds(0, L).set_stride(1);
    a.dim(1).set_bounds(0, I).set_stride(L);
    a.dim(2).set_bounds(0, J).set_stride(L*I);
    b.dim(0).set_bounds(0, L).set_stride(1);
    b.dim(1).set_bounds(0, K).set_stride(L);
    unloader.output_buffer().dim(0).set_bounds(0, JJ).set_stride(1);
    unloader.output_buffer().dim(1).set_bounds(0, KK).set_stride(JJ);
    unloader.output_buffer().dim(2).set_bounds(0, II).set_stride(JJ*KK);
    unloader.output_buffer().dim(3).set_bounds(0, KKK).set_stride(JJ*KK*II);
    unloader.output_buffer().dim(4).set_bounds(0, JJJ).set_stride(JJ*KK*II*KKK);
    unloader.output_buffer().dim(5).set_bounds(0, III).set_stride(JJ*KK*II*JJJ*KKK);
    unloader.output_buffer().dim(6).set_bounds(0, OK).set_stride(JJ*KK*II*JJJ*KKK*III);
    unloader.output_buffer().dim(7).set_bounds(0, OJ).set_stride(JJ*KK*II*JJJ*KKK*III*OK);
    unloader.output_buffer().dim(8).set_bounds(0, OI).set_stride(JJ*KK*II*JJJ*KKK*III*OJ*OK);

    Buffer<float> ina = new_data_3d<float, L, I, J>(SEQUENTIAL); //or RANDOM
    Buffer<float> inb = new_data_2d<float, L, K>(SEQUENTIAL); //or RANDOM
    a.set(ina);
    b.set(inb);
    Target target = get_host_target();
    target.set_feature(Target::Debug);
    target.set_feature(Target::IntelFPGA);
    Buffer<float> golden = get_result_of_tmm<float, I, J, K, L>(ina, inb);
    Buffer<float> result = unloader.realize({JJ, KK, II, KKK, JJJ, III, OK, JJ, OI}, target);

    for (size_t ox = 0; ox < OI; ox++) {
        for (size_t xx = 0; xx < II; xx++) {
            for (size_t xxx = 0; xxx < III; xxx++) {
                for (size_t oy = 0; oy < OJ; oy++) {
                    for (size_t yy = 0; yy < JJ; yy++) {
                        for (size_t yyy = 0; yyy < JJJ; yyy++) {
                            for (size_t oz = 0; oz < OK; oz++) {
                                for (size_t zz = 0; zz < KK; zz++) {
                                    for (size_t zzz = 0; zzz < KKK; zzz++) {
                                        size_t x = xxx + xx * III + ox * II * III;
                                        size_t y = yyy + yy * JJJ + oy * JJ * JJJ;
                                        size_t z = zzz + zz * KKK + oz * KK * KKK;
                                        cout << "(" << x << ", " << y << ", " << z << ") = " << golden(x, y, z) << " " << result(yy, zz, xx, zzz, yyy, xxx, oz, oy, ox) << endl;
                                        assert(result(yy, zz, xx, zzz, yyy, xxx, oz, oy, ox) == golden(x, y, z));
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    cout << "Success!\n";
    return 0;
}
    


