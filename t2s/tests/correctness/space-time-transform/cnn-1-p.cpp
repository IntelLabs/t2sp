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
// 15 designs for cnn below from Jason Cong and Jie  Wang. PolySA: polyhedral-based systolic array auto-compilation. ICCAD '18.
//      z(o,r,c) += x(i,r+p,c+q) * w(o,i,p,q)
//      Input: x[I][R+2][C+2], w[O][I][3][3]
//      Output: z[O][I][R][C]
//          for (int o = 0; o < O; o++)
//            for (int r = 0; r < R; r++)
//              for (int c = 0; c < C; c++)
//                z[o][r][c] = 0;
//                for (int i = 0; i < I; i++)
//                  for (int p = 0; p < P; p++)
//                    for (int q = 0; q < Q; q++)
//                      z[o][r][c] += x[i][r + p][c + q] * w[o][i][p][q];
// The "OUT_IMG_H/W" in the PolySA paper corresponds to "R/C" here.
// The designs that have negative dependence vectors have been moved to cnn-3-n.cpp. We enable them in a different set of UREs in cnn-2-p.cpp.

#include "util.h"

// Parameters from Table 2 of the paper
#define O 4     // OUT_NUM
#define I 2     // IN_NUM
#define R 3     // OUT_IMG_H
#define C 3     // OUT_IMG_W
#define P 3
#define Q 3

void kernel(Func &X, Func &W, Func &Z, Func &z, Var &q, Var &p, Var &i, Var &c, Var &r, Var &o) {
    // Input parameters: x and w are 3D and 4D tensors.
    ImageParam x(type_of<int>(), 3);
    ImageParam w(type_of<int>(), 4);

    // Macros: for convenient use.
    #define A                     q, p, i, c, r, o
    // From these, you can see the dependence vectors deltaA:
    //    (0, -1, 0, 0, 1, 0)
    //    (-1, 0, 0, 1, 0, 0)
    //    (0, 0, 0, 0, 0, 1)
    //    (0, 0, 0, 0  1, 0)
    //    (0, 0, 0, 1, 0, 0)
    //    (-Q+1, -P+1, 1, 0, 0, 0)
    //    (-Q+1, 1, 0, 0, 0, 0)
    //    (1, 0, 0, 0, 0, 0)
    // These dependence vectors are positive (read from right to left, i.e. from the outermost to the innermost loop).
    #define A_p_plus_1_r_minus_1 q, p + 1, i, c, r - 1, o
    #define A_q_plus_1_c_minus_1 q + 1, p, i, c - 1, r, o
    #define A_o_minus_1          q, p, i, c, r, o - 1
    #define A_r_minus_1          q, p, i, c, r - 1, o
    #define A_c_minus_1          q, p, i, c - 1, r, o
    #define A_i_minus_1          q, p + P - 1, i - 1, c, r, o
    #define A_p_minus_1          q + Q - 1, p - 1, i, c, r, o
    #define A_q_minus_1          q - 1, p, i, c, r, o

    // UREs
    switch (DESIGN) {
        case 6: // Space: C, Q. X transfers along diagonals, W along C, intermediate results along Q.
            X(A) = select(c == 0 || q == Q - 1, x(c + q, r + p, i), X(A_q_plus_1_c_minus_1));
            W(A) = select(c == 0, w(q, p, i, o), W(A_c_minus_1));
            break;
        default:
            assert(false);
    }
    Z(A) = select(q == 0, select(p == 0, select(i == 0, 0, Z(A_i_minus_1)), Z(A_p_minus_1)), Z(A_q_minus_1)) + X(A) * W(A);
    z(c, r, o) = select(q == Q - 1 && p == P - 1 && i == I - 1, Z(A));
    X.merge_ures(W, Z, z)          // Put all the UREs into the same loop nest
        .set_bounds(q, 0, Q,
                    p, 0, P,
                    i, 0, I)
        .set_bounds(c, 0, C,
                    r, 0, R,
                    o, 0, O);

    // Generate input and run.
    Buffer<int> inx = new_data_3D<int, C + 2, R + 2, I>(SEQUENTIAL); //or RANDOM
    Buffer<int> inw = new_data_4D<int, Q, P, I, O>(SEQUENTIAL);      //or RANDOM
    x.set(inx);
    w.set(inw);
}

int main(void) {
    #define A0  q0, p0, i0, c0, r0, o0

    Var  A0;
    Func X0(Int(32), {A0});
    Func W0(Int(32), {A0});
    Func Z0(Int(32), {A0});
    Func z0;

    kernel(X0, W0, Z0, z0, A0);

    Target target = get_host_target();
    Buffer<int> golden = z0.realize({C, R, O}, target);

    // Space-time transform.
    Var  q, p, i, c, r, o;
    Func X(Int(32), {q, p, i, c, r, o});
    Func W(Int(32), {q, p, i, c, r, o});
    Func Z(Int(32), {q, p, i, c, r, o});
    Func z;

    kernel(X, W, Z, z, q, p, i, c, r, o);

    Buffer<int> out;
    switch (DESIGN) {
    case 6: // c and q are space loops
        X.reorder(q, c, p, i, r, o)
         .space_time_transform(q, c);
        break;
    default:
        assert(false);
    }

    // Check correctness.
    out = z.realize({C, R, O}, target);
    check_equal_3D<int>(golden, out);
    cout << "Success!\n";
    return 0;
}
