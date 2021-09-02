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
  // A simple 2-D loop
  Var i, j;
  Func A(Int(32), {i, j}), B(Int(32), {i, j}), C(Int(32), {i, j});
  A(i, j) = select(i == 0, j, A(i-1, j));
  B(i, j) = select(j == 0, i, B(i, j-1));
  C(i, j) = A(i, j) + B(i, j);

  // Space-time-transform
  A.merge_ures(B, C)
   .set_bounds(i, 0, SIZE)
   .set_bounds(j, 0, SIZE);

  // Compile and run
  Target target = get_host_target();
  Buffer<int> golden = C.realize({SIZE, SIZE}, target);

  // A simple 2-D loop
  Var i2, j2;
  Func A2(Int(32), {i2, j2}), B2(Int(32), {i2, j2}), C2(Int(32), {i2, j2});
  A2(i2, j2) = select(i2 == 0, j2, A2(i2-1, j2));
  B2(i2, j2) = select(j2 == 0, i2, B2(i2, j2-1));
  C2(i2, j2) = A2(i2, j2) + B2(i2, j2);

  // Space-time-transform
  A2.merge_ures(B2, C2)
    .set_bounds(i2, 0, SIZE)
    .set_bounds(j2, 0, SIZE)
    .space_time_transform(i2);

  Buffer<int> result = C2.realize({SIZE, SIZE}, target);

  // Check correctness.
  check_equal_2D<int>(golden, result);
  cout << "Success!\n";
}
