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
// On linux, you can compile and run it like so:
// g++ simple-loop-4.cpp -g -I ../util  -I ../../../../Halide/include -L ../../../../Halide/bin -lHalide -lpthread -ldl -std=c++11 -DSIZE=10  -DVERBOSE_DEBUG -DPLACE0=Place::Host -DPLACE1=Place::Host
// LD_LIBRARY_PATH=../../../../Halide/bin ./a.out
// You can change SIZE to be arbitrary unsigned integer, and remove -DVERBOSE_DEBUG for cleaner output without details, and set PLACE* either as Place::Host or Place::Device.

#include "util.h"

int main(void) {
  // A simple 2-D loop
  Var i, j;
  Func A(Int(32), {i, j}), B;
  A(i, j) = i + j;
  B(i) = select(j == SIZE-1, A(i, j));

  // Space-time-transform
  A.merge_ures(B)
   .set_bounds(i, 0, SIZE)
   .set_bounds(j, 0, SIZE);

  // Compile and run
  Target target = get_host_target();
  Buffer<int> golden = B.realize({SIZE}, target);

  // A simple 2-D loop
  Var i2, j2;
  Func A2(Int(32), {i2, j2}), B2;
  A2(i2, j2) = i2 + j2;
  B2(i2) = select(j2 == SIZE-1, A2(i2, j2));

  // Space-time-transform
  A2.merge_ures(B2)
    .set_bounds(i2, 0, SIZE)
    .set_bounds(j2, 0, SIZE);

  Func consumer(PLACE0);
  B2.isolate_consumer(consumer);

  A2.space_time_transform({i2}, {2});

  Buffer<int> result = consumer.realize({SIZE}, target);

  // Check correctness.
  check_equal<int>(golden, result);
  cout << "Success!\n";
}
