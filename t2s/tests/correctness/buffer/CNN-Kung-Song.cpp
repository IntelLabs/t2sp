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
// To compile and run:
//    g++ CNN-Kung-Song.cpp  -g -I ../util  -I ../../../../Halide/include -L ../../../../Halide/bin -lHalide -lpthread -ldl -std=c++11 -DVERBOSE_DEBUG -O0
//    rm ~/tmp/a.* ~/tmp/a -rf
//    env CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA=1 AOC_OPTION="-march=emulator -O0 -g "   ./a.out
//
// About the design:
//    See ../../../doc/workload_analysis/CNN-Kung-Song.md for a description.
//    As we can see, cc is extended from [0, CC - 1] to [-Q + 1, CC + Q - 2]. To workaround a limitation of the compiler that requires the loop min to be non-negative,
//    we further adjust the domain to [0, CC + 2 * Q - 3], named as cc_adjusted below.
//    More precisely, the design has two parts: (1) the core systolic array and its input paths, where the extended and adjusted domain [0, CC + 2 * Q - 3] is used.
//    and (2) the output path, where the original domain [0, CC - 1] is used, so that the extended parts of results are thrown away. In short, it is the input domain
//    that gets extended, not the output domain.

#include "util.h"

#ifdef TINY // For correctness testing
#define OUTO 2
#define OUTC 2
#define OUTR 2
#define I 4
#define OO 6
#define CC 16
#define RR 2
#define Q 3
#define P 3
#define TYPE int
#define HALIDE_TYPE Int(32)
#elif defined(MEDIUM) // For correctness testing of bigger scale
#define OUTO 2
#define OUTC 2
#define OUTR 4
#define I 4
#define OO 6
#define CC 128
#define RR 16
#define Q 3
#define P 3
#define TYPE int
#define HALIDE_TYPE Int(32)
#else // For performance testing
#define OUTO 16
#define OUTC 16
#define OUTR 16
#define I 128
#define OO 8
#define CC 128
#define RR 128
#define Q 3
#define P 3
#define TYPE float
#define HALIDE_TYPE Float(32)
#endif

#define cc (cc_adjusted - Q + 1)
#define PLACE Place::Device

int main(void) {
    // setenv("WAIT", "wait", 1);
    ImageParam x(type_of<TYPE>(), 3);
    ImageParam w(type_of<TYPE>(), 4);

// Macros: for convenient use.
// Note that we have to put loop rr outside of loop cc. Otherwise, the dependence vector due to A_last_q_previous_rr will be negative.
#define A p, q, cc_adjusted, rr, oo, i, outc, outr, outo
#define A_q_minus_1_cc_minus_1 p, q - 1, cc_adjusted - 1, rr, oo, i, outc, outr, outo
#define A_cc_minus_1 p, q, cc_adjusted - 1, rr, oo, i, outc, outr, outo
#define A_q_minus_1 p, q - 1, cc_adjusted, rr, oo, i, outc, outr, outo
#define A_p_minus_1 p - 1, q, cc_adjusted, rr, oo, i, outc, outr, outo
#define A_i_minus_1 p, q, cc_adjusted, rr, oo, i - 1, outc, outr, outo
#define A_last_q_previous_rr p + 1, q + Q - 1, cc_adjusted + Q - 1, rr - 1, oo, i, outc, outr, outo // Used only when q == 0. We must write as q+Q-1 \
                                                                                                    // instead of Q-1 to help the compiler calculate the dependence
#define FUNC_DECL HALIDE_TYPE, {A}, PLACE

    Var A, rr1, t;
    Func X(FUNC_DECL), W(FUNC_DECL), H(FUNC_DECL), V(FUNC_DECL), Z(FUNC_DECL), // A recurrent Func needs declare return type, args, and place.
        O(PLACE);                                                              // A non-recurrent Func needs declare only its place.

    Expr x_for_first_q_last_p = x(outc * CC + cc + Q - 1, outr * RR + rr + P - 1, i);
    Expr x_for_first_q_other_p_first_rr = x(outc * CC + cc + Q - 1, outr * RR + p, i);

    X(A) = select(q == 0, select(p == P - 1, select(cc < CC, x_for_first_q_last_p, 0 /*arbitrary value, not contributing to output anyway*/), select(rr == 0, x_for_first_q_other_p_first_rr, X(A_last_q_previous_rr))),
                  select(cc >= q - (Q - 1), X(A_q_minus_1_cc_minus_1), 0 /*arbitrary value, not contributing to output anyway*/));
    W(A) = select(q == 0, select(cc <= 0, w(cc + Q - 1, p, i, outo * OO + oo), W(A_cc_minus_1)),
                  select(cc <= 0, select(cc >= q - (Q - 1), W(A_q_minus_1_cc_minus_1), 0 /*arbitrary value, not contributing to output anyway*/),
                         W(A_cc_minus_1)));
    H(A) = select(q == 0, 0, H(A_q_minus_1)) + X(A) * W(A);
    V(A) = select(q == Q - 1, select(p == 0, 0, V(A_p_minus_1)) + H(A), 0 /* arbitrary value, not contributing to output anyway*/);
    Z(A) = select(i == 0, 0, Z(A_i_minus_1)) + V(A);
    O(cc_adjusted, rr, oo, outc, outr, outo) = select(cc >= 0 && cc < CC && q == Q - 1 && p == P - 1 && i == I - 1, Z(A));

    X.merge_ures(W, H, V, Z, O) // Put all the UREs into the same loop nest
        .set_bounds(p, 0, P, q, 0, Q, cc_adjusted, 0, CC + 2 * Q - 2)
        .set_bounds(rr, 0, RR, oo, 0, OO, i, 0, I)
        .set_bounds(outc, 0, OUTC, outr, 0, OUTR, outo, 0, OUTO);

    // X.space_time_transform(p, q);
    //  .unroll(rr); // We cannot add rr as a space loop into the space-time transform, because rr has to be outside of cc, which is sequential, while
    //               // in space-time transform, we require all space loops to be contiguous and at the innermost levels. So just unroll loop rr here.
    //               // The only difference from being a space loop in space-time transform is that this loop cannot be time scheduled, but instead, driven
    //               // by dependences. Since we did not specify a time schedule anyway, there is no real difference.

    Func XSerializer1(Place::Host), XLoader1(PLACE), XFeeder1(PLACE), // For the few streams corresponding x_for_first_q_other_p_first_rr
        XSerializer2(Place::Host), XLoader2(PLACE), XFeeder2(PLACE),  // For most of the streams that correspond to x_for_first_q_last_p
        WSerializer(Place::Host), WLoader(PLACE), WFeeder1(PLACE), WFeeder2(PLACE),
        OCollector(PLACE), OUnloader(PLACE), ODeserializer(Place::Host);

    X.isolate_producer_chain(x_for_first_q_other_p_first_rr, XFeeder1)
        .isolate_producer_chain(x_for_first_q_last_p, XFeeder2)
        .isolate_producer_chain(w, WFeeder2);

    XFeeder1.space_time_transform(
        {p, q, cc_adjusted, rr},
        {p, q, rr, t},
        {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 0, 1,
            0 /*p*/, 0 /*q*/, 1 /*cc*/, 0 /*rr*/
        },
        {
            p, p,
            q, q,
            cc_adjusted, (t),
            rr,rr,
        },
        SpaceTimeTransform::CheckTime);

    XFeeder2.space_time_transform(
        {p, q, cc_adjusted, rr},
        {p, q, rr, t},
        {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 0, 1,
            0 /*p*/, 0 /*q*/, 1 /*cc*/, 0 /*rr*/
        },
        {
            p, p,
            q, q,
            cc_adjusted, t,
            rr,rr,
        },
        SpaceTimeTransform::CheckTime);

    WFeeder2.space_time_transform(
        {p, q, cc_adjusted, rr},
        {p, q, rr, t},
        {
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 0, 1,
            0 /*p*/, 0 /*q*/, 1 /*cc*/, 0 /*rr*/
        },
        {
            p, p,
            q, q,
            cc_adjusted, t,
            rr,rr,
        },
        SpaceTimeTransform::CheckTime);

    XFeeder1.isolate_producer_chain(x_for_first_q_other_p_first_rr, XSerializer1, XLoader1);
    XFeeder2.isolate_producer_chain(x_for_first_q_last_p, XSerializer2, XLoader2);
    WFeeder2.isolate_producer_chain(w, WSerializer, WLoader, WFeeder1);

    X.space_time_transform(
        {p, q, cc_adjusted, rr, oo, i},
        {p, q, rr, t},
        {1, 0, 0, 0, 0, 0,
         0, 1, 0, 0, 0, 0,
         0, 0, 0, 1, 0, 0,
         0 /*p*/, 1 /*q*/, 1 /*cc*/, 2 * Q - 1 /*rr*/, (Q + CC + 2 * Q - 2 + (2 * Q - 1) * RR), (Q + CC + 2 * Q - 2 + (2 * Q - 1) * RR) * OO},
        {p, p,
         q, q,
         cc_adjusted, (t - (2 * Q - 1) * rr - q) % (Q + CC + 2 * Q - 2 + (2 * Q - 1) * RR),
         rr, rr,
         oo, ((t - (2 * Q - 1) * rr - q - cc_adjusted) / (Q + CC + 2 * Q - 2 + (2 * Q - 1) * RR)) % OO,
         i, (t - (2 * Q - 1) * rr - q - cc_adjusted - (Q + CC + 2 * Q - 2 + (2 * Q - 1) * RR) * oo) / ((Q + CC + 2 * Q - 2 + (2 * Q - 1) * RR) * OO)},
        SpaceTimeTransform::CheckTime);

    O.isolate_consumer_chain(OCollector);

    OCollector.space_time_transform(
        {cc_adjusted, rr},
        {rr, t},
        {0, 1,
         1, 0},
        {cc_adjusted, t,
         rr, rr});

    OCollector.isolate_consumer_chain(OUnloader, ODeserializer);

    // For the output path, use the original domain for the cc loop (although still called cc_adjusted)
    OCollector.set_bounds(cc_adjusted, 0, CC)
        .set_bounds(rr, 0, RR, oo, 0, OO)
        .set_bounds(outc, 0, OUTC, outr, 0, OUTR, outo, 0, OUTO);

    OUnloader.set_bounds(cc_adjusted, 0, CC)
        .set_bounds(rr, 0, RR, oo, 0, OO)
        .set_bounds(outc, 0, OUTC, outr, 0, OUTR, outo, 0, OUTO);

    ODeserializer.set_bounds(cc_adjusted, 0, CC)
        .set_bounds(rr, 0, RR, oo, 0, OO)
        .set_bounds(outc, 0, OUTC, outr, 0, OUTR, outo, 0, OUTO);

    // Note that rr is not at inner-most level originally, but it becomes the inner-most loop after stt. 
    // That's why we can vectorize rr. 
    OCollector.gather(O, rr).vectorize(rr);
    OUnloader.vectorize(rr);
    ODeserializer.vectorize(rr);

    XSerializer1.remove(oo).remove(outo);
    XLoader1.min_depth(10000);
    XLoader1./*vectorize(rr).*/ remove(oo);
    XFeeder1.buffer(XLoader1, i, BufferStrategy::Double);
    XFeeder1.min_depth(10000);
    XFeeder1.scatter(XLoader1, p); // Issue: last PE is inactive.

    XSerializer2.remove(oo).remove(outo);
    XLoader2/*.vectorize(p)*/.remove(oo);
    XLoader2.min_depth(10000);
    XFeeder2.buffer(XLoader2, i, BufferStrategy::Double).scatter(XLoader2, rr);
    XFeeder2.min_depth(10000);

    WSerializer.remove(rr).remove(outr).remove(outc);
    WLoader.remove(rr);
    WLoader.min_depth(10000);
    // WLoader.remove(cc_adjusted); // w depends on cc_adjusted, and thus it cannot be removed
    WFeeder1.buffer(WLoader, i, BufferStrategy::Double).scatter(WLoader, rr);
    WFeeder1.min_depth(10000);
    WFeeder2.scatter(WFeeder1, p);
    WFeeder2.min_depth(10000);

    O.min_depth(10000);
    OCollector.min_depth(10000);
    OUnloader.min_depth(10000);

    // Generate input and run.
    Buffer<TYPE> inx = new_data_3D<TYPE, OUTC * CC + Q - 1, OUTR * RR + P - 1, I>(SEQUENTIAL); //or RANDOM
    Buffer<TYPE> inw = new_data_4D<TYPE, Q, P, I, OUTO * OO>(SEQUENTIAL);                      //or RANDOM
    x.set(inx);
    w.set(inw);
    Target target = get_host_target();
    target.set_feature(Target::IntelFPGA);
    target.set_feature(Target::Debug);
    Buffer<TYPE> results = ODeserializer.realize({CC, RR, OO, OUTC, OUTR, OUTO}, target);
    void check(const Buffer<TYPE> &, const Buffer<TYPE> &, const Buffer<TYPE> &results);
    check(inx, inw, results);
    cout << "Success!\n";
    return 0;
}

void check(const Buffer<TYPE> &x, const Buffer<TYPE> &w, const Buffer<TYPE> &results) {
    Buffer<TYPE> golden(CC, RR, OO, OUTC, OUTR, OUTO);
    for (int outo = 0; outo < OUTO; outo++) {
        for (int outr = 0; outr < OUTR; outr++) {
            for (int outc = 0; outc < OUTC; outc++) {
                for (int oo = 0; oo < OO; oo++) {
                    for (int rr = 0; rr < RR; rr++) {
                        for (int cc1 = 0; cc1 < CC; cc1++) {
                            golden(cc1, rr, oo, outc, outr, outo) = 0;
                            for (int i = 0; i < I; i++) {
                                for (int q = 0; q < Q; q++) {
                                    for (int p = 0; p < P; p++) {
                                        golden(cc1, rr, oo, outc, outr, outo) +=
                                            (TYPE)x(outc * CC + cc1 + Q - 1 - q, outr * RR + rr + p, i) * w(Q - 1 - q, p, i, outo * OO + oo);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    check_equal_6D<TYPE>(golden, results);
}
