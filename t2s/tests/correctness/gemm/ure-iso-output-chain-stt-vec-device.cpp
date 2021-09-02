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

#define I 8
#define J 8
#define K 8
#define II 2
#define JJ 2
#define KK 2
#define III 2
#define JJJ 2
#define KKK 2
#define OI I/II/III
#define OJ J/JJ/JJJ
#define OK K/KK/KKK

int main(void) {
    // Input parameters: a and b are 2D matrices.
    ImageParam a(type_of<int>(), 2);
    ImageParam b(type_of<int>(), 2);

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
    #define compute Int(32), {P}, PLACE1

    Func firstk(control), firstkk(control), lastk(control); // Control UREs
    Func A(compute), B(compute), C(compute), c(PLACE1);             // Compute UREs
    firstk(P)  = select(jj == 0, k, firstk(P_jj_minus_1));
    firstkk(P) = select(jj == 0, kk, firstkk(P_jj_minus_1));
    lastk(P)   = select(jj == 0, K - 1 - (k), lastk(P_jj_minus_1));
    A(P)       = select(jj == 0, a(i, k), A(P_jj_minus_1));
    B(P)       = select(ii == 0, b(k, j), B(P_ii_minus_1));
    C(P)       = select(firstk(P) == 0, 0, select(kkk == 0, select(firstkk(P) == 0,
                    C(P_ok_minus_1), C(P_kk_minus_1)), C(P_kkk_minus_1))) + A(P) * B(P);
    c(P_c)     = select(lastk(P) == 0, C(P));

    // Merge UREs
    firstk.merge_ures(firstkk, lastk, A, B, C, c)
          .set_bounds(kkk, 0, KKK,
                      jjj, 0, JJJ,
                      iii, 0, III)
          .set_bounds(kk,  0, KK,
                      jj,  0, JJ,
                      ii,  0, II)
          .set_bounds(ok,  0, OK,
                      oj,  0, OJ,
                      oi,  0, OI);
    firstk.space_time_transform(kkk, jj, ii);
    firstk.vectorize(kkk);

    Func drainer(PLACE1), collector(PLACE1), unloader(PLACE0);
    c.isolate_consumer_chain(drainer);
    drainer.space_time_transform(jj, ii);
    drainer.isolate_consumer_chain(collector, unloader);
    collector.vectorize(jj);
    unloader.vectorize(jj);

    // Generate input and run.
    Buffer<int> ina = new_data_2d<int, I, K>(SEQUENTIAL); //or RANDOM
    Buffer<int> inb = new_data_2d<int, K, J>(SEQUENTIAL); //or RANDOM
    a.set(ina);
    b.set(inb);
    Target target = get_host_target();
    target.set_feature(Target::IntelFPGA);
    Buffer<int> golden = get_result_of_mm<int, I, K, J>(ina, inb);
    Buffer<int> result = unloader.realize({JJ, II, JJJ, III, OJ, OI}, target);

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
    


