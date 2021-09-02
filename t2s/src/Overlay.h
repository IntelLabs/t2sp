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
#ifndef HALIDE_OVERLAY_H
#define HALIDE_OVERLAY_H
#include <map>
#include <string>

#include "../../Halide/src/DeviceArgument.h"
#include "../../Halide/src/Func.h"
#include "../../Halide/src/IR.h"
#include "../../Halide/src/IROperator.h"
#include "../../Halide/src/IntrusivePtr.h"

using std::map;
using std::string;
using std::vector;

namespace Halide {

class Func;
struct ImageParamOrExpr;
class ImageParam;
#ifndef INPUT_ONLY
#define INPUT_ONLY check_input
#endif

#ifndef OUTPUT_ONLY
#define OUTPUT_ONLY check_output
#endif

#ifndef INOUT
#define INOUT check_inout
#endif

template <typename... Args>
HALIDE_NO_USER_CODE_INLINE typename std::enable_if<Internal::all_are_convertible<ImageParamOrExpr, Args...>::value, std::vector<Argument>>::type
check_input(Args &&... args) {
    std::vector<ImageParamOrExpr> collected_args{std::forward<Args>(args)...};
    std::vector<Argument> input_arg;
    for (ImageParamOrExpr &arg : collected_args) {
        if (arg.is_image)
            input_arg.push_back(Argument(*(arg.image)));
        else
            input_arg.push_back(Argument());
    }
    return input_arg;
}

template <typename... Args>
HALIDE_NO_USER_CODE_INLINE typename std::enable_if<Internal::all_are_convertible<ImageParamOrExpr, Args...>::value, std::vector<Argument>>::type
check_output(Args &&... args) {
    std::vector<ImageParamOrExpr> collected_args{std::forward<Args>(args)...};
    std::vector<Argument> output_arg;
    for (ImageParamOrExpr &arg : collected_args) {
        if (arg.is_image)
            output_arg.push_back(Argument(*(arg.image)));
        else
            output_arg.push_back(Argument());
    }
    return output_arg;
}

template <typename... Args>
HALIDE_NO_USER_CODE_INLINE typename std::enable_if<Internal::all_are_convertible<ImageParamOrExpr, Args...>::value, std::vector<Argument>>::type
check_inout(Args &&... args) {
    std::vector<ImageParamOrExpr> collected_args{std::forward<Args>(args)...};
    std::vector<Argument> inout_arg;
    for (ImageParamOrExpr &arg : collected_args) {
        if (arg.is_image)
            inout_arg.push_back(Argument(*(arg.image)));
        else
            inout_arg.push_back(Argument());
    }
    return inout_arg;
}

using namespace Internal;

namespace Internal {

struct depInfo {
    int task_id;
    vector<Expr> iter_vars;
    vector<int> distances;
    vector<Expr> conds;
};

#define OVERLAY_BUFFER_ITEM_MAX_DIMS 5
struct buffer_item {
    string name;
    int dim;
    Expr mins[OVERLAY_BUFFER_ITEM_MAX_DIMS];
    Expr extents[OVERLAY_BUFFER_ITEM_MAX_DIMS];
    Expr strides[OVERLAY_BUFFER_ITEM_MAX_DIMS];
};

struct assign_map {
    map<string, string> arg_map;
    map<string, int> arg_pos;
};

using argMap = map<string, vector<DeviceArgument>>;
using inputMap = map<int, vector<ImageParamOrExpr>>;
using argAssignment = map<int, assign_map>;

struct OverlayContents;
class OverlayDefinition {
    IntrusivePtr<OverlayContents> contents;

public:
    OverlayDefinition();
    /** Get the taskItems associated with the overlay */
    vector<Function> &taskItems() const;
    /** Get the (inferred) argItems associated with the overlay */
    argMap &argItems() const;
    /** Get the user-provided inputs to the ip kernels */
    inputMap &inputItems() const;
    /** Get the kernel names associated with the overlay */
    vector<string> &kernelNames() const;
    /** Get the auto-run kernel arg assignment map of the overlay */
    argAssignment &assignMap() const;
    /** Map from task_id to queueNo */
    map<int, int> &task2queue() const;
};

extern Stmt create_overlay_schedule(Stmt s, const std::map<std::string, Function> &env);

} // end of namespace Internal

/* Overlay class */
class Overlay {
public:
    string output_file;
    vector<Func> functions;           // Overlay built-in kernel functions
    vector<Expr> exprs;               // Overlay Call intrinsics
    vector<Internal::Function> tasks; // Overlay enqueued tasks
    OverlayDefinition overlay;

    Overlay() = default;
    Overlay(Func &function);
    Overlay(vector<Func> _functions);
    template <typename... Args>
    Overlay(Func &function, Args &... args) {
        functions.push_back(function);
        std::vector<Func> collected_args{args...};
        for (auto &f : collected_args) {
            functions.push_back(f);
        }
    };

    /** Compile overlay kernels to separate CL files */
    Overlay &compile(const Target &t, const char *output);

    /** Get the internal overlay handle */
    OverlayDefinition definition() const {
        return overlay;
    }
};

/** Enqueue into overlay and return task definitin */
Overlay &enqueue(Overlay &overlay, int queueNo, vector<ImageParamOrExpr> &args);

// template <typename... T>
// Overlay &enqueue(Overlay &overlay, int queueNo, vector<ImageParamOrExpr> &enqueue_args, ImageParamOrExpr arg, T... rest) {
//     enqueue_args.push_back(arg);
//     return enqueue(overlay, queueNo, enqueue_args, std::forward<T>(rest)...);
// };

// Create an argument list from inputs variadic pack
template <typename... T>
Overlay &enqueue(Overlay &overlay, int queueNo, T &&... args) {
    vector<ImageParamOrExpr> enqueue_args{std::forward<T>(args)...};
    return enqueue(overlay, queueNo, enqueue_args);
};

} // namespace Halide
#endif
