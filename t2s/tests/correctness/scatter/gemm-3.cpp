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
// 5 designs from Jason Cong and Jie  Wang. PolySA: polyhedral-based systolic array auto-compilation. ICCAD '18.

#include "util.h"

// A 2*3 matrix times 3*2 matrix
#define I 2
#define J 2
#define K 12

int main(void) {
    // Input parameters: a and b are 2D matrices.
    ImageParam a(type_of<int>(), 2);
    ImageParam b(type_of<int>(), 2);

    // Macros: for convenient use.
    #define P0             i0, j0, k0
    #define P0_i_minus_1   i0 - 1, j0, k0
    #define P0_j_minus_1   i0, j0 - 1, k0
    #define P0_k_minus_1   i0, j0, k0 - 1
    #define P             i, j, k
    #define P_i_minus_1   i - 1, j, k
    #define P_j_minus_1   i, j - 1, k
    #define P_k_minus_1   i, j, k - 1

    // UREs
    Var  i0, j0, k0;
    Func A0(Int(32), {P0}, PLACE), B0(Int(32), {P0}, PLACE), C0(Int(32), {P0}, PLACE), c0(PLACE);
    A0(P0)     = select(j0 == 0, a(i0, k0), A0(P0_j_minus_1));
    B0(P0)     = select(i0 == 0, b(k0, j0), B0(P0_i_minus_1));
    C0(P0)     = select(k0 == 0, 0, C0(P0_k_minus_1)) + A0(P0) * B0(P0);
    c0(i0, j0) = select(k0 == K - 1, C0(P0));
    A0.merge_ures(B0, C0, c0)      // Put all the UREs into the same loop nest
      .reorder(i0, j0, k0)         // The loop nest has loop i, k, j starting from the innermost level.
      .set_bounds(i0, 0, I,
                  k0, 0, K,
                  j0, 0, J);

    // Generate input and run.
    Buffer<int> ina = new_data_2d<int, I, K>(SEQUENTIAL); //or RANDOM
    Buffer<int> inb = new_data_2d<int, K, J>(SEQUENTIAL); //or RANDOM
    a.set(ina);
    b.set(inb);
    Target target = get_host_target();
#ifdef SCATTER
    target.set_features({Target::IntelFPGA, Target::Debug});
#endif
    Buffer<int> golden = c0.realize({I, J}, target);

    // UREs
    Var  i, j, k;
    Func A(Int(32), {P}, PLACE), B(Int(32), {P}, PLACE), C(Int(32), {P}, PLACE), c(PLACE);
    A(P)    = select(j == 0, a(i, k), A(P_j_minus_1));
    B(P)    = select(i == 0, b(k, j), B(P_i_minus_1));
    C(P)    = select(k == 0, 0, C(P_k_minus_1)) + A(P) * B(P);
    c(i, j) = select(k == K - 1, C(P));
    A.merge_ures(B, C, c)      // Put all the UREs into the same loop nest
     .set_bounds(i, 0, I,
                 k, 0, K,
                 j, 0, J);

    // Space-time transform.
    Buffer<int> out;
    switch (DESIGN) {
    case 1:
        A.reorder(i, k, j).space_time_transform(i, k); // Loop i and k are space loops.
        A.vectorize(i);
        break;
    case 2:
        A.reorder(k, j, i).space_time_transform(k, j); // Loop k and j are space loops.
        A.vectorize(j);
        break;
    case 3:
        A.reorder(j, i, k).space_time_transform(j, i); // Loop j and i are space loops.
        A.vectorize(j);
        break;
    case 4:
        A.reorder(j, i, k).space_time_transform(j, i); // First projection: project k loop away
        A.space_time_transform(j);                     // Second projection: project i loop away
        A.vectorize(j);
        break;
    case 5:
        A.reorder(i, j, k).space_time_transform(i, j); // First projection: project k loop away
        A.space_time_transform(i);                     // Second projection: project i loop away
        A.vectorize(i);
        break;
    default:
        assert(false);
    }

#ifdef SCATTER
    Func A_feeder(Place::Device), A_loader(Place::Device);
    Func B_feeder(Place::Device), B_loader(Place::Device);
    A.isolate_producer_chain(a, A_loader, A_feeder);
    A.isolate_producer_chain(b, B_loader, B_feeder);
    switch (DESIGN) {
    case 1:
        A_feeder.scatter(A_loader, i, STRATEGY);
        B_feeder.scatter(B_loader, k, STRATEGY);
        break;
    case 2:
        A_feeder.scatter(A_loader, k, STRATEGY);       // vectorize scatter loop
        B_feeder.scatter(B_loader, k, STRATEGY);
        break;
    case 3:
        A_feeder.scatter(A_loader, i, STRATEGY);
        B_feeder.scatter(B_loader, j, STRATEGY);
        break;
    case 4:
        B_feeder.scatter(B_loader, j, STRATEGY);
        break;
    default:
        A_feeder.scatter(A_loader, i, STRATEGY);
    }
#endif

    // Check correctness.
    remove(getenv("BITSTREAM"));
    out = c.realize({I, J}, target);
    check_equal<int>(golden, out);
    cout << "Success!\n";
    return 0;
}
