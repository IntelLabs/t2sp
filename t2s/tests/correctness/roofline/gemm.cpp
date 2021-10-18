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

// Roofline utilities
#include "Roofline.h"

using namespace Halide;

#define I 64
#define J 64
#define K 256
#define II 2
#define JJ 2
#define KK 8
#define III 4
#define JJJ 4
#define KKK 8
#define OI (I/II/III)
#define OJ (J/JJ/JJJ)
#define OK (K/KK/KKK)
#define PLACE1 Place::Device

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

    Func firstk(control), firstkk(control), lastk(control); // Control UREs
    Func A(compute), B(compute), C(compute), c(PLACE1);     // Compute UREs
    Func ASerializer(Place::Host), BSerializer(Place::Host), unloaderDSerializer(Place::Host);
    Func fk, fkk, lk;
    fk(P) = k;
    fkk(P) = kk;
    lk(P) = K - 1 - k;
    firstk(P) = select(jj == 0, fk(P), firstk(P_jj_minus_1));
    firstkk(P) = select(jj == 0, fkk(P), firstkk(P_jj_minus_1));
    lastk(P) = select(jj == 0, lk(P), lastk(P_jj_minus_1));
    A(P) = select(jj == 0, a(k, i), A(P_jj_minus_1));
    B(P) = select(ii == 0, b(k, j), B(P_ii_minus_1));
    C(P) = select(firstk(P) == 0, 0, select(kkk == 0, select(firstkk(P) == 0,
                  C(P_ok_minus_1), C(P_kk_minus_1)), C(P_kkk_minus_1))) + A(P) * B(P);
    c(P_c) = select((lastk(P) == 0) && (kkk == (KKK - 1)), C(P));

    // Merge UREs
    Var sss, ss, s, t;
    firstk.merge_ures(firstkk, lastk, A, B, C, c);
    firstk.set_bounds(kkk, 0, KKK,
                      jjj, 0, JJJ,
                      iii, 0, III)
        .set_bounds(kk, 0, KK,
                    jj, 0, JJ,
                    ii, 0, II)
        .set_bounds(ok, 0, OK,
                    oj, 0, OJ,
                    oi, 0, OI);
    firstk.space_time_transform(kkk, jj, ii);
    firstk.vectorize(kkk);

    Func feederA(PLACE1), feederB(PLACE1), loaderA(PLACE1), loaderB(PLACE1);
    firstk.isolate_producer_chain(a, feederA);
    feederA.isolate_producer_chain(a, loaderA);
    loaderA.isolate_producer_chain(a, ASerializer);
    firstk.isolate_producer_chain(b, loaderB, feederB);
    loaderB.isolate_producer_chain(b, BSerializer);
    ASerializer.remove(jjj);
    BSerializer.remove(iii);
    feederA.scatter(loaderA, ii);
    feederB.scatter(loaderB, jj);
    loaderA.remove(jjj);
    loaderB.remove(iii);
    loaderA.min_depth(256);
    loaderB.min_depth(256);
    c.min_depth(256);
    feederA.min_depth(256);
    feederB.min_depth(256);
    feederA.buffer(loaderA, iii, BufferStrategy::Double);
    feederB.buffer(loaderB, kk, BufferStrategy::Double);

    Func drainer(PLACE1), collector(PLACE1), unloader(PLACE1);
    c.isolate_consumer_chain(drainer);
    drainer.space_time_transform(jj, ii);
    drainer.isolate_consumer_chain(collector, unloader, unloaderDSerializer);
    collector.vectorize(jj);
    unloader.vectorize(jj);
    unloaderDSerializer.vectorize(jj);
    drainer.gather(c, ii);
    drainer.min_depth(256);
    collector.gather(drainer, jj);
    collector.min_depth(256);

    // Generate input and run.
    a.dim(0).set_bounds(0, K).set_stride(1);
    a.dim(1).set_bounds(0, I).set_stride(K);
    b.dim(0).set_bounds(0, K).set_stride(1);
    b.dim(1).set_bounds(0, J).set_stride(K);
    unloaderDSerializer.output_buffer().dim(0).set_bounds(0, JJ).set_stride(1);
    unloaderDSerializer.output_buffer().dim(1).set_bounds(0, II).set_stride(JJ);
    unloaderDSerializer.output_buffer().dim(2).set_bounds(0, JJJ).set_stride(JJ * II);
    unloaderDSerializer.output_buffer().dim(3).set_bounds(0, III).set_stride(JJ * II * JJJ);
    unloaderDSerializer.output_buffer().dim(4).set_bounds(0, OJ).set_stride(JJ * II * JJJ * III);
    unloaderDSerializer.output_buffer().dim(5).set_bounds(0, OI).set_stride(JJ * II * JJJ * III * OJ);

    Buffer<float> ina = new_data_2d<float, K, I>(SEQUENTIAL); //or RANDOM
    Buffer<float> inb = new_data_2d<float, K, J>(SEQUENTIAL); //or RANDOM
    a.set(ina);
    b.set(inb);
    Target target = get_host_target();
    target.set_feature(Target::Debug);
    target.set_feature(Target::IntelFPGA);

    Buffer<float> result = unloaderDSerializer.realize({ JJ, II, JJJ, III, OJ, OI }, target);
    Buffer<float> golden = get_result_of_mm2<float, I, J, K>(ina, inb);
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
    
    // We test correctness in emulation, and thus there is not really a quartus report to produce the rooflines.
    // Here we copy an example report to the directory where a quartus report is supposed to be.
    // This is a hack, and NOT necesssary if running on real hardware.  
    system("cp acl_quartus_report.txt gemm/");

    double mem_bandwidth = 33;
    double compute_roof = 2 * DSPs() * FMax();
    double number_ops = 2 * (long)(III * II * I) * (long)(JJJ * JJ * J) * (long)(KKK * KK * K); // Total operations (GFLOP for GEMM), independent of designs
    double number_bytes = (long)(KKK * III * KK * II * K * J * I) * 4 + (long)(KKK * JJJ * KK * JJ * K * J * I) * 4 + (long)(III * II * I * JJJ * JJ * J) * 4;
    double exec_time=ExecTime();
    roofline(mem_bandwidth, compute_roof, number_ops, number_bytes,exec_time);

    if (fopen("roofline.png", "r") == NULL) {
        return 1;
    }
    cout << "Success!\n";

    return 0;
}

