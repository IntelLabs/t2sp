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
#include "util.h"
#define P 1
#define N 2

int main(void) {
  Var i, j, u, v;
  Func E1(Int(32), {i, j, u, v});
  Func E2(Int(32), {i, j, u, v});
  Func E3(Int(32), {i, j, u, v});
  Func E4(Int(32), {u, v});
  ImageParam r(Int(32), 2), s(Int(32), 2);

  E1(i, j, u, v) = select((j==0) && (v==P), s(i, j), E1(i, j-1, u, v+1));
  E2(i, j, u, v) = select(v == P, r(i, j), E2(i, j, u, v+1));
  E3(i, j, u, v) = select(j == 0, 0, E3(i, j-1, u, v) + abs(E1(i, j, u, v) - E2(i, j, u ,v)));
  E4(u, v) = select(i == N-1 && j == N-1, E3(i, j, u, v));

  E1.merge_ures(E2, E3, E4)
    .set_bounds(i, 0, N)
    .set_bounds(j, 0, N)
    .set_bounds(u, -P, P+1)
    .set_bounds(v, -P, P+1);

  Buffer<int> inr = new_data_2d<int, N, N>(SEQUENTIAL); //or RANDOM
  Buffer<int> ins = new_data_2d<int, N, N>(SEQUENTIAL); //or RANDOM
  r.set(inr);
  s.set(ins);

  // Compile and run
  Target target = get_host_target();
  Buffer<int> golden = E4.realize({2*P+1, 2*P+1}, target);

  // Space-time-transform
  Var i2, j2, u2, v2;
  Func E12(Int(32), {i2, j2, u2, v2});
  Func E22(Int(32), {i2, j2, u2, v2});
  Func E32(Int(32), {i2, j2, u2, v2});
  Func E42(Int(32), {u2, v2});

  E12(i2, j2, u2, v2) = select((j2==0) && (v2==P), s(i2, j2), E12(i2, j2-1, u2, v2+1));
  E22(i2, j2, u2, v2) = select(v2 == P, r(i2, j2), E22(i2, j2, u2, v2+1));
  E32(i2, j2, u2, v2) = select(j == 0, 0, E32(i2, j2-1, u2, v2) + abs(E12(i2, j2, u2, v2) - E22(i2, j2, u2 ,v2)));
  E42(u2, v2) = select(i == N-1 && j == N-1, E32(i2, j2, u2, v2));

  E12.merge_ures(E22, E32, E42)
     .set_bounds(i2, 0, N)
     .set_bounds(j2, 0, N)
     .set_bounds(u2, -P, P+1)
     .set_bounds(v2, -P, P+1);

  // d4 = [0 0 -1 0], s4 = [0 0 -1 0]
  // p4 = [[1 0 0 0][0 1 0 0][0 0 0 -1]]
  // d3 = [0 0 1], s3 = [1 0 1]
  // p3 = [[1 0 0][0 1 0]]
  Var s1, s2, t;
  E12.space_time_transform({i2, j2, u2, v2},    // the source loops
                          {s1, s2, t},          // the destination loops
                          {1, 0, 0, 0,          // space loop: s1 = i2, s2 = j2
                           0, 1, 0, 0,
                           1, 0, -2*P-1, -1},   // time loop: t = i2 + (-2p-1)*u2 - v2
                          {i2, s1,
                          j2, s2,               // reverse transform
                          v2, (i2-t)%(2*P+1),
                          u2, (i2-t)/(2*P+1)});

  Buffer<int> result = E42.realize({2*P+1, 2*P+1}, target);
  // Check correctness.
  check_equal_2D<int>(golden, result);
  cout << "Success!\n";
}
