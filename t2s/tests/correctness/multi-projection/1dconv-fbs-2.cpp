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
#include "Halide.h"
#include "util.h"

using namespace Halide;

#define IIC 16
#define IC  1
#define OC  1
#define Q   5

int main(void) {
    ImageParam x(Int(32), 1), w(Int(32), 1);
    x.set(new_data<int, Q+IIC-1>(VALUES::SEQUENTIAL));
    w.set(new_data<int, Q>(VALUES::SEQUENTIAL));

    Var q2, oc2, ic2, iic2;
    Func Z2(Int(32), {q2, iic2, ic2, oc2});
    Func X2(Int(32), {q2, iic2, ic2, oc2});
    Func W2(Int(32), {q2, iic2, ic2, oc2});
    Func out2;
    X2(q2, iic2, ic2, oc2) = select(iic2 == IIC/2-1 || q2 < 2,
                                    x(q2 + 2*iic2 + ic2*IIC + oc2*IIC*IC),
                                    X2(q2-2, iic2+1, ic2, oc2));
    W2(q2, iic2, ic2, oc2) = w(q2);
    Z2(q2, iic2, ic2, oc2) = select(q2 == 0, 0,
                                            Z2(q2 - 1, iic2, ic2, oc2)) 
                                            + X2(q2, iic2, ic2, oc2)
                                            * W2(q2, iic2, ic2, oc2);
    out2(iic2, ic2, oc2) = select(q2 == Q - 1, Z2(q2, iic2, ic2, oc2));
    X2.merge_ures(W2, Z2, out2)
      .set_bounds(oc2, 0, OC)
      .set_bounds(ic2, 0, IC)
      .set_bounds(iic2, 0, IIC/2)
      .set_bounds(q2, 0, Q)
      .reorder(iic2, q2, ic2, oc2);
    // Compile and run
    Target target = get_host_target();
    Buffer<int> golden = out2.realize({IIC/2, IC, OC}, target);

    Var q, oc, ic, iic;
    Func Z(Int(32), {q, iic, ic, oc});
    Func X(Int(32), {q, iic, ic, oc});
    Func W(Int(32), {q, iic, ic, oc});
    Func out;
    X(q, iic, ic, oc) = select(iic == IIC/2-1 || q < 2,
                                x(q + 2*iic + ic*IIC + oc*IIC*IC),
                                X(q-2, iic+1, ic, oc));
    W(q, iic, ic, oc) = w(q);
    Z(q, iic, ic, oc) = select(q == 0, 0, Z(q-1, iic, ic, oc))
                                        + X(q, iic, ic, oc)
                                        * W(q, iic, ic, oc);
    out(iic, ic, oc) = select(q == Q-1, Z(q, iic, ic, oc));

    Var k, t;
    // Compile and run
    X.merge_ures(W, Z, out)
     .set_bounds(oc, 0, OC)
     .set_bounds(ic, 0, IC)
     .set_bounds(iic, 0, IIC/2)
     .set_bounds(q, 0, Q)
     .reorder(iic, q, ic, oc)
     .space_time_transform({iic, q},
                           {k, t},
                          {{1, 0},
                           {0, 1}},
                          {{iic, k},
                           {q, t}},
                           SpaceTimeTransform::NoCheckTime);

    Buffer<int> result = out.realize({IIC/2, IC, OC}, target);
    check_equal_2D<int>(golden, result);
    cout << "Success!\n";
    return 0;
}
