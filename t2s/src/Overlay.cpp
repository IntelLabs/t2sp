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

#include "Overlay.h"
#include "CmdQueue.h"
#include "Simplify.h"

#include "../../Halide/src/Buffer.h"
#include "../../Halide/src/Expr.h"
#include "../../Halide/src/Func.h"
#include "../../Halide/src/IREquality.h"
#include "../../Halide/src/IRMutator.h"
#include "../../Halide/src/IRVisitor.h"
#include "../../Halide/src/ImageParam.h"

namespace Halide {

using namespace Internal;

// Overlay Constructor
Overlay::Overlay(Func &function) {
    functions.push_back(function);
}

Overlay::Overlay(vector<Func> _functions)
    : functions(_functions) {}

// Replace $ with _ and add prefix
string task_renaming(string name) {
    std::string new_name = name;
    if (name.find("$") != std::string::npos) {
        new_name = replace_all(new_name, "$", "_");
    }
    return "task_ip_" + new_name;
}

// Compile overlay kernels to separate CL files
Overlay &Overlay::compile(const Target &t, const char *binary) {
    output_file = std::string(binary);
    setenv("HL_KERNEL_NUM", std::to_string(functions.size()).c_str(), 1);
    for (auto &function : functions) {
        // Retrieve command queue args (ImageParam or Expr)
        vector<CmdQueueItem> &cmd_params = function.function().definition().schedule().cmd_params();
        user_assert(cmd_params.size() == 1);
        auto q = cmd_params[0];

        // for (Argument &arg : q.args) {
        //     switch (arg.kind) {
        //     case Argument::InputScalar:
        //         debug(4) << "Input expr: " << arg.name << "\n";
        //         break;
        //     case Argument::InputBuffer:
        //         debug(4) << "Input buffer: " << arg.name << "\n";
        //         break;
        //     case Argument::OutputBuffer:
        //         debug(4) << "Output buffer: " << arg.name << "\n";
        //         break;
        //     case Argument::InoutBuffer:
        //         debug(4) << "Inout buffer: " << arg.name << "\n";
        //         break;
        //     }
        // }

        // for (auto& arg : args) debug(4) << arg;

        function.print_loop_nest();
        auto name = task_renaming(function.name());

        setenv("HL_OVERLAY_KERNEL", name.c_str(), 1);
        // Each kernel function is linked with the overlay
        function.function().overlay(*this);
        // Compile kernels into separate files
        function.compile_jit(t);
        unsetenv("HL_OVERLAY_KERNEL");

        // Compile options. These options are used for invoking AOC
        // compiler to include the kernel functions with main body
        char *overlay_kenrel_files = getenv("HL_OVERLAY_FILES");
        if (overlay_kenrel_files == NULL) {
            setenv("HL_OVERLAY_FILES", name.c_str(), 1);
        } else {
            string current_ips = std::string(overlay_kenrel_files);
            current_ips += " " + name;
            setenv("HL_OVERLAY_FILES", current_ips.c_str(), 1);
        }
    }
    return *this;
}

// Define the UREs (main body of app)
// for i from 0 to 10:
//     Task0(...) = Call::overlay(...)
//     Task1(...) = Call::overlay(...)
//     Task2(...) = Call::overlay(...)
Overlay &enqueue(Overlay &overlay, int queue, vector<ImageParamOrExpr> &args) {

    // Counting the enqueued task numbers
    char *overlay_kenrel_num = getenv("HL_OVERLAY_NUM");
    int num;
    if (overlay_kenrel_num == NULL) {
        setenv("HL_OVERLAY_NUM", "1", 1);
        num = 1;
    } else {
        num = std::atoi(overlay_kenrel_num) + 1;
        setenv("HL_OVERLAY_NUM", std::to_string(num).c_str(), 1);
    }

    user_assert(overlay.functions.size() > (unsigned)queue)
        << "Not found queue " << queue << " in the overlay of size "
        << overlay.functions.size() << "\n";

    // Create arg list for enqueued kernel functions
    // Here we save all the buffers into the overlay
    // and include these buffers as task inputs
    // in the scheduling function's argument list
    vector<Expr> func_args = {IntImm::make(Int(32), num - 1), IntImm::make(Int(32), queue)};

    Type type;
    for (auto &arg : args) {
        if (arg.is_image) {
            const ImageParam &param = *(arg.image);
            debug(4) << arg.image;
            Buffer<> buffer = param.get();
            if (buffer.dimensions() > 0) {
                debug(4) << "Processing buffer " << buffer.name()
                         << " passed by enqueue()...\n";
                if (arg.is_cropped) {
                    std::pair<int, std::vector<Expr>> cropped_info = arg.cropped_info;
                    debug(4) << "Buffer " << arg.image->get().name() << " is cropped...\n"
                             << "Block size: " << cropped_info.first << "\n"
                             << "Dim: " << cropped_info.second.size() / 2 << "\n";
                    for (unsigned int i = 0; i < cropped_info.second.size(); i += 2) {
                        debug(4) << "Start: " << cropped_info.second[i]
                                 << ", End: " << cropped_info.second[i + 1] << "\n";
                    }
                }

                vector<Expr> indices;
                for (int k = 0; k < buffer.dimensions(); k++) {
                    auto dim = buffer.dim(k);
                    debug(4) << "Dim " << dim.min() << ":" << dim.max() << "\n";
                    indices.push_back(dim.max());
                }
                Expr e = buffer(indices);
                func_args.push_back(e);
                type = buffer.type();
            }
        } else {
            // TODO: the enqueued arg is expr
        }

        // func_args.push_back(StringImm::make(arg.name()));
    }

    // Add task queue user-provided inputs (actual buffers and values) into overlay
    // The kernel ip func symbolic defined inputs are saved into each func
    overlay.definition().inputItems().insert({num - 1, args});
    overlay.definition().task2queue().insert({num - 1, queue});

    // user_assert(func_args.size() == args.size() + 1);
    debug(4) << "Assign output buffer data type as " << type << "\n";
    Expr e = Call::make(type, Call::overlay, func_args, Call::Intrinsic);
    debug(4) << "Intrinsics " << e << " Args size " << args.size() << "\n";

    // Pass the call instrinsic expr to Func
    overlay.exprs.push_back(e);
    debug(4) << "Main loop expr size:  " << overlay.exprs.size() << "\n";

    // Set up default data type
    if (type.is_float())
        setenv("HL_OVERLAY_DTYPE", "float", 1);
    else
        setenv("HL_OVERLAY_DTYPE", "int", 1);
    return overlay;
}
namespace Internal {

// Stmt call_extern_and_assert(const string &name, const vector<Expr> &args) {
//     Expr call = Call::make(Int(32), name, args, Call::Extern);
//     string call_result_name = unique_name(name + "_result");
//     Expr call_result_var = Variable::make(Int(32), call_result_name);
//     return LetStmt::make(call_result_name, call,
//                          AssertStmt::make(EQ::make(call_result_var, 0), call_result_var));
// }

// Intrusive shared pointer for overlay
struct OverlayContents {
    mutable RefCount ref_count;
    bool is_init;
    Expr predicate;
    vector<Function> tasks; // enqueued tasks
    argMap args;            // inferred kernel arguments
    inputMap inputs;        // args pushed by enqueue()
    vector<string> kernels; // kernel names
    argAssignment assignment;
    map<int, int> task_to_queue;
    OverlayContents()
        : is_init(true), predicate(const_true()) {}
};

template <>
RefCount &ref_count<OverlayContents>(const OverlayContents *d) noexcept {
    return d->ref_count;
}

template <>
void destroy<OverlayContents>(const OverlayContents *d) {
    delete d;
}

OverlayDefinition::OverlayDefinition()
    : contents(new OverlayContents) {}

// Get enqueued tasks as array of Function
vector<Function> &OverlayDefinition::taskItems() const {
    return contents->tasks;
}

// Get (inferred) argument list of included kernel functions
argMap &OverlayDefinition::argItems() const {
    return contents->args;
}

// Get user-provided inputs to the kernel function
inputMap &OverlayDefinition::inputItems() const {
    return contents->inputs;
}

// Get list of overlay IP function names
vector<string> &OverlayDefinition::kernelNames() const {
    return contents->kernels;
}

argAssignment &OverlayDefinition::assignMap() const {
    return contents->assignment;
}

/** Map from task_id to queueNo */
map<int, int> &OverlayDefinition::task2queue() const {
    return contents->task_to_queue;
}

// Add an end signal updater at end of the outer loop
class OverlayLoopUpdating : public IRMutator {
    using IRMutator::visit;
    std::string target_loop;

    Stmt visit(const For *op) override {
        auto name = op->name;
        if (name == target_loop) {
            auto loop_var = Variable::make(Int(32), op->name);
            Expr cond = EQ::make(loop_var, simplify(op->extent - op->min - 1));

            vector<Expr> args = {StringImm::make("end_signal")};
            Expr e = Call::make(Handle(), Call::overlay_switch, args, Call::Intrinsic);
            Stmt end = IfThenElse::make(cond, Evaluate::make(e));

            Stmt body = mutate(op->body);
            Stmt new_body = Block::make(body, end);
            return For::make(name, op->min, op->extent, op->for_type,
                             op->device_api, new_body);
        }
        return For::make(name, op->min, op->extent, op->for_type,
                         op->device_api, mutate(op->body));
    }

public:
    OverlayLoopUpdating(const string _target_loop)
        : target_loop(_target_loop) {}
};

// Replace user-input loop vars with real vars in IR
// E.g. i <-> Task0.s0.i
class SubstitueLoopVar : public IRMutator {
    using IRMutator::visit;
    vector<string> loop_var_names;
    Expr visit(const Variable *op) override {
        string name = "";
        for (unsigned int i = 0; i < loop_var_names.size(); i++) {
            if (ends_with(loop_var_names[i], op->name)) {
                name = loop_var_names[i];
                break;
            }
        }

        if (name != "") {
            debug(4) << "Replace " << op->name << " with "
                     << name << "...\n";
            return Variable::make(op->type, name);
        }
        return IRMutator::visit(op);
    }

public:
    SubstitueLoopVar(vector<string> _loop_var_names)
        : loop_var_names(_loop_var_names) {}
};

// Update the scehduler main body
class OverlayIntrinsicUpdating : public IRMutator {
    using IRMutator::visit;
    argMap &dev_func_args;
    const map<int, vector<depInfo>> &deps;
    const map<int, int> &task2queue;
    std::vector<Expr> loop_vars;
    vector<string> loop_var_names;
    inputMap arg_map;
    const argAssignment assignment;
    bool dev_entry = false;
    vector<Expr> expected_args;
    Buffer<> output_buffer;
    Type type;
    std::string output_task_name;
    Expr index;

    Stmt visit(const Store *op) override {
        auto in = op->value.as<Call>();
        internal_assert(in);
        internal_assert(in->is_intrinsic(Call::overlay));
        type = in->type;
        output_task_name = op->name;
        index = op->index;
        // Extract expected arguments
        internal_assert(in->args[0].as<IntImm>());
        internal_assert(in->args[1].as<IntImm>());
        int task_id = in->args[0].as<IntImm>()->value;
        int queue = in->args[1].as<IntImm>()->value;
        internal_assert(in->args.size() > 1);

        for (int i = 2; i < (signed)in->args.size(); i++) {
            expected_args.push_back(in->args[i]);
            if (auto load = in->args[i].as<Load>()) {
                debug(4) << "Set up the output buffer...\n";
                output_buffer = load->image;
            }
        }

        // Assert the output buffer is passed in
        internal_assert(output_buffer.defined());

        // Create insertion points
        // E.g.
        // for (i, 0, 10) {
        //    task_t task = {index, {deps...}, {inputs...}, num_input, valid}
        //    case 0:
        //      index_t dep_0 = {queue, {conds...}}
        //      task.deps[0] = dep_0;
        //      ...
        //      task.num_of_deps = ...
        // }
        //

        // Construct the new body
        debug(4) << "\n";
        debug(4) << "Constructing the app function body...\n";
        Stmt new_body = Evaluate::make(0);

        // Create args for each task
        //   1. depending queue index
        //   2. iteration vars
        //   3. numbers of dependents

        vector<Expr> task_args;
        task_args.push_back(IntImm::make(Int(32), task_id));
        debug(4) << "Checking task " << task_id << "'s dependency...\n";
        task_args.push_back(IntImm::make(Int(32), queue));

        debug(4) << "Iteration space dimension of " << loop_vars.size() << "...\n";
        auto dep_vec = deps.at(task_id);
        for (auto &dep_info : dep_vec) {
            task_args.push_back(dep_info.task_id);

            // Push back distance
            int index = 0;
            internal_assert(dep_info.distances.size() == loop_vars.size());
            for (auto &v : dep_info.distances) {
                Expr expr = simplify(v + loop_vars[index]);
                task_args.push_back(expr);
                debug(4) << v << ",";
                index++;
            }

            // TO FIX: Add use-input conds
            // Push back conditions
            // Here we ensure that the (Itervar + distance >= 0)
            index = 0;
            for (auto &dis : dep_info.distances) {
                if (dis < 0) {
                    Expr cond = (loop_vars[index] + dis >= 0);
                    task_args.push_back(cond);
                }
                index++;
            }

            debug(4) << "\n";
        } // end of dependency-related intrinsic args

        // Add input information (actual input buffer pointers along
        // with the hidden parameters including mins, extents and strides)
        auto offset = buffer_offset.at(task_id);
        SubstitueLoopVar update(loop_var_names);
        for (auto &kv : hidden_params_assignment.at(task_id)) {
            Expr data = kv.second;
            // string task_arg = kv.first + "::" + print_expr(data);
            // if (auto v = data.as<IntImm>()) {
            //     task_arg = kv.first + "::" + std::to_string(v->value);
            // } else if (auto v = data.as<FloatImm>()) {
            //     task_arg = kv.first + "::" + std::to_string(v->value);
            // } else if (auto v = data.as<StringImm>()) {
            //     task_arg = kv.first + "::" + v->value;
            // }
            // if (auto v = data.as<StringImm>()) {
            //     Expr src_buffer = Variable::make(type_of<struct halide_buffer_t *>(), v->value);

            //     Expr alloca_size = Call::make(Int(32), Call::size_of_halide_buffer_t, {}, Call::Intrinsic);
            //     Expr cropped_input = Call::make(type_of<struct halide_buffer_t *>(), Call::alloca,
            //                                     {alloca_size}, Call::Intrinsic);

            //     vector<Expr> args(5);
            //     args[0] = cropped_input;
            //     args[1] = Call::make(type_of<struct halide_dimension_t *>(), Call::alloca,
            //                          {(int)sizeof(halide_dimension_t) * 1}, Call::Intrinsic);
            //     args[2] = src_buffer;

            //     vector<Expr> mins, extents;

            //     mins.push_back(loop_vars[0]);
            //     extents.push_back(2);

            //     args[3] = Call::make(type_of<const int *>(), Call::make_struct, mins, Call::Intrinsic);
            //     args[4] = Call::make(type_of<const int *>(), Call::make_struct, extents, Call::Intrinsic);

            //     cropped_input = Call::make(type_of<struct halide_buffer_t *>(), Call::buffer_crop,
            //                                args, Call::Extern);
            //     // new_body = LetStmt::make(v->value + ".cropped", cropped_input, new_body);
            //     task_args.push_back(StringImm::make(kv.first));
            //     task_args.push_back(cropped_input);
            // } else {
            // Stmt c = call_extern_and_assert("_halide_buffer_crop", );
            if (auto v = data.as<StringImm>()) {
                task_args.push_back(StringImm::make(kv.first + "::" + v->value));
                Expr offset_arg = update.mutate(offset[kv.first]);
                task_args.push_back(offset_arg);
                debug(4) << "Add Task" << task_id << "input \"" << kv.first << "\" = " << data << "+" << offset_arg << "\n";
            } else {
                data = update.mutate(data);
                task_args.push_back(StringImm::make(kv.first));
                task_args.push_back(data);
                debug(4) << "Add Task" << task_id << "input \"" << kv.first << "\" = " << data << "\n";
            }
        }

        // Define the command queue channels with overlay intrinsic
        Expr e = Call::make(in->type, in->name, task_args,
                            in->call_type, in->func, in->value_index, in->image, in->param);
        return Block::make(Evaluate::make(e), new_body);
    }

    // Extract the overly intrinsics in the innermost loop body
    Stmt visit(const For *op) override {
        auto name = op->name;

        // Rename the device function
        if (ends_with(op->name, ".run_on_device") && !dev_entry) {
            dev_entry = true;
            name = "app.run_on_device";

            // Add a loop waiting for the end signal
            // at the beginning and end of the body
            Stmt body = mutate(op->body);

            // Add statements before loop starts
            vector<Expr> args = {StringImm::make("before_main_body")};
            Expr e = Call::make(Handle(), Call::overlay_switch, args, Call::Intrinsic);
            Stmt s = Block::make(Evaluate::make(e), body);

            // Add statements after loop ends
            vector<Expr> new_args = {StringImm::make("after_main_body")};
            Expr new_e = Call::make(Handle(), Call::overlay_switch, new_args, Call::Intrinsic);
            s = Block::make(s, Evaluate::make(new_e));

            return For::make(name, op->min, op->extent, op->for_type, op->device_api, s);
        } else if (!ends_with(op->name, ".run_on_device")) {
            auto loop_var = Variable::make(Int(32), op->name);
            loop_vars.push_back(loop_var);
            loop_var_names.push_back(op->name);
            if (loop_vars.size() == 1)
                outermost_loop = op->name;
        }

        // Inner-most loop for task index switching
        // Note that the store op needs to be kept to store the final output
        // of the task. For now we do not support in-place calculation
        if (!op->body.as<For>() && !op->body.as<ProducerConsumer>()) {

            debug(4) << "Overlay intrinsic: " << op->body << "\n";
            Stmt new_body = mutate(op->body);
            // Define the loop body before entering switch
            // E.g.
            //
            // task_t task;
            // task.done = false;
            //
            // switch(var) {
            //   // ...
            // }
            setenv("HL_SPACE_DIM", std::to_string(loop_vars.size()).c_str(), 1);
            vector<Expr> args = {StringImm::make("before_switch")};
            for (auto &var : loop_vars) {
                args.push_back(var);
            }
            Expr e = Call::make(Handle(), Call::overlay_switch, args, Call::Intrinsic);
            new_body = Block::make(Evaluate::make(e), new_body);

            // // Define the loop on top of switch statement
            // debug(4) << "Create ip switch loop of size " << deps.size() << "\n";
            // new_body = For::make("ip.switch", 0, static_cast<int>(deps.size()),
            //                      ForType::Serial, DeviceAPI::None, new_body);

            // Setup an instrinsic to anchor input buffer
            // Here we create an intrinsic call with args containing all
            // the actual input buffers.
            Expr intrinsic_type = StringImm::make("buffer_marker");
            expected_args.insert(expected_args.begin(), intrinsic_type);

            Expr output = Load::make(type, output_task_name, index, output_buffer, Parameter(),
                                     const_true(), ModulusRemainder());
            expected_args.push_back(output);

            // Add buffer marker instrinsics after the switch loop
            Expr new_value = Call::make(Handle(), Call::overlay_switch,
                                        expected_args, Call::Intrinsic);
            new_body = Block::make(new_body, Evaluate::make(new_value));

            return For::make(name, op->min, op->extent, op->for_type,
                             op->device_api, new_body);
        }

        return For::make(name, op->min, op->extent, op->for_type,
                         op->device_api, mutate(op->body));
    }

public:
    // Save buffer attributes (acquired from enqueued buffers)
    vector<buffer_item> buffer_items;
    // Save inferred results of hidden param assignment for each task
    // E.g. A.min.0 <--> ( data.constants[1] <--> 0 )
    map<int, map<string, Expr>> hidden_params_assignment;
    map<int, map<string, Expr>> buffer_offset; // Store offset when buffer is cropped

    std::string outermost_loop;

    OverlayIntrinsicUpdating(argMap &_dev_func_args,
                             const map<int, vector<depInfo>> &_deps,
                             const map<int, int> &_task2queue,
                             const inputMap &_arg_map, /*enqueued buffers*/
                             const argAssignment _assignment)
        : dev_func_args(_dev_func_args), deps(_deps), task2queue(_task2queue),
          arg_map(_arg_map), assignment(_assignment) {

        // Analyze the assignment (ordered) map
        // binding: Map from command queue index --> map of arg assignment
        // arg_map: vector of enqueued buffers
        // internal_assert(assignment.size() == arg_map.size());
        for (unsigned int task_id = 0; task_id < arg_map.size(); task_id++) {
            debug(4) << "\nTask " << task_id << " Assignment...";

            auto buffer_or_expr = arg_map.at(task_id);
            int queueNo = task2queue.at(task_id);
            debug(4) << "\nQueueNo " << queueNo;
            auto binding = assignment.at(queueNo).arg_map;
            auto arg_pos = assignment.at(queueNo).arg_pos;

            // Create memory items with attribute
            // Here we require the ordering of input buffers from enqueue()
            // as the same for args passed in from command()
            for (auto &kv : binding) {
                string key = kv.first;

                // Match the hidden arg (buffer min)
                if (key.find(".min") != string::npos) {
                    int dim = std::atoi(&key.back());
                    internal_assert(dim < OVERLAY_BUFFER_ITEM_MAX_DIMS);
                    string name = replace_all(key, ".min." + std::to_string(dim), "");

                    auto item = buffer_items.back();
                    debug(4) << "  Set min for buffer " << name << " as "
                             << item.mins[dim] << " labeled as " << kv.second << "\n";
                    hidden_params_assignment[task_id][kv.second] = item.mins[dim];

                    // Match the hidden arg (buffer extent)
                } else if (key.find(".extent") != string::npos) {
                    int dim = std::atoi(&key.back());
                    internal_assert(dim < OVERLAY_BUFFER_ITEM_MAX_DIMS);
                    string name = replace_all(key, ".extent." + std::to_string(dim), "");

                    auto item = buffer_items.back();
                    debug(4) << "  Set extent for buffer " << name << " as "
                             << item.extents[dim] << " labeled as " << kv.second << "\n";
                    hidden_params_assignment[task_id][kv.second] = item.extents[dim];

                    // Match the hidden arg (buffer stride)
                } else if (key.find(".stride") != string::npos) {
                    int dim = std::atoi(&key.back());
                    internal_assert(dim < OVERLAY_BUFFER_ITEM_MAX_DIMS);
                    string name = replace_all(key, ".stride." + std::to_string(dim), "");

                    auto item = buffer_items.back();
                    debug(4) << "  Set stride for buffer " << name << " as "
                             << item.strides[dim] << " labeled as " << kv.second << "\n";
                    hidden_params_assignment[task_id][kv.second] = item.strides[dim];

                    // Match the hidden arg (buffer pointer)
                    // Create an entry in the buffer-items mapping (which will
                    // later be used to create overlay instrinsic args)
                } else if (key.find(".") != string::npos) {
                    ImageParamOrExpr param = buffer_or_expr[arg_pos[key]];
                    internal_assert(!param.is_image);
                    Expr e = param.expr;
                    debug(4) << "  Set Scalar " << key << " as "
                             << e << " labeled as " << kv.second << "\n";
                    hidden_params_assignment[task_id][kv.second] = e;
                } else {
                    debug(4) << "\n";
                    int index;
                    if (arg_pos.find(key) == arg_pos.end()) {
                        index = (signed)buffer_or_expr.size() - 1;
                        debug(4) << "Auto Buffer-Aliasing: reusing input buffer "
                                 << buffer_or_expr[index].image->name() << " as output buffer\n";
                    } else {
                        index = arg_pos[key];
                    }

                    ImageParamOrExpr param = buffer_or_expr[index];
                    Buffer<> buffer = (param.image)->get();

                    debug(4) << "Buffer binding (" << key << ") with ("
                             << buffer.name() << ") labeled as " << kv.second << "\n";

                    buffer_item bi;
                    bi.name = buffer.name();
                    bi.dim = buffer.dimensions();
                    Expr offset = 0;
                    // Save the buffer attributes
                    if (param.is_cropped) {

                        std::pair<int, std::vector<Expr>> cropped_info = param.cropped_info;
                        auto block_size = IntImm::make(Int(32), cropped_info.first);
                        for (int i = 0; i < buffer.dimensions(); i++) {
                            bi.mins[i] = cropped_info.second[2 * i] * block_size;
                            bi.extents[i] = simplify((cropped_info.second[2 * i + 1] - cropped_info.second[2 * i] + 1) * block_size);
                            bi.strides[i] = buffer.dim(i).stride();
                            offset = offset * bi.strides[i] + bi.mins[i];
                            debug(4) << "test extent" << bi.extents[i]
                                     << " dim" << i << "\n";
                        }
                        buffer_offset[task_id][kv.second] = simplify(offset);
                    } else {
                        for (int i = 0; i < buffer.dimensions(); i++) {
                            bi.mins[i] = buffer.dim(i).min();
                            bi.extents[i] = buffer.dim(i).extent();
                            bi.strides[i] = buffer.dim(i).stride();
                        }
                        buffer_offset[task_id][kv.second] = 0;
                    }

                    buffer_items.push_back(bi);
                    hidden_params_assignment[task_id][kv.second] = buffer.name();
                }
            }
        }
    }
};

// Replace the arguments with read-in data from channel
class SubstituteArgs : public IRMutator {
public:
    using IRMutator::visit;
    // Names of buffers and loop boundary
    vector<string> buffers, mins, extents, strides, scalars;
    // User-provided actual arguments
    vector<Argument> sym_args;
    // Map from inferred arg to arg position index
    map<string, int> arg_pos;
    map<string, string> arg_map;

    // Intermediate scalar counter
    int scalar_counter{0};

    // Intermediate variable name map. E.g. scalar1 to data.constants[0]
    map<string, string> temp_var_name_map;
    // Memory op name map. E.g. A_ to data.arg0
    map<string, string> mem_op_name_map;

    Expr visit(const Variable *op) override {
        Expr expr = IRMutator::visit(op);

        // Substitute the loop constant as indicated in the arg map
        // E.g. assign data.constants[0] to A.0.extent
        if (arg_map.count(op->name)) {
            debug(4) << "Substitute " << arg_map[op->name] << " to "
                     << op->name << "...\n";
            string name = "scalar" + std::to_string(scalar_counter);
            scalar_counter += 1;
            temp_var_name_map[name] = arg_map[op->name];
            return Variable::make(op->type, name);
        }
        return expr;
    }

    // Substitue the memory related ops (i.e. load and store)
    // Here we cannot create a new store stmt or laod expr
    // since the memory re-casting will cause issues in AOC 17
    Stmt visit(const Store *op) override {
        if (arg_map.count(op->name)) {
            debug(4) << "Substitute " << arg_map[op->name] << " to tensor "
                     << op->name << "...\n";
            mem_op_name_map[op->name] = arg_map[op->name];
        }
        return IRMutator::visit(op);
    }

    Expr visit(const Load *op) override {
        if (arg_map.count(op->name)) {
            debug(4) << "Substitute " << arg_map[op->name] << " to tensor "
                     << op->name << "...\n";
            mem_op_name_map[op->name] = arg_map[op->name];
        }
        return IRMutator::visit(op);
    }

    // Construstor
    SubstituteArgs(vector<string> _buffers, vector<string> _mins,
                   vector<string> _extents, vector<string> _strides,
                   vector<string> _scalars,
                   vector<Argument> _sym_args) : buffers(_buffers),
                                                 mins(_mins), extents(_extents), strides(_strides), scalars(_scalars), sym_args(_sym_args) {

        // Create the map from inffered args to actual arg position
        // e.g. A_ shuold be mapped to arg_t data.args
        // The output buffer (e.g. buffers[-1]) won't be passed as
        // the symbolic args (i.e. command() API)
        int buffer_index = 0, scalar_index = 0, constant_index = 0;
        int pos = 0;
        for (auto &arg : sym_args) {

            if (arg.is_buffer()) {
                auto buffer_name = buffers[buffer_index];
                arg_map[buffer_name] = "inputs.args" + std::to_string(buffer_index);
                arg_pos[buffer_name] = pos;
                debug(4) << "\nAssign buffer index inputs.args" << buffer_index
                         << " to buffer " << buffer_name << "...\n";
                buffer_index += 1;

                // Process min, extent and stride associated with the buffer
                for (unsigned int i = 0; i < mins.size(); i++) {
                    auto min = mins[i];
                    if (starts_with(min, buffer_name + ".")) {
                        arg_map[min] = "inputs.constants[" +
                                       std::to_string(constant_index + 1) + "]";
                        debug(4) << "Loop.min: Assign inputs.constants[" << constant_index + 1
                                 << "] to buffer attribute " << min << "...\n";
                        constant_index += 1;
                    }
                }

                for (unsigned int i = 0; i < extents.size(); i++) {
                    auto extent = extents[i];
                    if (starts_with(extent, buffer_name + ".")) {
                        arg_map[extent] = "inputs.constants[" +
                                          std::to_string(constant_index + 1) + "]";
                        debug(4) << "Loop.extent: Assign inputs.constants[" << constant_index + 1
                                 << "] to buffer attribute " << extent << "...\n";
                        constant_index += 1;
                    }
                }

                for (unsigned int i = 0; i < strides.size(); i++) {
                    auto stride = strides[i];
                    if (starts_with(stride, buffer_name + ".")) {
                        arg_map[stride] = "inputs.constants[" +
                                          std::to_string(constant_index + 1) + "]";
                        debug(4) << "Loop.stride: Assign inputs.constants[" << constant_index + 1
                                 << "] to buffer attribute " << stride << "...\n";
                        constant_index += 1;
                    }
                }
                debug(4) << "\n";

                // Other non-buffer parameters
            } else {
                auto scalar_name = scalars[scalar_index];
                arg_map[scalar_name] = "inputs.constants[" + std::to_string(constant_index + 1) + "]";
                arg_pos[scalar_name] = pos;
                debug(4) << "Assign inputs.constants[" << constant_index + 1 << "] to scalar " << scalar_name << "...\n";
                scalar_index++;
                constant_index++;
            }
            pos++;
        }

        // The last left buffer is for function output
        internal_assert((unsigned)buffer_index == buffers.size() - 1);
        auto buffer_name = buffers[buffer_index];
        arg_map[buffer_name] = "inputs.args" + std::to_string(buffer_index);
        debug(4) << "Assign buffer index " << arg_map[buffer_name]
                 << " to (output) buffer " << buffer_name << "...\n";

        // Check the hidden args associated with output buffer
        for (unsigned int i = 0; i < mins.size(); i++) {
            auto min = mins[i];
            if (starts_with(min, buffer_name + ".")) {
                arg_map[min] = "inputs.constants[" +
                               std::to_string(constant_index + 1) + "]";
                debug(4) << "Loop.min: Assign inputs.constants[" << constant_index + 1
                         << "] to buffer attribute " << min << "...\n";
                constant_index += 1;
            }
        }

        for (unsigned int i = 0; i < extents.size(); i++) {
            auto extent = extents[i];
            if (starts_with(extent, buffer_name + ".")) {
                arg_map[extent] = "inputs.constants[" +
                                  std::to_string(constant_index + 1) + "]";
                debug(4) << "Loop.extent: Assign inputs.constants[" << constant_index + 1
                         << "] to buffer attribute " << extent << "...\n";
                constant_index += 1;
            }
        }

        for (unsigned int i = 0; i < strides.size(); i++) {
            auto stride = strides[i];
            if (starts_with(stride, buffer_name + ".")) {
                arg_map[stride] = "inputs.constants[" +
                                  std::to_string(constant_index + 1) + "]";
                debug(4) << "Loop.stride: Assign inputs.constants[" << constant_index + 1
                         << "] to buffer attribute " << stride << "...\n";
                constant_index += 1;
            }
        }

        // Check all the hidden args have been assigned
        int hidden_args_num = buffers.size() + mins.size() + extents.size() + strides.size();
        int assigned_args_num = (buffer_index + 1) + (constant_index - scalar_index);
        internal_assert(hidden_args_num == assigned_args_num)
            << "Required " << hidden_args_num << " hidden parameters ("
            << assigned_args_num << " given)\n";
    }
};

// Eliminate all pending args to make autorun kernels
Stmt MakeAutorunKernel(Stmt s, vector<Argument> sym_args,
                       vector<DeviceArgument> inferred_args, map<string, string> &arg_map,
                       map<string, int> &arg_pos) {

    vector<string> buffers, mins, extents, strides, scalars;
    for (auto &arg : inferred_args) {
        // argument passed by pointer
        if (arg.is_buffer) {
            buffers.push_back(arg.name);
            debug(4) << "Buffer: " << arg.name << "\n";

        } else if (arg.name.find("min") != string::npos) {
            mins.push_back(arg.name);
            debug(4) << "Min: " << arg.name << "\n";

        } else if (arg.name.find("extent") != string::npos) {
            extents.push_back(arg.name);
            debug(4) << "Extent: " << arg.name << "\n";

        } else if (arg.name.find("stride") != string::npos) {
            strides.push_back(arg.name);
            debug(4) << "Stride: " << arg.name << "\n";

        } else {
            scalars.push_back(arg.name);
            debug(4) << "Scalar: " << arg.name << "\n";
        }
    }

    // Here we assume the min starts from 0 for kernel function
    // Symbolic arguments are the args defined in command(...)
    // 1. Parameters like extents, strides are passed from scheduler
    //    in form of args.constans[0/1...], and they must be defined
    //    in the command() APIs.
    // 2. The symbolic arguments does not provide the information to
    //    create an auto-run kernel. It only checks the buffer arg num.
    //    All other hidden parameters (min/extent/stride) will be treated
    //    as parameters passed from scheduler

    // unsigned int num_of_buffers = buffers.size();
    // internal_assert(num_of_buffers == sym_args.size() + 1)
    //     << "Missing arguments at command()... " << (num_of_buffers - 1)
    //     << " arguments required but " << sym_args.size() << " given\n";

    // Assign arg_t fields to inferred args
    SubstituteArgs update(buffers, mins, extents, strides, scalars, sym_args);
    s = update.mutate(s);
    arg_map = update.arg_map;
    arg_pos = update.arg_pos;
    auto temp_var_name_map = update.temp_var_name_map;
    auto mem_op_name_map = update.mem_op_name_map;

    // Add let stmt for scalars after the data-reading channel
    Expr type = StringImm::make("data");
    for (auto &kv : temp_var_name_map) {
        Expr arg_name = StringImm::make(kv.second);
        Expr e = Call::make(Int(32), Call::overlay_switch, {type, arg_name}, Call::Intrinsic);
        s = LetStmt::make(kv.first, e, s);
    }

    // Create global pointer allocation
    for (auto &kv : mem_op_name_map) {
        string name = kv.first;
        Allocate *node = new Allocate;
        node->name = name;
        node->type = Int(32);
        node->memory_type = MemoryType::CLPtr;
        node->extents = {kv.second};
        node->new_expr = Expr();
        node->free_function = std::string();
        node->condition = const_true();
        node->body = std::move(s);
        s = Stmt(node);
    }

    return s;
};

// Mutate IP kernel function body
class IpFuncBodyUpdate : public IRMutator {
    using IRMutator::visit;
    // Inferred kernel function args
    map<string, vector<DeviceArgument>> inferred_args;
    string target_kernel_name;
    int target_kernel_queue_index;
    vector<Argument> sym_args;
    map<string, string> &arg_map;
    map<string, int> &arg_pos;
    bool run_on_device_entry{false};

    // The function must be placed on device
    Stmt visit(const For *op) override {
        if (ends_with(op->name, ".run_on_device") && !run_on_device_entry) {

            run_on_device_entry = true;
            Stmt body = mutate(op->body);

            // Adding start and end helper code to the kernel function
            debug(4) << "Overlay decorating kenrel " << op->name << "...\n";

            // Create autorun kernel
            // Analyze the args defined by command()
            //
            // 1. Assign the task itervar min as 0
            // 2. Assign input/output buffer to dara.args0/1...
            for (auto &e : sym_args) {
                debug(4) << "Overlay kernel " << target_kernel_name
                         << " input args: " << e.name << "\n";
            }
            debug(4) << "Making autorun kernel....\n";
            debug(4) << body;

            auto dev_args = inferred_args[target_kernel_name];
            body = MakeAutorunKernel(body, sym_args, dev_args, arg_map, arg_pos);

            // Add leading and trailing decoration code block to
            // the kernel IP function (e.g. arg_t data = read_channel_intel...)
            vector<Expr> args = {StringImm::make("kernel_begin")};
            args.push_back(IntImm::make(Int(32), target_kernel_queue_index));
            Expr e = Call::make(Handle(), Call::overlay_switch, args, Call::Intrinsic);
            body = Block::make(Evaluate::make(e), body);

            vector<Expr> new_args = {StringImm::make("kernel_end")};
            new_args.push_back(IntImm::make(Int(32), target_kernel_queue_index));
            Expr new_e = Call::make(Handle(), Call::overlay_switch, new_args, Call::Intrinsic);
            body = Block::make(body, Evaluate::make(new_e));

            // Add a while loop for the body
            body = For::make("autorun.infinite", 0, 10, ForType::Serial, DeviceAPI::None, body);

            return For::make(op->name, op->min, op->extent, op->for_type,
                             op->device_api, body);
        }
        return For::make(op->name, op->min, op->extent, op->for_type,
                         op->device_api, mutate(op->body));
    }

public:
    IpFuncBodyUpdate(map<string, vector<DeviceArgument>> _inferred_args,
                     string _taregt_kernel_name, int _target_kernel_queue_index,
                     vector<Argument> _sym_args, map<string, string> &_arg_map,
                     map<string, int> &_arg_pos) : inferred_args(_inferred_args), target_kernel_name(_taregt_kernel_name),
                                                   target_kernel_queue_index(_target_kernel_queue_index),
                                                   sym_args(_sym_args), arg_map(_arg_map), arg_pos(_arg_pos) {}
};

// Analyze the run_on_device for loops as kernel functions
class LoopArgsInference : public IRMutator {
    using IRMutator::visit;
    bool run_on_device_entry{false};

    // Analyze the loop variables (kernel args)
    Stmt visit(const For *op) override {
        if (ends_with(op->name, ".run_on_device") && !run_on_device_entry) {
            debug(4) << "Overlay found kernel func: " << op->name << "\n";
            run_on_device_entry = true;

            // compute a closure over the state passed into the kernel
            HostClosure c(op->body, op->name);

            // Determine the arguments that must be passed into the halide function
            vector<DeviceArgument> closure_args = c.arguments();

            // Sort the args by the size of the underlying type. This is
            // helpful for avoiding struct-packing ambiguities in metal,
            // which passes the scalar args as a struct.
            std::sort(closure_args.begin(), closure_args.end(),
                      [](const DeviceArgument &a, const DeviceArgument &b) {
                          if (a.is_buffer == b.is_buffer) {
                              return a.type.bits() > b.type.bits();
                          } else {
                              return a.is_buffer < b.is_buffer;
                          }
                      });

            // Extract the kernel argument list
            // HL_OVERLAY_KERNEL is set when compiling kernel functions
            char *overlay_kenrel_name = getenv("HL_OVERLAY_KERNEL");
            user_assert(overlay_kenrel_name != NULL);
            inferred_args[string(overlay_kenrel_name)] = closure_args;
            for (size_t i = 0; i < closure_args.size(); i++) {
                debug(4) << "Extract arg name " << closure_args[i].name << "\n";
            }

            return For::make(op->name, op->min, op->extent,
                             ForType::Serial, op->device_api, mutate(op->body));
        }
        return For::make(op->name, op->min, op->extent, op->for_type,
                         op->device_api, mutate(op->body));
    }

    Expr visit(const Call *op) override {
        if (op->is_intrinsic(Call::overlay)) {
            return Call::make(op->type, op->name, op->args,
                              op->call_type, op->func, op->value_index, op->image, op->param);
        }
        return IRMutator::visit(op);
    }

public:
    map<string, vector<DeviceArgument>> inferred_args;
};

// Lower IR pass to create dynamic scheduler
Stmt create_overlay_schedule(Stmt s, const std::map<std::string, Function> &env) {
    // Extract the dependency vector between tasks
    // HL_OVERLAY_KERNEL is unset when compiling the tasks
    char *overlay_kenrel_name = getenv("HL_OVERLAY_KERNEL");
    if (overlay_kenrel_name == NULL) {
        debug(4) << "Overlay main body mutating...\n";

        // Collecting dependency information
        for (auto e : env) {
            // If there is no arg map for IP functions
            if (e.second.overlay().definition().argItems().size() == 0) {
                debug(4) << "Cannot find inferred args of function "
                         << e.second.name() << "...\n";
                return s;
            }

            string func_name = e.second.name();
            debug(4) << "Processing Overlay Task " << func_name << "...\n";
            debug(4) << "Extracting Task dependency...\n";

            // Get enqueued task information
            // The dependencies vector used to create app
            map<int, vector<depInfo>> deps;

            int task_id = 0;

            // Get task Funcs added by enqueue()
            // enqueue() function also records the actual input items
            for (auto &task : e.second.overlay().definition().taskItems()) {
                debug(4) << "Task" << task_id << ": " << task.name() << "\n";

                // Create dep vector for the specific task
                vector<depInfo> dep_tasks;
                for (auto &kv : task.definition().schedule().task_deps()) {
                    depInfo dep_task;
                    dep_task.task_id = kv.first;
                    debug(4) << dep_task.task_id << ": (";

                    for (int k = 0; k < (signed)kv.second.size(); k++) {
                        auto &expr = kv.second[k];
                        // Iteration var
                        if (expr.as<Variable>()) {
                            dep_task.iter_vars.push_back(expr);
                            // Iteration var depending distance
                        } else if (expr.as<IntImm>()) {
                            int scalar = expr.as<IntImm>()->value;
                            dep_task.distances.push_back(scalar);
                        } else {
                            dep_task.conds.push_back(expr);
                        }
                        debug(4) << expr << ", ";
                    }
                    dep_tasks.push_back(dep_task);
                    debug(4) << ")\n";
                }
                deps[task_id] = dep_tasks;
                task_id++;
            }

            // Get kernel ips (device) argument information
            auto &inferred_args = e.second.overlay().definition().argItems();
            for (auto &kv : inferred_args) {
                debug(4) << kv.first << ": ";
                for (auto &v : kv.second)
                    debug(4) << v.name << ", ";
                debug(4) << "\n";
            }

            // Map from task id to enqueued args (buffers)
            inputMap task_to_enqueued_args = e.second.overlay().definition().inputItems();
            // Map for hidden args assignment (e.g. A.stride --> args.constant[0])
            argAssignment arg_assignment = e.second.overlay().definition().assignMap();
            // Map from task id to queue index
            map<int, int> task2queue = e.second.overlay().definition().task2queue();
            // Mutate the main loop body
            // 1. Format overlay instrinsics for task dispatching and deps
            // 2. Format actual input args for each ip kernel function
            OverlayIntrinsicUpdating update(inferred_args, deps, task2queue, task_to_enqueued_args, arg_assignment);
            s = update.mutate(s);

            // Add a end signal updater at end of the outer loop
            OverlayLoopUpdating insert(update.outermost_loop);
            return insert.mutate(s);
        }
    }

    // Mutate the Kernel IP functions
    // 1. Extract the inferred args and push into overlay
    // 2. Create a map for arg mapping in the overlay
    //    Example:
    //        LU.stride.1 --> (args.constant[0])
    //        LU --> (args.args[0])
    //    This information will be used to match the actual args
    //    provided by the enqueue() API
    //
    if (overlay_kenrel_name != NULL) {
        LoopArgsInference in;
        debug(4) << "Analyzing the input kernel IP function... \n"
                 << in.mutate(s);
        user_assert(in.inferred_args.size() > 0) << "Function " << string(overlay_kenrel_name)
                                                 << " does not have any valid arguments in the overlay";

        // Pushing the inferred arg list and
        // argument assignment map into overlay
        int curr_queue_index = -1;
        string env_key_name;
        for (auto e : env) {
            auto name = task_renaming(e.first);
            if (name != string(overlay_kenrel_name)) {
                debug(4) << "Locating kernel " << string(overlay_kenrel_name) << " (" << e.first << ")\n";
                continue;
            }
            env_key_name = e.first;
            debug(4) << "Loading inferred args into overlay from func "
                     << e.first << "...\n";
            for (auto &kv : in.inferred_args) {
                debug(4) << "Overlay add arg mapping: "
                         << kv.first << ":" << e.first << "\n";
                e.second.overlay().definition().argItems()[kv.first] = kv.second;
            }
            curr_queue_index = e.second.overlay().definition().kernelNames().size();
            e.second.overlay().definition().kernelNames().push_back(overlay_kenrel_name);
            break;
        }

        // Reading the symbolic args from the function
        vector<Argument> sym_args;
        for (auto e : env) {
            auto func_name = task_renaming(e.first);
            if (func_name == overlay_kenrel_name) {
                sym_args = e.second.definition().schedule().cmd_params()[0].args;
                break;
            }
        }

        // Argument Inference and Body Mutation
        //   1. Add task receiver at the beginning of the kernel
        //   2. Add ack sender at the end of the kernel
        internal_assert(sym_args.size() != 0);
        internal_assert(curr_queue_index != -1);
        map<string, string> arg_map;
        map<string, int> arg_pos;
        IpFuncBodyUpdate update(in.inferred_args, overlay_kenrel_name,
                                curr_queue_index, sym_args, arg_map, arg_pos);
        s = update.mutate(s);
        internal_assert(env.count(env_key_name));

        env.at(env_key_name).overlay().definition().assignMap()[curr_queue_index].arg_map = arg_map;
        env.at(env_key_name).overlay().definition().assignMap()[curr_queue_index].arg_pos = arg_pos;
        return s;
    }

    return s;
}
} // namespace Internal
} // namespace Halide
