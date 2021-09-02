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

#define I 64
#define J 64
#define K 64
#define II 4
#define JJ 4
#define KK 4
#define III 8
#define JJJ 8
#define KKK 4
#define OI I/II/III
#define OJ J/JJ/JJJ
#define OK K/KK/KKK

int main(void) {
    // Input parameters: a and b are 2D matrices.
    ImageParam a(type_of<float>(), 2);
    ImageParam b(type_of<float>(), 2);

    Var  oi, oj, ok, ii, jj, kk, iii, jjj, kkk;

    // Macros for convenience.
    #define P             kkk, jj, ii, jjj, iii, kk, ok, oj, oi
    #define P_ii_minus_1  kkk, jj, ii - 1, jjj, iii, kk, ok, oj, oi
    #define P_jj_minus_1  kkk, jj - 1, ii, jjj, iii, kk, ok, oj, oi
    #define P_ok_minus_1  kkk + KKK - 1, jj, ii, jjj, iii, kk + KK - 1, ok - 1, oj, oi // One case of k - 1
    #define P_kk_minus_1  kkk + KKK - 1, jj, ii, jjj, iii, kk - 1, ok, oj, oi          // Another case of k - 1
    #define P_kkk_minus_1 kkk - 1, jj, ii, jjj, iii, kk, ok, oj, oi                    // Yet another case of k - 1
    #define i             (oi * II * III + ii * III + iii)
    #define j             (oj * JJ * JJJ + jj * JJJ + jjj)
    #define k             (ok * KK * KKK + kk * KKK + kkk)
    #define P_c           jj, ii, jjj, iii, oj, oi

    #define control Int(32), {P}, PLACE1
    #define compute Float(32), {P}, PLACE1

    Func A(compute), B(compute), C(compute), c(PLACE1);     // Compute UREs
    A(P)       = select(jj == 0, a(k, i), A(P_jj_minus_1));
    B(P)       = select(ii == 0, b(k, j), B(P_ii_minus_1));
    if (KK != OK) {
    C(P)       = select((ok == 0) && (kk == 0) && (kkk == 0), 0, select(kkk == 0, select(kk == 0,
                    C(P_ok_minus_1), C(P_kk_minus_1)), C(P_kkk_minus_1))) + A(P) * B(P);
    } else {
    C(P)       = select((ok == 0) && (kk == 0) && (kkk == 0), 0, select(kkk == 0, C(P_ok_minus_1), C(P_kkk_minus_1))) + A(P) * B(P);
    }
    c(P_c)     = select((ok == (OK-1)) && (kk == (KK-1)) && (kkk == (KKK-1)), C(P));

    // Merge UREs
    A.merge_ures(B, C, c);
    A.set_bounds(kkk, 0, KKK,
                      jjj, 0, JJJ,
                      iii, 0, III)
          .set_bounds(kk,  0, KK,
                      jj,  0, JJ,
                      ii,  0, II)
          .set_bounds(ok,  0, OK,
                      oj,  0, OJ,
                      oi,  0, OI);
    A.space_time_transform(kkk, jj, ii);
    A.vectorize(kkk);

    Func feederA(PLACE1), feederB(PLACE1), loaderA(PLACE1), loaderB(PLACE1), serializerA(PLACE0), serializerB(PLACE0);
    A.isolate_producer_chain(a, serializerA, loaderA, feederA);
    A.isolate_producer_chain(b, serializerB, loaderB, feederB);
    feederA.scatter(loaderA, ii);
    feederB.scatter(loaderB, jj);
    serializerA.remove(jjj);
    serializerB.remove(iii);
    loaderA.remove(jjj);
    loaderB.remove(iii);
    feederA.buffer(loaderA, iii, BufferStrategy::Double);
    feederB.buffer(loaderB, kk, BufferStrategy::Double);

    Func drainer(PLACE1), collector(PLACE1), unloader(PLACE1);
    c.isolate_consumer_chain(drainer);
    drainer.space_time_transform(jj, ii);
    drainer.set_bounds(jjj, 0, JJJ,
                       iii, 0, III,
                       jj, 0, JJ)
           .set_bounds(ii, 0, II,
                       oj, 0, OJ,
                       oi, 0, OI);
    drainer.isolate_consumer_chain(collector, unloader);
    collector.vectorize(jj);
    unloader.vectorize(jj);
    drainer.gather(c, ii);
    collector.gather(drainer, jj);

    // Generate input and run.
    a.dim(0).set_bounds(0, K).set_stride(1);
    a.dim(1).set_bounds(0, I).set_stride(K);
    b.dim(0).set_bounds(0, K).set_stride(1);
    b.dim(1).set_bounds(0, J).set_stride(K);
    unloader.output_buffer().dim(0).set_bounds(0, JJ).set_stride(1);
    unloader.output_buffer().dim(1).set_bounds(0, II).set_stride(JJ);
    unloader.output_buffer().dim(2).set_bounds(0, JJJ).set_stride(JJ*II);
    unloader.output_buffer().dim(3).set_bounds(0, III).set_stride(JJ*II*JJJ);
    unloader.output_buffer().dim(4).set_bounds(0, OJ).set_stride(JJ*II*JJJ*III);
    unloader.output_buffer().dim(5).set_bounds(0, OI).set_stride(JJ*II*JJJ*III*OJ);

    Buffer<float> ina = new_data_2d<float, K, I>(SEQUENTIAL); //or RANDOM
    Buffer<float> inb = new_data_2d<float, K, J>(SEQUENTIAL); //or RANDOM
    a.set(ina);
    b.set(inb);
    Target target = get_host_target();
    target.set_feature(Target::Debug);
    target.set_feature(Target::IntelFPGA);
    Buffer<float> golden = get_result_of_mm2<float, I, J, K>(ina, inb);
    Buffer<float> result = unloader.realize({JJ, II, JJJ, III, OJ, OI}, target);

    for (size_t ox = 0; ox < OI; ox++) {
        for (size_t xx = 0; xx < II; xx++) {
            for (size_t xxx = 0; xxx < III; xxx++) {
                for (size_t oy = 0; oy < OJ; oy++) {
                    for (size_t yy = 0; yy < JJ; yy++) {
                        for (size_t yyy = 0; yyy < JJJ; yyy++) {
                            size_t x = xxx + xx * III + ox * II * III;
                            size_t y = yyy + yy * JJJ + oy * JJ * JJJ;
                            cout << "(" << x << ", " << y << ") = " << golden(x, y) << " " << result(yy, xx, yyy, xxx, oy, ox) << endl;
                            assert(result(yy, xx, yyy, xxx, oy, ox) == golden(x, y));
                        }
                    }
                }
            }
        }
    }

    cout << "Success!\n";
    return 0;
}
    


