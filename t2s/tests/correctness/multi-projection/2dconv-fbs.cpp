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

#define P       2
#define Q       2
#define IIC     16
#define IC      1
#define OC      1
#define IIR     16
#define IR      1
#define OR      1

int main(void) {
    ImageParam x(Int(32), 2), w(Int(32), 2);
    x.set(new_matrix<int, IIC+Q-1, IIC+P-1>(VALUES::SEQUENTIAL));
    w.set(new_matrix<int, P, Q>(VALUES::SEQUENTIAL));

#define ARGS        p, q, iic, iir, ic, ir, oc, or1
#define ARGS2       p2, q2, iic2, iir2, ic2, ir2, oc2, or2
#define X_ARGS      p, q-1, iic-1, iir, ic, ir, oc, or1
#define X2_ARGS     p2, q2-1, iic2-1, iir2, ic2, ir2, oc2, or2
#define Z_ARGS      p+P-1, q-1, iic, iir, ic, ir, oc, or1
#define Z2_ARGS     p2+P-1, q2-1, iic2, iir2, ic2, ir2, oc2, or2
#define Z_ARGS_2    p-1, q, iic, iir, ic, ir, oc, or1
#define Z2_ARGS_2   p2-1, q2, iic2, iir2, ic2, ir2, oc2, or2
#define OUT         iic, iir, ic, ir, oc, or1
#define OUT2        iic2, iir2, ic2, ir2, oc2, or2
#define ORD         iic, p, q, iir, ic, ir, oc, or1
#define riic        IIC-1-iic
#define riic2       IIC-1-iic2

    Var ARGS2;
    Func Z2(Int(32), {ARGS2});
    Func X2(Int(32), {ARGS2});
    Func W2, out2;
    X2(ARGS2) = select(iic2 == 0 || q2 == 0,
                    x(riic2+q2, iir2+p2), X2(X2_ARGS));
    W2(ARGS2) = w(q2, p2);
    Z2(ARGS2) = select(p2==0 && q2==0, 0,
                    select(p2==0, Z2(Z2_ARGS), Z2(Z2_ARGS_2)))
                        + X2(ARGS2) * W2(ARGS2);
    out2(OUT2) = select(p2==P-1 && q2==Q-1, Z2(ARGS2));
    X2.merge_ures(W2, Z2, out2)
      .set_bounds(p2, 0, P)
      .set_bounds(q2, 0, Q)
      .set_bounds(iic2, 0, IIC)
      .set_bounds(ic2, 0, IC)
      .set_bounds(oc2, 0, OC)
      .set_bounds(iir2, 0, IIR)
      .set_bounds(ir2, 0, IR)
      .set_bounds(or2, 0, OR);
    // Compile and run
    Target target = get_host_target();
    Buffer<int> golden = out2.realize({IIC, IIR, IC, IR, OC, OR}, target);

    Var ARGS;
    Func Z(Int(32), {ARGS});
    Func X(Int(32), {ARGS});
    Func W, out;
    X(ARGS) = select(iic == 0 || q == 0,
                    x(riic+q, iir+p), X(X_ARGS));
    W(ARGS) = w(q, p);
    Z(ARGS) = select(p==0 && q==0, 0,
                select(p==0, Z(Z_ARGS), Z(Z_ARGS_2)))
                    + X(ARGS) * W(ARGS);
    out(OUT) = select(p==P-1 && q==Q-1, Z(ARGS));

    Var k, t;
    // Compile and run
    X.merge_ures(W, Z, out)
     .set_bounds(p, 0, P)
     .set_bounds(q, 0, Q)
     .set_bounds(iic, 0, IIC)
     .set_bounds(ic, 0, IC)
     .set_bounds(oc, 0, OC)
     .set_bounds(iir, 0, IIR)
     .set_bounds(ir, 0, IR)
     .set_bounds(or1, 0, OR)
     .reorder(ORD)
     .space_time_transform({iic, p, q},
                           {k, t},
                          {{1, 0, 0},       // k = iic
                           {0, 1, P}},      // t = p + P*q
                          {{p, t%P},
                           {q, t/P},
                           {iic, k}},
                          SpaceTimeTransform::NoCheckTime);

    Buffer<int> result = out.realize({IIC, IIR, IC, IR, OC, OR}, target);
    check_equal_2D<int>(golden, result);
    cout << "Success!\n";
    return 0;
}
