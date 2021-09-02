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

#include "util.h"

// Parameters from Table 2 of the paper
#define O 4     // OUT_NUM
#define I 2     // IN_NUM
#define R 3     // OUT_IMG_H
#define C 3     // OUT_IMG_W
#define P 3
#define Q 3

int main() {
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
    //    (-Q+1, -P+1, 1, 0, 0, 0) // When A_i_minus_1 below is used, q=p=0, that is why the distances at q and p are -Q+1 and -P+1, respectively
    //    (-Q+1, 1, 0, 0, 0, 0)    // When A_p_minus_1 below is used, q=0, that is why distances at q is -Q+1
    //    (1, 0, 0, 0, 0, 0)
    // These dependence vectors are positive (read from right to left, i.e. from the outermost to the innermost loop).
    #define A_p_plus_1_r_minus_1 q, p + 1, i, c, r - 1, o
    #define A_q_plus_1_c_minus_1 q + 1, p, i, c - 1, r, o
    #define A_o_minus_1          q, p, i, c, r, o - 1
    #define A_r_minus_1          q, p, i, c, r - 1, o
    #define A_c_minus_1          q, p, i, c - 1, r, o
    #define A_i_minus_1          Q - 1, P - 1, i - 1, c, r, o
    #define A_p_minus_1          Q - 1, p - 1, i, c, r, o
    #define A_q_minus_1          q - 1, p, i, c, r, o

    // Space-time transform.
    Var  A;
    Func X(Int(32), {A});
    Func W(Int(32), {A});
    Func Z(Int(32), {A});
    Func z;

    // UREs
    switch (DESIGN) {
        case 1: // Space: R, C. X and W transfer along R
            // The limitation of the UREs below: P is small, and therefore, p == P - 1 is true
            // frequently, and thus x is loaded from memory frequently. This inefficiency will be
            // solved in cnn-2-p.cpp.
            X(A) = select(p == P - 1 || r == 0, x(c + q, r + p, i), X(A_p_plus_1_r_minus_1));
            W(A) = select(r == 0, w(q, p, i, o), W(A_r_minus_1));
            break;
        case 2: // Space: R, C. X transfers along R, W along C
            X(A) = select(p == P - 1 || r == 0, x(c + q, r + p, i), X(A_p_plus_1_r_minus_1));
            W(A) = select(c == 0, w(q, p, i, o), W(A_c_minus_1));
            break;
        case 3: // Space: R, P. Intermediate results transfer along P, W along R, X along diagonals.
            X(A) = select(p == P - 1 || r == 0, x(c + q, r + p, i), X(A_p_plus_1_r_minus_1));
            W(A) = select(r == 0, w(q, p, i, o), W(A_r_minus_1));
            break;
        case 4: // Space: R, Q. X and W transfer along R, intermediate results along Q.
            X(A) = select(p == P - 1 || r == 0, x(c + q, r + p, i), X(A_p_plus_1_r_minus_1));
            W(A) = select(r == 0, w(q, p, i, o), W(A_r_minus_1));
            break;
        case 5: // Space: R, P. X and W transfer along R, intermediate results along P.
            X(A) = select(p == P - 1 || r == 0, x(c + q, r + p, i), X(A_p_plus_1_r_minus_1));
            W(A) = select(r == 0, w(q, p, i, o), W(A_r_minus_1));
            break;
        case 7: // Space: O, R. X transfers along O, W along R.
            X(A) = select(o == 0, x(c + q, r + p, i), X(A_o_minus_1));
            W(A) = select(r == 0, w(q, p, i, o), W(A_r_minus_1));
            break;
        case 8: // Space: O, C. X transfers along O, W along C.
            X(A) = select(o == 0, x(c + q, r + p, i), X(A_o_minus_1));
            W(A) = select(c == 0, w(q, p, i, o), W(A_c_minus_1));
            break;
        case 9: // Space: O, P. X and W transfer along O, intermediate results along P.
            X(A) = select(o == 0, x(c + q, r + p, i), X(A_o_minus_1));
            W(A) = select(o == 0, w(q, p, i, o), W(A_o_minus_1));
            break;
        case 10: // Space: O, P. X transfers along O, W along P. intermediate results along P.
            X(A) = select(o == 0, x(c + q, r + p, i), X(A_o_minus_1));
            W(A) = select(p == 0, w(q, p, i, o), W(A_p_minus_1));
            break;
        case 11: // Space: O, Q. X transfers along O, W along Q. intermediate results along Q.
            X(A) = select(o == 0, x(c + q, r + p, i), X(A_o_minus_1));
            W(A) = select(q == 0, w(q, p, i, o), W(A_q_minus_1));
            break;
        case 12: // Space: O, Q. X and W transfer along O, intermediate results along Q.
            X(A) = select(o == 0, x(c + q, r + p, i), X(A_o_minus_1));
            W(A) = select(o == 0, w(q, p, i, o), W(A_o_minus_1));
            break;
        case 13: // Space: O, I. X and W transfer along O, intermediate results along I.
            X(A) = select(o == 0, x(c + q, r + p, i), X(A_o_minus_1));
            W(A) = select(o == 0, w(q, p, i, o), W(A_o_minus_1));
            break;
        case 14: // Space: O, I. X transfers along O, W long I, intermediate results along I.
            X(A) = select(o == 0, x(c + q, r + p, i), X(A_o_minus_1));
            W(A) = select(i == 0, w(q, p, i, o), W(A_i_minus_1));
            break;
        case 15: // Space: R, I. X and W transfer along R, intermediate results along I.
            X(A) = select(p == P - 1 || r == 0, x(c + q, r + p, i), X(A_p_plus_1_r_minus_1));
            W(A) = select(r == 0, w(q, p, i, o), W(A_r_minus_1));
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

    try {
        switch (DESIGN) {
        case 1:
        case 2: // r and c are space loops
            X.reorder(c, r, q, p, i, o) // Move c and r to innermost. Dependence (0, -1, 0, 0, 1, 0)==> (0, 1, 0, -1, 0, 0) negative (read from right to left)
             .space_time_transform(c, r);
            break;
        case 3: // r and p are space loops
        case 5:
            X.reorder(p, r, q, i, c, o) // Move p and r to innermost. Dependence (-Q+1, 1, 0, 0, 0, 0)==> (1, 0, -Q+1, 0, 0, 0) negative (read from right to left)
             .space_time_transform(p, r);
            break;
        case 4: // r and q are space loops
            X.reorder(q, r, p, i, c, o) // Dependence (0, -1, 0, 0, 1, 0) ==> (0, 1, -1, 0, 0, 0) negative (read from right to left)
             .space_time_transform(q, r);
            break;
        case 7: // o and r are space loops
            X.reorder(r, o, q, p, i, c)   // (0, -1, 0, 0, 1, 0) ==> (1, 0, 0, -1, 0, 0) negative (read from right to left)
             .space_time_transform(r, o);
            break;
        case 8: // c and o are space loops
            X.reorder(c, o, q, p, i, r) // (-1, 0, 0, 1, 0, 0) ==> (1, 0, -1, 0, 0, 0) negative (read from right to left)
             .space_time_transform(c, o);
            break;
        case 9: // p and o are space loops
        case 10:
            X.reorder(p, o, q, i, c, r) // (-Q+1, 1, 0, 0, 0, 0) ==> (1, 0, -Q+1, 0, 0, 0, 0) negative (read from right to left)
             .space_time_transform(p, o);
            break;
        case 11: // o and q are space loops
        case 12:
            X.reorder(q, o, p, i, c, r) // (0, -1, 0, 0, 1, 0)==>(0, 0, -1, 0, 0, 1). Same issue for space-time transform as in case 3 and 5.
             .space_time_transform(q, o);
            break;
        case 13: // i and o are space loops
        case 14:
            X.reorder(i, o, q, p, c, r) // (-Q+1, -P+1, 1, 0, 0, 0)==>(0, 1, -Q+1, -P+1, 0, 0) negative (read from right to left)
             .space_time_transform(i, o);
            break;
        case 15: // i and r are space loops
            X.reorder(i, r, q, p, c, o) // (0, -1, 0, 0, 1, 0)==>(0, 1, 0, -1, 0, 0) negative (read from right to left)
             .space_time_transform(i, r);
            break;
        default:
            assert(false);
        }
        Target target = get_host_target();
        z.compile_jit(target);
    } catch (...) {
        // An error should be thrown for the negative dependence vectors
        cout << "Success!\n";
    }

    return 0;
}
