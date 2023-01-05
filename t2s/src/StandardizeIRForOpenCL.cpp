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
#include "CodeGen_Internal.h"
#include "IR.h"
#include "IRMutator.h"
#include "IROperator.h"

#include "./StandardizeIRForOpenCL.h"

namespace Halide {
namespace Internal {

class OpenCLStandardizer : public IRMutator {
  public:
    OpenCLStandardizer() : in_opencl_device_kernel(false) {}

  private:
    using IRMutator::visit;

    bool in_opencl_device_kernel;

    Stmt visit(const For* op) override {
        if (op->device_api == DeviceAPI::OpenCL && ends_with(op->name, ".run_on_device")) {
            in_opencl_device_kernel = true;

            Stmt body = mutate(op->body);
            Stmt loop = For::make(op->name, mutate(op->min), mutate(op->extent), op->for_type, op->device_api, body);

            in_opencl_device_kernel = false;
            return loop;
        }
        return IRMutator::visit(op);
    }

    Expr visit(const Call* op) override {
        if (in_opencl_device_kernel) {
            if (op->is_intrinsic(Call::shift_left) || op->is_intrinsic(Call::shift_right)) {
                // Some OpenCL implementations forbid mixing signed-and-unsigned shift values;
                // if the RHS is uint, quietly cast it back to int if the LHS is int
                if (op->args[0].type().is_int() && op->args[1].type().is_uint()) {
                    Type t = op->args[0].type().with_code(halide_type_int);
                    Expr e = Call::make(op->type, op->name, {mutate(op->args[0]), cast(t, mutate(op->args[1]))}, op->call_type);
                    return e;
                }
            }
            return IRMutator::visit(op);
        }
        return op;
    }

    Expr visit(const Div *op) override {
        if (in_opencl_device_kernel) {
            int bits;
            char* ediv = getenv("EUCLIDEAN_DIVISION");
            if (is_const_power_of_two_integer(op->b, &bits)) {
                Expr e = Call::make(op->type, Internal::Call::shift_right, {mutate(op->a), make_const(op->a.type(), bits)}, Internal::Call::PureIntrinsic);
                return e;
            } else if (ediv && op->type.is_int()) {
                return lower_euclidean_div(mutate(op->a), mutate(op->b));
            }
            return IRMutator::visit(op);
        }
        return op;
    }

    Expr visit(const Mod *op) override {
        if (in_opencl_device_kernel) {
            int bits;
            char* ediv = getenv("EUCLIDEAN_DIVISION");
            if (is_const_power_of_two_integer(op->b, &bits)) {
                Expr e = Call::make(op->type, Internal::Call::bitwise_and, {mutate(op->a), make_const(op->a.type(), (1 << bits) - 1)}, Internal::Call::PureIntrinsic);
                return e;
            } else if (ediv && op->type.is_int()) {
                return lower_euclidean_mod(mutate(op->a), mutate(op->b));
            }
            return IRMutator::visit(op);
        }
        return op;
    }

};
Stmt standardize_ir_for_opencl_code_gen(Stmt s) {
    OpenCLStandardizer standardizer;
    s = standardizer.mutate(s);
    return s;
}

} // Internal
} // Halide
