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
#include "sizes.h"

using namespace Halide;

int main(void)
{
    // Dependences
    #define P               r_cii,       coo,   bb,   mw, hw, co, r_ci,      r_mk,      r_kw,      r_kh,   mh, b
    #define P_cii_minus_1   r_cii-1,     coo,   bb,   mw, hw, co, r_ci,      r_mk,      r_kw,      r_kh,   mh, b
    #define P_ci_minus_1    r_cii+CII-1, coo,   bb,   mw, hw, co, r_ci-1,    r_mk,      r_kw,      r_kh,   mh, b
    #define P_mk_minus_1    r_cii+CII-1, coo,   bb,   mw, hw, co, r_ci+CI-1, r_mk-1,    r_kw,      r_kh,   mh, b
    #define P_kw_minus_1    r_cii+CII-1, coo,   bb,   mw, hw, co, r_ci+CI-1, r_mk+MK-1, r_kw-1,    r_kh,   mh, b
    #define P_kh_minus_1    r_cii+CII-1, coo,   bb,   mw, hw, co, r_ci+CI-1, r_mk+MK-1, r_kw+KW-1, r_kh-1, mh, b
    #define P_coo_minus_1   r_cii,       coo-1, bb,   mw, hw, co, r_ci,      r_mk,      r_kw,      r_kh,   mh, b
    #define P_bb_minus_1    r_cii,       coo,   bb-1, mw, hw, co, r_ci,      r_mk,      r_kw,      r_kh,   mh, b
    #define P_Out                        coo,   bb,   mw, hw, co,                                          mh, b

    // Linearized addresses
    #define total_iw        ((hw % W)*2 + r_kw)
    #define total_ih        ((hw / W)*2 + r_kh)
    #define total_co        (coo + COO * co)
    #define total_ci        (r_cii + CII * r_ci)
    #define total_b         (bb + BB * b)

    // Type of the data to process in C and T2S
    #define CTYPE float
    #define TTYPE Float(32)

    // Inputs
#ifdef GPU
    ImageParam I("I", TTYPE, 2), K("K", TTYPE, 2);
    #define P_I     (total_ci) + TOTAL_CI*(r_mk) + TOTAL_CI*MK*(mh), (total_b) + TOTAL_B*(total_iw) + TOTAL_B*TOTAL_IW*(total_ih)
    #define P_K     (total_co) + TOTAL_CO*(r_mk) + TOTAL_CO*MK*(mw), (total_ci) + TOTAL_CI*(r_kw) + TOTAL_CI*KW*(r_kh)
    #define P_O     (total_co) + TOTAL_CO*(mw) + TOTAL_CO*MW*(mh),   (total_b) + TOTAL_B*hw
    #define UB      (I.dim(1).extent() % TOTAL_B)
#else
    ImageParam I("I", TTYPE, 6), K("K", TTYPE, 6);
    #define P_I     r_mk, mh, total_ci, total_iw, total_ih, total_b
    #define P_K     mw, r_mk, total_ci, total_co, r_kw, r_kh
    #define P_O     P_Out
    #define UB      (I.dim(5).extent() / BB)
#endif

    // UREs
    Var r_cii("r_cii"), coo("coo"), bb("bb"), mw("mw"), hw("hw"), co("co"), r_ci("r_ci"), r_mk("r_mk"), r_kw("r_kw"), r_kh("r_kh"), mh("mh"), b("b");
    URE X("X", TTYPE, {P}), Y("Y", TTYPE, {P}), Z("Z", TTYPE, {P}), Out("Out");
    X(P) = select(coo == 0, I(P_I), X(P_coo_minus_1));
    Y(P) = select(bb == 0, K(P_K), Y(P_bb_minus_1));
    Z(P) = select(r_cii == 0 && r_ci == 0 && r_mk == 0 && r_kw == 0 && r_kh == 0, 0,
                select(r_cii == 0, select(r_ci == 0, select(r_mk == 0, select(r_kw == 0, Z(P_kh_minus_1), Z(P_kw_minus_1)), Z(P_mk_minus_1)), Z(P_ci_minus_1)), Z(P_cii_minus_1)))
                + X(P) * Y(P);
    Out(P_Out) = select(r_cii == CII-1 && r_ci == CI-1 && r_mk == MK-1 && r_kw == KW-1 && r_kh == KH-1, Z(P));

    // Put all the UREs inside the same loop nest of X.
    X.merge_ures(Y, Z, Out);

    // Explicitly set the loop bounds
    X.set_bounds(bb,    0, BB,  b,    0, UB)
     .set_bounds(r_cii, 0, CII, r_ci, 0, CI)
     .set_bounds(coo,   0, COO, co,   0, CO)
     .set_bounds(r_kw,  0, KW,  r_kh, 0, KH)
     .set_bounds(r_mk,  0, MK,  hw,   0, H*W)
     .set_bounds(mw,    0, MW,  mh,   0, MH);

    // Create a systolic array
    X.space_time_transform(coo, bb);

    // GPU can have many threads running in parallel.
#ifdef GPU
    X.gpu_blocks(hw, co, b).gpu_threads(mw, mh);
#endif

    // I/O network
    Stensor DI("iLoader", DRAM), SI("iFeeder", SRAM), DK("kLoader", DRAM), SK("kFeeder", SRAM);
    Stensor RO2("drainer", REG), RO1("collector", REG), DO("unloader", DRAM), O("deserializer");
    I >> DI.out(r_cii) >> FIFO(128)
      >> SI.scope(r_kw).out(r_cii, bb) >> FIFO(128);
    K >> DK.out(r_cii) >> FIFO(128)
      >> SK.scope(r_kw).out(r_cii, coo) >> FIFO(128);
    Out >> FIFO(1024) >> RO2.scope(mh).banks(coo, bb)
        >> FIFO(128)  >> RO1.scope(bb).banks(coo)
        >> FIFO(128)  >> DO >> O(P_O);

    // Compile the kernel to an FPGA bitstream, and expose a C interface for the host to invoke
#ifdef GPU
    O.compile_to_host("capsule-interface", { I, K }, "capsule", IntelGPU);
#else
    O.compile_to_host("capsule-interface", { I, K }, "capsule", IntelFPGA);
#endif
    printf("Success\n");
    return 0;
}
