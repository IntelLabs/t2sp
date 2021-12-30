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

using namespace Halide;

#define N               1
#define X               6
#define Y               6
#define CO              16
#define CI              32
#define MX              4
#define MK              4
#define MY              4
#define KX              3
#define KY              3
#define s               2
#define IX              14
#define IY              14

#ifdef DESIGN1
#define LAYOUT_P        MX, MK, CI, IY, IX,  N
#define LAYOUT_W        MY, MK, CI, KY, KX, CO
#define LAYOUT_V        MY, MX,  Y,  X, CO,  N
#define ARGS            my, mx, mk,      ci,      ky,      kx,    y, x, co, n
#define ARGS_Vrv1       mk
#define ARGS_Vr1        my, mx, mk-1,    ci,      ky,      kx,    y, x, co, n
#define ARGS_Vrv2       ci
#define ARGS_Vr2        my, mx, mk+MK-1, ci-1,    ky,      kx,    y, x, co, n
#define ARGS_Vrv3       ky
#define ARGS_Vr3        my, mx, mk+MK-1, ci+CI-1, ky-1,    kx,    y, x, co, n
#define ARGS_Vrv4       kx
#define ARGS_Vr4        my, mx, mk+MK-1, ci+CI-1, ky+KY-1, kx-1,  y, x, co, n
#define ARGS_P          mx, mk, ci, s*y+ky, s*x+kx, n
#define ARGS_W          my, mk, ci, ky, kx, co
#define ARGS_out        my, mx, y, x, co, n
#endif

#define LAYOUT_P        MK, CI, IY, IX, MX, N
#define LAYOUT_W        CO, MK, CI, KY, KX, MY
#define LAYOUT_V        CO, MY, MX,  Y,  X,  N
#define ARGS            co, mk,      ci,      ky,      kx,    my, mx, y, x, n
#define ARGS_Vrv1       mk
#define ARGS_Vr1        co, mk-1,    ci,      ky,      kx,    my, mx, y, x, n
#define ARGS_Vrv2       ci
#define ARGS_Vr2        co, mk+MK-1, ci-1,    ky,      kx,    my, mx, y, x, n
#define ARGS_Vrv3       ky
#define ARGS_Vr3        co, mk+MK-1, ci+CI-1, ky-1,    kx,    my, mx, y, x, n
#define ARGS_Vrv4       kx
#define ARGS_Vr4        co, mk+MK-1, ci+CI-1, ky+KY-1, kx-1,  my, mx, y, x, n
#define ARGS_P          mk, ci, s*y+ky, s*x+kx, mx, n
#define ARGS_W          co, mk, ci, ky, kx, my
#define ARGS_out        co, my, mx, y, x, n

#define t2s_buf_t       Float(32)
typedef float           buf_t;

void check_correctness(const Buffer<buf_t> &a)
{
    Buffer<buf_t> vote(MY, MX, CO, Y, X, N);
    Buffer<buf_t> aout(MY, MX, CO, Y, X, N);
    Buffer<buf_t> pose = new_data_6D<buf_t, MK, MX, CI, IY, IX, N>(VALUES::CONSTANT);
    Buffer<buf_t> weight = new_data_6D<buf_t, MY, MK, KY, KX, CO, CI>(VALUES::CONSTANT);

#define ARGS_A      ARGS_out
    for (int n = 0; n < N; n++)
        for (int x = 0; x < X; x++)
            for (int y = 0; y < Y; y++)
                for (int co = 0; co < CO; co++)
                    for (int mx = 0; mx < MX; mx++)
                        for (int my = 0; my < MY; my++) {
                            vote(my, mx, co, y, x, n) = 0.0f;
                            aout(my, mx, co, y, x, n) = a(ARGS_A);
                            for (int ci = 0; ci < CI; ci++)
                                for (int kx = 0; kx < KX; kx++)
                                    for (int ky = 0; ky < KY; ky++)
                                        for (int mk = 0; mk < MK; mk++) {
                                            vote(my, mx, co, y, x, n) +=
                                                pose(mk, mx, ci, s*y+ky, s*x+kx, n) *
                                                weight(my, mk, ky, kx, co, ci);
                                        }
                        }

    check_equal_6D<buf_t>(vote, aout);
}

Buffer<buf_t> get_golden(void)
{
    ImageParam pose(t2s_buf_t, 6), weight(t2s_buf_t, 6);
    pose.set(new_data_6D<buf_t, LAYOUT_P>(VALUES::CONSTANT));
    weight.set(new_data_6D<buf_t, LAYOUT_W>(VALUES::CONSTANT));

    Var ARGS;
    Func Func_P(t2s_buf_t, {ARGS});
    Func Func_W(t2s_buf_t, {ARGS});
    Func Func_V(t2s_buf_t, {ARGS});
    Func Func_out(t2s_buf_t, {ARGS_out});

    Func_P(ARGS) = pose(ARGS_P);
    Func_W(ARGS) = weight(ARGS_W);
    Func_V(ARGS) = select(ARGS_Vrv1 != 0, Func_V(ARGS_Vr1),
                        select(ARGS_Vrv2 != 0, Func_V(ARGS_Vr2),
                            select(ARGS_Vrv3 != 0, Func_V(ARGS_Vr3),
                                select(ARGS_Vrv4 != 0, Func_V(ARGS_Vr4), 0))))
                                    + Func_P(ARGS) * Func_W(ARGS);
    Func_out(ARGS_out) = select(mk==MK-1 && ky==KY-1 && kx==KX-1 && ci==CI-1,
                                Func_V(ARGS));

    Func_P.merge_ures(Func_W, Func_V, Func_out)
        .set_bounds(mk, 0, MK)
        .set_bounds(my, 0, MY, mx, 0, MX)
        .set_bounds(ky, 0, KY, kx, 0, KX)
        .set_bounds(ci, 0, CI, co, 0, CO)
        .set_bounds( y, 0,  Y,  x, 0,  X)
        .set_bounds( n, 0,  N);

    Target target = get_host_target();
    Buffer<buf_t> golden = Func_out.realize({LAYOUT_V}, target);
    check_correctness(golden);
    printf("URE correct\n");

    return golden;
}

int main(void)
{
    Buffer<buf_t> golden = get_golden();

    ImageParam pose(t2s_buf_t, 6), weight(t2s_buf_t, 6);
    pose.set(new_data_6D<buf_t, LAYOUT_P>(VALUES::CONSTANT));
    weight.set(new_data_6D<buf_t, LAYOUT_W>(VALUES::CONSTANT));

    Var ARGS;
    Func Func_P(t2s_buf_t, {ARGS});
    Func Func_W(t2s_buf_t, {ARGS});
    Func Func_V(t2s_buf_t, {ARGS});
    Func Func_out(t2s_buf_t, {ARGS_out});

    Func_P(ARGS) = pose(ARGS_P);
    Func_W(ARGS) = weight(ARGS_W);
    Func_V(ARGS) = select(ARGS_Vrv1 != 0, Func_V(ARGS_Vr1),
                        select(ARGS_Vrv2 != 0, Func_V(ARGS_Vr2),
                            select(ARGS_Vrv3 != 0, Func_V(ARGS_Vr3),
                                select(ARGS_Vrv4 != 0, Func_V(ARGS_Vr4), 0))))
                                    + Func_P(ARGS) * Func_W(ARGS);
    Func_out(ARGS_out) = select(mk==MK-1 && ky==KY-1 && kx==KX-1 && ci==CI-1,
                                Func_V(ARGS));

    Var k, t;
    Func_P.merge_ures(Func_W, Func_V, Func_out)
        .set_bounds(mk, 0, MK)
        .set_bounds(my, 0, MY, mx, 0, MX)
        .set_bounds(ky, 0, KY, kx, 0, KX)
        .set_bounds(ci, 0, CI, co, 0, CO)
        .set_bounds( y, 0,  Y,  x, 0,  X)
        .set_bounds( n, 0,  N)
        .space_time_transform({co, mk, ci, ky, kx},
                              {k, t},
                              {1, 0, 0,  0,     0,              // k = co
                               0, 1, MK, MK*CI, MK*CI*KY},      // t = mk + MK*ci + MK*CI*ky + MK*CI*KY*kx
                              {co, k,
                               mk, t % MK,
                               ci, (t/MK) % CI,
                               ky, (t/MK/CI) % KY,
                               kx, (t/MK/CI/KY)});
    Func_P.prefetch(pose, ci, 2);

    Func_P.store_in(MemoryType::Register);
    Func_W.store_in(MemoryType::Register);
    Func_V.store_in(MemoryType::Register);

    Target target = get_host_target();
    Buffer<buf_t> result = Func_out.realize({LAYOUT_V}, target);
    check_equal_6D<buf_t>(golden, result);
    printf("Success!\n");

    return 0;
}
