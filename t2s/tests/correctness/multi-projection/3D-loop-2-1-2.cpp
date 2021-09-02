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
#define SIZE 10

int main(void) {
  // A simple 3-D loop
  Var i, j, k;
  Func A(Int(32), {i, j, k}), B(Int(32), {i, j, k});
  Func C(Int(32), {i, j, k});
  A(i, j, k) = select(i == 0, j, A(i - 1, j, k));
  B(i, j, k) = select(j == 0, i, A(i, j, k) + B(i, j - 1, k));
  C(i, j, k) = B(i, j, k);

  // Space-time-transform
  A.merge_ures(B, C)
   .set_bounds(i, 0, SIZE)
   .set_bounds(j, 0, SIZE)
   .set_bounds(k, 0, SIZE);

  // Compile and run
  Target target = get_host_target();
  Buffer<int> golden = C.realize({SIZE, SIZE, SIZE}, target);

  // A simple 2-D loop
  Var i2, j2, k2;
  Func A2(Int(32), {i2, j2, k2}), B2(Int(32), {i2, j2, k2});
  Func C2(Int(32), {i2, j2, k2});
  A2(i2, j2, k2) = select(i2 == 0, j2, A2(i2 - 1, j2, k2));
  B2(i2, j2, k2) = select(j2 == 0, i2, A2(i2, j2, k2) + B2(i2, j2 - 1, k2));
  C2(i2, j2, k2) = B2(i2, j2, k2);

  // Space-time-transform
  A2.merge_ures(B2, C2)
    .set_bounds(i2, 0, SIZE)
    .set_bounds(j2, 0, SIZE)
    .set_bounds(k2, 0, SIZE);

#ifdef CASE1
  Var s1, s2, t;
  // d3 = [0 0 1], s3 = [0 0 1], p3 = [[1 0 0][0 1 0]]
  A2.space_time_transform({i2, j2, k2},     // the source loops
                          {s1, s2, t},      // the destination loops
                          {1, 0, 0,         // space loop: s1 = i2
                           0, 1, 0,         // space loop: s2 = j2
                           0, 0, 1},        // time loop: t = k2
                          {i2, s1,          // reverse transform
                           j2, s2,
                           k2, t});
#elif CASE2
  Var s1, s2, t;
  // d3 = [0 1 1], s3 = [0 0 1], p3 = [[1 0 0][0 1 -1]]
  A2.space_time_transform({i2, j2, k2},     // the source loops
                          {s1, s2, t},      // the destination loops
                          {1, 0, 0,         // space loop: s1 = i2
                           0, 1, -1,        // space loop: s2 = j2 - k2
                           0, 0, 1},        // time loop: t = k2
                          {i2, s1,          // reverse transform
                           k2, t,
                           j2, s2+k2});
#endif

  Buffer<int> result = C2.realize({SIZE, SIZE, SIZE}, target);
  // Check correctness.
  check_equal_3D<int>(golden, result);
  cout << "Success!\n";
}
