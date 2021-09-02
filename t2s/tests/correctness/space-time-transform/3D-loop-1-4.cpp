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
  Var i, j, k;
  Func A(Int(32), {i, j, k}), B(Int(32), {i, j, k});
  A(i, j, k) = 10;
  B(i, j, k) = A(i, j, k);

  // Space-time-transform
  A.merge_ures(B)
   .set_bounds(i, 0, SIZE)
   .set_bounds(j, 0, SIZE)
   .set_bounds(k, 0, SIZE);

  // Compile and run
  Target target = get_host_target();
  Buffer<int> golden = B.realize({SIZE, SIZE, SIZE}, target);

  // A simple 2-D loop
  Var i2, j2, k2;
  Func A2(Int(32), {i2, j2, k2}), B2(Int(32), {i2, j2, k2});
  A2(i2, j2, k2) = 10;
  B2(i2, j2, k2) = A2(i2, j2, k2);

  // Space-time-transform
  A2.merge_ures(B2)
    .set_bounds(i2, 0, SIZE)
    .set_bounds(j2, 0, SIZE)
    .set_bounds(k2, 0, SIZE)
    .space_time_transform(i2, j2);

  Buffer<int> result = B2.realize({SIZE, SIZE, SIZE}, target);

  // Check correctness.
  check_equal_3D<int>(golden, result);
  cout << "Success!\n";
}
