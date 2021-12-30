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
  Var i2, j2, k2;
  Func A2(Int(32), {i2, j2, k2}), B2(Int(32), {i2, j2, k2});
  A2(i2, j2, k2) = 10;
  B2(i2, j2, k2) = A2(i2, j2, k2);

  // Space-time-transform
  A2.merge_ures(B2)
    .set_bounds(i2, 0, SIZE)
    .set_bounds(j2, 0, SIZE)
    .set_bounds(k2, 0, SIZE);

  // Space loops should be adjacent to each other.
  A2.space_time_transform(i2, k2);
  cout << "Success!\n";
  return 0;
}
