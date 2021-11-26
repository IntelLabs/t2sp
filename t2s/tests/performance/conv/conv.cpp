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

int main(void)
{
    // Dependences
    #define P               cii,       coo,   yy,   xxp,  cop,  y,  x,  ky,      kx,      ci,   yp,  xp,  xx, co, n
    #define P_cii_minus_1   cii-1,     coo,   yy,   xxp,  cop,  y,  x,  ky,      kx,      ci,   yp,  xp,  xx, co, n
    #define P_ky_minus_1    cii+CII-1, coo,   yy,   xxp,  cop,  y,  x,  ky-1,    kx,      ci,   yp,  xp,  xx, co, n
    #define P_kx_minus_1    cii+CII-1, coo,   yy,   xxp,  cop,  y,  x,  ky+KY-1, kx-1,    ci,   yp,  xp,  xx, co, n
    #define P_ci_minus_1    cii+CII-1, coo,   yy,   xxp,  cop,  y,  x,  ky+KY-1, kx+KX-1, ci-1, yp,  xp,  xx, co, n
    #define P_coo_minus_1   cii,       coo-1, yy,   xxp,  cop,  y,  x,  ky,      kx,      ci,   yp,  xp,  xx, co, n
    #define P_yy_minus_1    cii,       coo,   yy-1, xxp,  cop,  y,  x,  ky,      kx,      ci,   yp,  xp,  xx, co, n
    #define P_Out                      coo,   yy,   xxp,  cop,  y,  x,                          yp,  xp,  xx, co, n
    // Linearized addresses
    #define total_iy        (yy + YY*y   + YY*Y*yp  + ky)
    #define total_ix        (xx + XX*xxp + XX*XXP*x + XX*XXP*X*xp + kx)
    #define total_oy        (yy + YY*y   + YY*Y*yp)
    #define total_ox        (xx + XX*xxp + XX*XXP*x + XX*XXP*X*xp)
    #define total_co        (coo + COO*cop + COO*COP*co)
    #define total_ci        (cii + CII*ci)

    // Type of the data to process in C and T2S
    #define CTYPE float
    #define TTYPE Float(32)

    // Inputs
#ifdef GPU
    ImageParam I("I", TTYPE, 2), K("K", TTYPE, 2);
    #define P_I     total_ci + (TOTAL_CI) * n,  total_iy + (TOTAL_IY) * total_ix
    #define P_K     total_co + (TOTAL_CO) * ky, total_ci + (TOTAL_CI) * kx
    #define P_O     total_co + (TOTAL_CO) * n,  total_oy + (TOTAL_OY) * total_ox
    #define UN      (I.dim(0).extent() / TOTAL_CI)
#else
    ImageParam I("I", TTYPE, 4), K("K", TTYPE, 4);
    #define P_I     total_iy, total_ix, total_ci, n
    #define P_K     ky, kx, total_ci, total_co
    #define P_O     P_Out
    #define UN      (I.dim(3).extent())
#endif

    // UREs
    Var cii("cii"), ci("ci"), coo("coo"), co("co"), ky("ky"), kx("kx"), yy("yy"), xx("xx"), y("y"), x("x"), n("n");
    Var xxp("xxp"), cop("cop"), yp("yp"), xp("xp");
    URE A("A", TTYPE, {P}), B("B", TTYPE, {P}), C("C", TTYPE, {P}), Out("Out");
    A(P) = select(coo == 0, I(P_I), A(P_coo_minus_1));
    B(P) = select(yy == 0, K(P_K), B(P_yy_minus_1));
    C(P) = select(cii == 0 && ky == 0 && kx == 0 && ci == 0, 0,
                select(cii == 0, select(ky == 0, select(kx == 0, C(P_ci_minus_1), C(P_kx_minus_1)), C(P_ky_minus_1)), C(P_cii_minus_1)))
                + A(P) * B(P);
    Out(P_Out) = select(cii == CII-1 && ky == KY-1 && kx == KX-1 && ci == CI-1, C(P));

    // Put all the UREs inside the same loop nest of X.
    A.merge_ures(B, C, Out);

    // Explicitly set the loop bounds
    A.set_bounds(ky,    0, KY,  kx,   0, KX)
     .set_bounds(cii,   0, CII, ci,   0, CI)
     .set_bounds(coo,   0, COO, co,   0, CO)
     .set_bounds(yy,    0, YY,  xx,   0, XX)
     .set_bounds(y,     0, Y,   x,    0, X)
     .set_bounds(xxp,   0, XXP, cop,  0, COP)
     .set_bounds(yp,    0, YP,  xp,   0, XP)
     .set_bounds(n,     0, UN);

    // Create a systolic array
    A.space_time_transform(coo, yy);

    // GPU can have many threads running in parallel.
#ifdef GPU
    A.gpu_blocks(xx, co, n).gpu_threads(y, x);
#endif

    // I/O network
    Stensor DI("iLoader", DRAM), SI("iFeeder", SRAM), DK("kLoader", DRAM), SK("kFeeder", SRAM);
    Stensor RO2("drainer", REG), RO1("collector", REG), DO("unloader", DRAM), O("deserializer");
    I >> DI.out(cii) >> FIFO(128)
      >> SI.scope(ci).out(cii, yy) >> FIFO(128);
    K >> DK.out(cii) >> FIFO(128)
      >> SK.scope(ci).out(cii, coo) >> FIFO(128);
    Out >> FIFO(1024) >> RO2.scope(xx).out(coo, yy)
        >> FIFO(128)  >> RO1.scope(yy).out(coo)
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
