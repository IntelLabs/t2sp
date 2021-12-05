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
#include "const-parameters.h"

using namespace Halide;

int main(void)
{
    // Dependences
    #define P               cii,       cooo,   yy_xx,   my, mx, y_x, coo, ky,      kx,      ci,      mk,   co, n
    #define P_cii_minus_1   cii-1,     cooo,   yy_xx,   my, mx, y_x, coo, ky,      kx,      ci,      mk,   co, n
    #define P_ky_minus_1    cii+CII-1, cooo,   yy_xx,   my, mx, y_x, coo, ky-1,    kx,      ci,      mk,   co, n
    #define P_kx_minus_1    cii+CII-1, cooo,   yy_xx,   my, mx, y_x, coo, ky+KY-1, kx-1,    ci,      mk,   co, n
    #define P_ci_minus_1    cii+CII-1, cooo,   yy_xx,   my, mx, y_x, coo, ky+KY-1, kx+KX-1, ci-1,    mk,   co, n
    #define P_mk_minus_1    cii+CII-1, cooo,   yy_xx,   my, mx, y_x, coo, ky+KY-1, kx+KX-1, ci+CI-1, mk-1, co, n
    #define P_co3_minus_1   cii,       cooo-1, yy_xx,   my, mx, y_x, coo, ky,      kx,      ci,      mk,   co, n
    #define P_yx3_minus_1   cii,       cooo,   yy_xx-1, my, mx, y_x, coo, ky,      kx,      ci,      mk,   co, n
    #define P_Out                      cooo,   yy_xx,   my, mx, y_x, coo,                                  co, n

    // Linearized addresses
    #define total_oy        ((yy_xx + YY_XX*y_x) % OY)
    #define total_ox        ((yy_xx + YY_XX*y_x) / OY)
    #define total_iy        (total_oy * 2 + ky)
    #define total_ix        (total_ox * 2 + kx)
    #define total_co        (cooo + COOO*coo + COOO*COO*co)
    #define total_ci        (cii + CII*ci)

    // Type of the data to process in C and T2S
    #define CTYPE float
    #define TTYPE Float(32)

    // Inputs
#ifdef GPU
    ImageParam I("I", TTYPE, 2), K("K", TTYPE, 2);
    #define P_I     total_ci + (TOTAL_CI)*mk + (TOTAL_CI*MK)*mx, total_iy + (TOTAL_IY)*total_ix + (TOTAL_IY*TOTAL_IX)*n
    #define P_K     total_co + (TOTAL_CO)*my, cii + (CII)*ky + (CII*KY)*kx + (CII*KY*KX)*ci + (TOTAL_CI*KY*KX)*mk
    #define P_O     total_co + (TOTAL_CO)*my + (TOTAL_CO*MY)*mx, total_oy + (OY)*total_ox + (OY*OX)*n
    #define UN      (I.dim(1).extent() / (TOTAL_IY*TOTAL_IX))
#else
    ImageParam I("I", TTYPE, 6), K("K", TTYPE, 6);
    #define P_I     mk, mx, total_ci, total_iy, total_ix, n
    #define P_K     my, mk, total_ci, total_co, ky, kx
    #define P_O     P_Out
    #define UN      (I.dim(5).extent())
#endif

    // UREs
    Var cii("cii"), cooo("cooo"), yy_xx("yy_xx"), my("my"), mx("mx"), y_x("y_x"), coo("coo"), ky("ky"), kx("kx"), ci("ci"), mk("mk"), co("co"), n("n");
    URE A("A", TTYPE, {P}), B("B", TTYPE, {P}), C("C", TTYPE, {P}), Out("Out");
    A(P) = select(cooo == 0, I(P_I), A(P_co3_minus_1));
    B(P) = select(yy_xx == 0, K(P_K), B(P_yx3_minus_1));
    C(P) = select(cii == 0 && ci == 0 && mk == 0 && ky == 0 && kx == 0, 0,
                select(cii == 0, select(ky == 0, select(kx == 0, select(ci == 0, C(P_mk_minus_1), C(P_ci_minus_1)), C(P_kx_minus_1)), C(P_ky_minus_1)), C(P_cii_minus_1)))
                + A(P) * B(P);
    Out(P_Out) = select(cii == CII-1 && ci == CI-1 && mk == MK-1 && ky == KY-1 && kx == KX-1, C(P));

    // Put all the UREs inside the same loop nest of X.
    A.merge_ures(B, C, Out);

    // Explicitly set the loop bounds
    A.set_bounds(cooo,    0, COOO,    coo,   0, COO,   co,  0, CO)
     .set_bounds(yy_xx,   0, YY_XX,   y_x,   0, Y_X)
     .set_bounds(my,      0, MY,      mx,    0, MX)
     .set_bounds(cii,     0, CII,     ci,    0, CI)
     .set_bounds(ky,      0, KY,      kx,    0, KX)
     .set_bounds(mk,      0, MK,      n,     0, UN);

#ifdef GPU
    // GPU can have many threads running in parallel.
    A.gpu_blocks(co, n).gpu_threads(my, mx);
    A.space_time_transform(cooo, yy_xx, y_x);
    A.reorder(cii, cooo, my, mx, coo, ky, kx, yy_xx, y_x, ci, mk, co, n);
#else
    // Create a systolic array
    A.space_time_transform(cooo, yy_xx);
#endif

    // I/O network
    Stensor DI("iLoader", DRAM), SI("iFeeder", SRAM), DK("kLoader", DRAM), SK("kFeeder", SRAM);
    Stensor RO2("drainer", REG), RO1("collector", REG), DO("unloader", DRAM), O("deserializer");
#ifdef GPU
    SI.scope(y_x);
#else
    SI.scope(ci);
#endif
    I >> DI.out(cii) >> FIFO(128)
      >> SI.out(cii, yy_xx) >> FIFO(128);
    K >> DK.out(cii) >> FIFO(128)
      >> SK.scope(ci).out(cii, cooo) >> FIFO(128);
    Out >> FIFO(1024) >> RO2.scope(my).out(cooo, yy_xx)
        >> FIFO(128)  >> RO1.scope(yy_xx).out(cooo)
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
