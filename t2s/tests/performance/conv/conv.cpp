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
    #define P               r_cii,       coo,   ww,   hh, co, r_kw,      r_kh,      r_ci,   w,   h,   b
    #define P_cii_minus_1   r_cii-1,     coo,   ww,   hh, co, r_kw,      r_kh,      r_ci,   w,   h,   b
    #define P_kw_minus_1    r_cii+CII-1, coo,   ww,   hh, co, r_kw-1,    r_kh,      r_ci,   w,   h,   b
    #define P_kh_minus_1    r_cii+CII-1, coo,   ww,   hh, co, r_kw+KW-1, r_kh-1,    r_ci,   w,   h,   b
    #define P_ci_minus_1    r_cii+CII-1, coo,   ww,   hh, co, r_kw+KW-1, r_kh+KH-1, r_ci-1, w,   h,   b
    #define P_coo_minus_1   r_cii,       coo-1, ww,   hh, co, r_kw,      r_kh,      r_ci,   w,   h,   b
    #define P_ww_minus_1    r_cii,       coo,   ww-1, hh, co, r_kw,      r_kh,      r_ci,   w,   h,   b
    #define P_Out                        coo,   ww,   hh, co,                               w,   h,   b

    // Linearized addresses
    #define total_iw        (ww + WW * w + r_kw)
    #define total_ih        (hh + HH * h + r_kh)
    #define total_ow        (ww + WW * w)
    #define total_oh        (hh + HH * h)
    #define total_co        (coo + COO * co)
    #define total_ci        (r_cii + CII * r_ci)

    // Type of the data to process in C and T2S
    #define CTYPE float
    #define TTYPE Float(32)

    // Inputs
#ifdef GPU
    ImageParam I("I", TTYPE, 2), K("K", TTYPE, 2);
    #define P_I     total_ci + (TOTAL_CI) * b,  total_iw + TOTAL_IW * total_ih
    #define P_K     total_co + TOTAL_CO * r_kw, total_ci + (TOTAL_CI) * r_kh
    #define P_O     total_co + (TOTAL_CO) * b,  total_ow + TOTAL_OW * total_oh
    #define UB      (I.dim(0).extent() / TOTAL_CI)
#else
    ImageParam I("I", TTYPE, 4), K("K", TTYPE, 4);
    #define P_I     total_iw, total_ih, total_ci, b
    #define P_K     r_kw, r_kh, total_ci, total_co
    #define P_O     P_Out
    #define UB      (I.dim(3).extent())
#endif

    // UREs
    Var r_cii("r_cii"), r_ci("r_ci"), r_kw("r_kw"), r_kh("r_kh"), coo("coo"), co("co"), ww("ww"), hh("hh"), w("w"), h("h"), b("b");
    URE X("X", TTYPE, {P}), Y("Y", TTYPE, {P}), Z("Z", TTYPE, {P}), Out("Out");
    X(P) = select(coo == 0, I(P_I), X(P_coo_minus_1));
    Y(P) = select(ww == 0, K(P_K), Y(P_ww_minus_1));
    Z(P) = select(r_cii == 0 && r_kw == 0 && r_kh == 0 && r_ci == 0, 0,
                select(r_cii == 0, select(r_kw == 0, select(r_kh == 0, Z(P_ci_minus_1), Z(P_kh_minus_1)), Z(P_kw_minus_1)), Z(P_cii_minus_1)))
                + X(P) * Y(P);
    Out(P_Out) = select(r_cii == CII-1 && r_kw == KW-1 && r_kh == KH-1 && r_ci == CI-1, Z(P));

    // Put all the UREs inside the same loop nest of X.
    X.merge_ures(Y, Z, Out);

    // Explicitly set the loop bounds
    X.set_bounds(r_kw,  0, KW,  r_kh, 0, KH)
     .set_bounds(r_cii, 0, CII, r_ci, 0, CI)
     .set_bounds(coo,   0, COO, co,   0, CO)
     .set_bounds(ww,    0, WW,  hh,   0, HH)
     .set_bounds(w,     0, W,   h,    0, H)
     .set_bounds(b,     0, UB);

    // Create a systolic array
    X.space_time_transform(coo, ww);

    // GPU can have many threads running in parallel.
#ifdef GPU
    X.gpu_blocks(hh, co, b).gpu_threads(w, h);
#endif

    // I/O network
    Stensor DI("iLoader", DRAM), SI("iFeeder", SRAM), DK("kLoader", DRAM), SK("kFeeder", SRAM);
    Stensor RO2("drainer", REG), RO1("collector", REG), DO("unloader", DRAM), O("deserializer");
    I >> DI.out(r_cii) >> FIFO(128)
      >> SI.scope(r_ci).out(r_cii, ww) >> FIFO(128);
    K >> DK.out(r_cii) >> FIFO(128)
      >> SK.scope(r_ci).out(r_cii, coo) >> FIFO(128);
    Out >> FIFO(1024) >> RO2.scope(hh).banks(coo, ww)
        >> FIFO(128)  >> RO1.scope(ww).banks(coo)
        >> FIFO(128)  >> DO >> O(P_O);

    // Compile the kernel to an FPGA bitstream, and expose a C interface for the host to invoke
#ifdef GPU
    O.compile_to_host("conv-interface", { I, K }, "CONV", IntelGPU);
#else
    O.compile_to_host("conv-interface", { I, K }, "CONV", IntelFPGA);
#endif
    printf("Success\n");
    return 0;
}
