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
// This file is the same as cnn-1-p.cpp, except that the input data x are pre-loaded into PEs.

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

    #define A                     q, p, i, c, r, o
    // From these, you can see the dependence vectors deltaA:
    //    (0, 0, 0, 0, 0, 1)
    //    (0, 0, 0, 0  1, 0)
    //    (0, 0, 0, 1, 0, 0)
    //    (-Q+1, -P+1, 1, 0, 0, 0)  // This prevents i from moving innermost
    //    (-Q+1, 1, 0, 0, 0, 0)     // This prevents p from moving innermost
    //    (1, 0, 0, 0, 0, 0)
    // These dependence vectors are positive (read from right to left, i.e. from the outermost to the innermost loop).
    #define A_o_minus_1          q, p, i, c, r, o - 1
    #define A_r_minus_1          q, p, i, c, r - 1, o
    #define A_c_minus_1          q, p, i, c - 1, r, o
    #define A_i_minus_1          q + Q - 1, p + P - 1, i - 1, c, r, o
    #define A_p_minus_1          q + Q - 1, p - 1, i, c, r, o
    #define A_q_minus_1          q - 1, p, i, c, r, o

    // We will make Feeder to scatter x along R or O, depending on the design.
    // Inside a PE, X transfers along o.
    X(A) = select(o == 0, x(c + q, r + p, i), X(A_o_minus_1));
    switch (DESIGN) {
    case 1:  // Space: R, C. X and W transfers along R
    case 4:  // Space: R, Q. X and W transfers along R
    case 7:  // Space: O, R. X transfers along O, W along R.
        W(A) = select(r == 0, w(q, p, i, o), W(A_r_minus_1));
        break;
    case 2: // Space: R, C. X transfers along R, W along C
    case 8: // Space: O, C. X transfers along O, W along C.
        W(A) = select(c == 0, w(q, p, i, o), W(A_c_minus_1));
        break;
    case 11: // Space: O, Q. X transfers along O, W along Q. intermediate results along Q.
        W(A) = select(q == 0, w(q, p, i, o), W(A_q_minus_1));
        break;
    case 12: // Space: O, Q. X and W transfer along O, intermediate results along Q.
        W(A) = select(o == 0, w(q, p, i, o), W(A_o_minus_1));
        break;
    default:
        assert(false);
    }
    Z(A) = select(q == 0, select(p == 0, select(i == 0, 0, Z(A_i_minus_1)), Z(A_p_minus_1)), Z(A_q_minus_1)) + X(A) * W(A);
    z(c, r, o) = select(q == Q - 1 && p == P - 1 && i == I - 1, Z(A));
    X.merge_ures(W, Z, z)       // Put all the UREs into the same loop nest
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
    // UREs
    #define A0                     q0, p0, i0, c0, r0, o0
    Var  A0;
    Func X0(Int(32), {A0}), W0(Int(32), {A0}), Z0(Int(32), {A0}), z0;

    kernel(X0, W0, Z0, z0, A0);

    Target target = get_host_target();
    Buffer<int> golden = z0.realize(C, R, O, target);

    // UREs
    #define A                     q, p, i, c, r, o
    Var  A;
    Func X(Int(32), {A}), W(Int(32), {A}), Z(Int(32), {A}), z;

    kernel(X, W, Z, z, A);

    // Feeder PEs directly send input data to the systolic array, which will have a
    // buffer automatically built inside, because X(A_o_minus_1) will make the compiler
    // to allocate such a buffer between two consecutive o�s.
    // Loader, due to memory accesses, will be made to run asynchronously with Feeder.
    // Feeder, whoever, will run synchronously with the systolic array as both do not access memory.
    #define x_transfer_along_r {                         \
            Func Loader, Feeder;                         \
            z.isolate_producer_chain(x, Loader, Feeder); \
            Feeder.scatter(x, r); }
    #define x_transfer_along_o {                         \
            Func Loader, Feeder;                         \
            z.isolate_producer_chain(x, Loader, Feeder); \
            Feeder.scatter(x, o); }

    // Further optimization TODO: for the same r + p but with different
    // combinations of r and p, the same x is loaded more than once. Should
    // optimize Loader so that x is loaded once, and buffered in Feeder.

    // Space-time transform.
    Buffer<int> out;
    switch (DESIGN) {
    case 1:
    case 2: // c and r are space loops
        X.reorder(c, r, q, p, i, o)            // Put space loops to the innermost levels.
         .space_time_transform(c, r);
        //x_transfer_along_r;
        break;
    case 4: // q and r are space loops
        X.reorder(q, r, p, i, c, o)
         .space_time_transform(q, r);
        //x_transfer_along_r;
        break;
    case 7: // r and o are space loops
        X.reorder(r, o, q, p, i, c)
         .space_time_transform(r, o);
        //x_transfer_along_o;
        break;
    case 8: // c and o are space loops
        X.reorder(c, o, q, p, i, r)
         .space_time_transform(c, o);
        //x_transfer_along_o;
        break;
    case 11: // q and o are space loops
    case 12:
        X.reorder(q, o, p, i, c, r)
         .space_time_transform(q, o);
        //x_transfer_along_o;
        break;
    default:
        assert(false);
    }

    // Check correctness.
    out = z.realize(C, R, O, target);
    check_equal<int>(golden, out);
    cout << "Success!\n";
    return 0;
}
