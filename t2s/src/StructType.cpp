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
#include "./StructType.h"

namespace Halide {

using namespace Internal;

namespace Internal {
    // Initialize the static members of the GeneratedStructType class
    vector<pair<Type, int>> GeneratedStructType::vectors;
    vector<pair<string, vector<Type>>> GeneratedStructType::structs;
    halide_handle_cplusplus_type       GeneratedStructType::dummy = halide_handle_cplusplus_type(
        halide_cplusplus_type_name(halide_cplusplus_type_name::Struct, "CGS"));

    Type generate_struct(const vector<Type> &field_types) {
        // Generate and record a new struct type
        size_t index;
        GeneratedStructType struct_type(field_types, index);

        // Return a halide type to represent the generated struct type
        // In the Halide type system, a type has one of the following type code: halide_type_int/unit/float/handle/bfloat,
        // And a type can have "bits" (8 bits wide) and "lanes" (16-bit wide). In order to plug this type
        // into the Halide type system as if it is a usual Halide type, we choose to create this type
        // as halide_type_handle pointing to a dummy handle named "CGS"(compiler_generated_struct).
        // We use the "bits" for type id, and so we can create up to 256 different structs.
        Type halide_type = Type(halide_type_handle, index, 1, &GeneratedStructType::dummy);
        return halide_type;
    }
} // Internal

bool Type::is_generated_struct() const {
    return (code() == halide_type_handle &&
            (handle_type != NULL &&
             handle_type->inner_name.cpp_type_type == halide_cplusplus_type_name::Struct &&
             handle_type->inner_name.name =="CGS"));
}

} // Halide
