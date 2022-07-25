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
#define GPU 1
#include "Halide.h"
#include "util.h"
// Constant parameters (inner loop bounds) of the design
#include "capsule_const-parameter.h"
#define N           4
#define TOTAL_N     N * NN
#define SIZE_P_0    TOTAL_CI * MK * MX * NN
#define SIZE_P_1    TOTAL_IY * TOTAL_IX * N
#define SIZE_W_0    TOTAL_CO * MY
#define SIZE_W_1    TOTAL_CI * KY * KX * MK
#define SIZE_V_0    TOTAL_CO * MY * MX * NN
#define SIZE_V_1    OY * OX * N
using namespace Halide;
int main(){
    float *p = new float[SIZE_P_0 * SIZE_P_1];
    float *w = new float[SIZE_W_0 * SIZE_W_1];
    float *v = new float[SIZE_V_0 * SIZE_V_1];
    for (unsigned m = 0; m < SIZE_P_0 * SIZE_P_1; ++m)
        p[m] = random()%5;
    for (unsigned m = 0; m < SIZE_W_0 * SIZE_W_1; ++m)
        w[m] = random()%5;
    for (unsigned m = 0; m < SIZE_V_0 * SIZE_V_1; ++m)
        v[m] = random()%5;


    size_t p_dim[2] = {SIZE_P_0, SIZE_P_1};
    size_t w_dim[2] = {SIZE_W_0, SIZE_W_1};
    size_t v_dim[2] = {SIZE_V_0, SIZE_V_1};
    

#pragma t2s_spec_start
        // Dependences
    #define Index               cii,       cooo,   yyy_xxx,   yy_xx, y_x, my, mx, coo, nn, ky,      kx,      ci,      mk,   co, n
    #define Index_cii_minus_1   cii-1,     cooo,   yyy_xxx,   yy_xx, y_x, my, mx, coo, nn, ky,      kx,      ci,      mk,   co, n
    #define Index_ky_minus_1    cii+CII-1, cooo,   yyy_xxx,   yy_xx, y_x, my, mx, coo, nn, ky-1,    kx,      ci,      mk,   co, n
    #define Index_kx_minus_1    cii+CII-1, cooo,   yyy_xxx,   yy_xx, y_x, my, mx, coo, nn, ky+KY-1, kx-1,    ci,      mk,   co, n
    #define Index_ci_minus_1    cii+CII-1, cooo,   yyy_xxx,   yy_xx, y_x, my, mx, coo, nn, ky+KY-1, kx+KX-1, ci-1,    mk,   co, n
    #define Index_mk_minus_1    cii+CII-1, cooo,   yyy_xxx,   yy_xx, y_x, my, mx, coo, nn, ky+KY-1, kx+KX-1, ci+CI-1, mk-1, co, n
    #define Index_co3_minus_1   cii,       cooo-1, yyy_xxx,   yy_xx, y_x, my, mx, coo, nn, ky,      kx,      ci,      mk,   co, n
    #define Index_yx3_minus_1   cii,       cooo,   yyy_xxx-1, yy_xx, y_x, my, mx, coo, nn, ky,      kx,      ci,      mk,   co, n
    #define Index_Out                      cooo,   yyy_xxx,   yy_xx, y_x, my, mx, coo, nn,                                  co, n
    // Linearized addresses
    #define total_oy        ((yyy_xxx + YYY_XXX*yy_xx + YYY_XXX*YY_XX*y_x) % OY)
    #define total_ox        ((yyy_xxx + YYY_XXX*yy_xx + YYY_XXX*YY_XX*y_x) / OY)
    #define total_iy        (total_oy * 2 + ky)
    #define total_ix        (total_ox * 2 + kx)
    #define total_ci        (cii  + CII*ci)
    #define total_n         (nn   + NN*n)
    #define total_co        (cooo + COOO*coo + COOO*COO*co)

    // Type of the data to process in C and T2S
    #define CTYPE float
    #define TTYPE Float(32)

    // Inputs
    ImageParam P("P", TTYPE, 2), W("W", TTYPE, 2);
    #define Index_P     total_ci + (TOTAL_CI)*mk + (TOTAL_CI*MK)*mx + (TOTAL_CI*MK*MX)*nn,  total_iy + (TOTAL_IY)*total_ix + (TOTAL_IY*TOTAL_IX)*n
    #define Index_W     total_co + (TOTAL_CO)*my,                                           cii + (CII)*ky + (CII*KY)*kx + (CII*KY*KX)*ci + (TOTAL_CI*KY*KX)*mk
    #define Index_V     total_co + (TOTAL_CO)*my + (TOTAL_CO*MY)*mx + (TOTAL_CO*MY*MX)*nn,  total_oy + (OY)*total_ox + (OY*OX)*n
    #define UN          (P.dim(1).extent() / (TOTAL_IY*TOTAL_IX))

    // UREs
    Var cii("cii"), my("my"), mx("mx"), nn("nn"), ky("ky"), kx("kx"), ci("ci"), mk("mk"), n("n");
    Var yyy_xxx("yyy_xxx"), yy_xx("yy_xx"), y_x("y_x"), cooo("cooo"), coo("coo"), co("co");
    URE A("A", TTYPE, {Index}), B("B", TTYPE, {Index}), C("C", TTYPE, {Index}), Out("Out");
    A(Index) = select(cooo == 0, P(Index_P), A(Index_co3_minus_1));
    B(Index) = select(yyy_xxx == 0, W(Index_W), B(Index_yx3_minus_1));
    C(Index) = select(cii == 0 && ci == 0 && mk == 0 && ky == 0 && kx == 0, 0,
                select(cii == 0, select(ky == 0, select(kx == 0, select(ci == 0, C(Index_mk_minus_1), C(Index_ci_minus_1)), C(Index_kx_minus_1)), C(Index_ky_minus_1)), C(Index_cii_minus_1)))
                + A(Index) * B(Index);
    Out(Index_Out) = select(cii == CII-1 && ci == CI-1 && mk == MK-1 && ky == KY-1 && kx == KX-1, C(Index));

    // Put all the UREs inside the same loop nest of X.
    A.merge_ures(B, C, Out);

    // Explicitly set the loop bounds
    A.set_bounds(cooo,    0, COOO,    coo,   0, COO,   co,  0, CO)
     .set_bounds(my,      0, MY,      mx,    0, MX,    mk,  0, MK)
     .set_bounds(yyy_xxx, 0, YYY_XXX, yy_xx, 0, YY_XX, y_x, 0, Y_X)
     .set_bounds(cii,     0, CII,     ci,    0, CI)
     .set_bounds(ky,      0, KY,      kx,    0, KX)
     .set_bounds(nn,      0, NN,      n,     0, UN);
    A.space_time_transform(cooo, yyy_xxx, yy_xx);

#ifdef GPU
    // GPU can have many threads running in parallel.
    A.gpu_blocks(co, nn, n).gpu_threads(my, mx);
    A.reorder(cii, cooo, y_x, my, mx, coo, ky, kx, yyy_xxx, yy_xx, ci, mk, co, nn, n);
#endif

    // I/O network
    Stensor DP("PLoader", DRAM), SP("PFeeder", SRAM), DW("WLoader", DRAM), SW("WFeeder", SRAM);
    Stensor RV("collector", REG), DV("unloader", DRAM), V("V");
#ifdef GPU
    SP.scope(yy_xx).out(cii, yyy_xxx);
#else
    SP.scope(ci).out(cii, yyy_xxx);
#endif
    P >> DP.out(cii) >> FIFO(256) >> SP >> FIFO(256);
    W >> DW.out(cii) >> FIFO(256)
      >> SW.scope(ci).out(cii, cooo)    >> FIFO(256);
    Out >> RV.scope(yyy_xxx).out(cooo)  >> FIFO(256)
        >> DV >> V(Index_V);

    // Compile the kernel to an FPGA bitstream, and expose a C interface for the host to invoke
#ifdef GPU
    V.compile_to_oneapi({ P, W }, "capsule", IntelGPU);
#else
    V.compile_to_oneapi({ P, W }, "capsule", IntelFPGA);
#endif

#pragma t2s_spec_end


#pragma t2s_submit capsule (P, p, p_dim) (W, w, w_dim) (V, v, v_dim)
}
