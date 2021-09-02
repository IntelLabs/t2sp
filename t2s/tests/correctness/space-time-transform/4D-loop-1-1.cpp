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

int main(void) {
  // A simple 3-D loop
  Var i, j, k, l;
  Func A(Int(32), {i, j, k, l}), B(Int(32), {i, j, k, l}), C(Int(32), {i, j, k, l}), D(Int(32), {i, j, k});
  A(i, j, k, l) = select(i == 0, j, A(i-1, j, k, l));
  B(i, j, k, l) = select(j == 0, i, B(i, j-1, k, l));
  C(i, j, k, l) = select(k == 0, 0, A(i, j, k, l) + B(i, j, k, l) + C(i, j, k-1, l));
  D(i, j, k) = select(l == 0, C(i, j, k, l));

  // Space-time-transform
  A.merge_ures(B, C, D)
   .set_bounds(i, 0, SIZE)
   .set_bounds(j, 0, SIZE)
   .set_bounds(k, 0, SIZE)
   .set_bounds(l, 0, SIZE);

  // Compile and run
  Target target = get_host_target();
  Buffer<int> golden = D.realize({SIZE, SIZE, SIZE}, target);

  // A simple 2-D loop
  Var i2, j2, k2, l2;
  Func A2(Int(32), {i2, j2, k2, l2}), B2(Int(32), {i2, j2, k2, l2}), C2(Int(32), {i2, j2, k2, l2}), D2(Int(32), {i2, j2, k2});
  A2(i2, j2, k2, l2) = select(i2 == 0, j2, A2(i2-1, j2, k2, l2));
  B2(i2, j2, k2, l2) = select(j2 == 0, i2, B2(i2, j2-1, k2, l2));
  C2(i2, j2, k2, l2) = select(k2 == 0, 0, A2(i2, j2, k2, l2) + B2(i2, j2, k2, l2) + C2(i2, j2, k2-1, l2));
  D2(i2, j2, k2) = select(l2 == 0, C2(i2, j2, k2, l2));

  // Space-time-transform
  A2.merge_ures(B2, C2, D2)
    .set_bounds(i2, 0, SIZE)
    .set_bounds(j2, 0, SIZE)
    .set_bounds(k2, 0, SIZE)
    .set_bounds(l2, 0, SIZE)
    .space_time_transform({i2}, {2});

  Buffer<int> result = D2.realize({SIZE, SIZE, SIZE}, target);

  // Check correctness.
  check_equal_3D<int>(golden, result);
  cout << "Success!\n";
}

