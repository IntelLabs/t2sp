#include <algorithm>
#include <sstream>
#include <fstream>

#include "../../Halide/src/CSE.h"
#include "../../Halide/src/CodeGen_Internal.h"
#include "CodeGen_OneAPI_Dev.h"
#include "../../Halide/src/Debug.h"
#include "../../Halide/src/EliminateBoolVectors.h"
#include "../../Halide/src/EmulateFloat16Math.h"
#include "../../Halide/src/ExprUsesVar.h"
#include "../../Halide/src/IRMutator.h"
#include "../../Halide/src/IROperator.h"
#include "../../Halide/src/Simplify.h"
#include "../../Halide/src/Substitute.h"
#include "DebugPrint.h"
#include "Utilities.h"

namespace Halide {
namespace Internal {

using std::ostringstream;
using std::sort;
using std::string;
using std::vector;

CodeGen_OneAPI_Dev::CodeGen_OneAPI_Dev(Target t)
    : one_clc(src_stream_oneapi, t) {
}


void CodeGen_OneAPI_Dev::add_kernel(Stmt s,
                                    const string &name,
                                    const vector<DeviceArgument> &args) {
    debug(2) << "CodeGen_OneAPI_Dev::compile " << name << "\n";

    // TODO: do we have to uniquify these names, or can we trust that they are safe?
    cur_kernel_name_oneapi = name;
    one_clc.add_kernel(s, name, args);
}


vector<char> CodeGen_OneAPI_Dev::compile_to_src() {
    // const Target &target = one_clc.get_target();
    // if (target.has_feature(Target::OneAPI)) {
    //     compile_to_aocx_oneapi(src_stream_oneapi);
    // }

    string str = src_stream_oneapi.str();
    debug(1) << "OneAPI kernel:\n"
             << str << "\n";
    vector<char> buffer(str.begin(), str.end());
    // buffer.push_back(0); // Not needed as we do not compile to a binary rn
    return buffer;
}

vector<char> CodeGen_OneAPI_Dev::compile_to_src_module(const LoweredFunc &f) {

    // Use a special CodeGen_OneAPI_C function to create the function wrapper
    debug(1) << "Creating function wrapper around kernels\n";
    string str = src_stream_oneapi.str();
    string src = one_clc.compile_oneapi_lower(f, str);

    debug(1) << "OneAPI kernel:\n"
             << src << "\n";
    vector<char> buffer(src.begin(), src.end());
    return buffer;
}


/* Methods only for generating OpenCL code for Intel FPGAs */
// (TODO) Replace with method to create dpcpp code and compile
void CodeGen_OneAPI_Dev::compile_to_aocx_oneapi(std::ostringstream &src_stream) {
    // If BITSTREAM env var or $HOME/tmp/a.aocx indicates an existing file, do nothing.
    // Otherwise, dump the source code to ~/tmp/a.cl and compile it to ~/tmp/a.aocx.
    char *aocx_name = getenv("BITSTREAM");
    std::string bitstream_file = (aocx_name != NULL) ? std::string(aocx_name) : (std::string(getenv("HOME")) + "/tmp/a.aocx");
    user_assert(ends_with(bitstream_file, ".aocx")) << " Bitstream file name expected to end with \".aocx\"\n";
    user_assert(bitstream_file.size() < 300) << "The full name of the bitstream file is too long. "
            << "Consider to define a environment variable BITSTREAM within 300 characters instead. Current file name:\n"
            << bitstream_file << "\n";

    // If HL_OVERLAY_KERNEL is set, only compile the CL files in local
    char *overlay_kernel_name = getenv("HL_OVERLAY_KERNEL");
    if (overlay_kernel_name != NULL) {
        auto cl_name = std::string(overlay_kernel_name) + ".cl";
        std::ofstream fp(cl_name.c_str(), std::ios::out);
        internal_assert(fp) << "Error: failed to open file " << cl_name << " for output.\n";
        fp << src_stream.str() << "\n";
        fp.close();
        return;
    }


    // Bitstream was generated ahead of time. Check the file exists.
    // Note that we do not check if the aocx file is really good or up to date with the
    // source code. So if you want to regenerate the aocx file, delete it before invoking Halide.
    std::ifstream file(bitstream_file, std::ios::in);
    if (file.good()) {
        user_warning << "Bitstream " << bitstream_file << " exists. No re-compilation.\n";
        return;
    }

    // Create the source file
    std::string cl_name = replace_all(bitstream_file, ".aocx", ".cl");
    {
        std::string str = src_stream.str();
        std::ofstream fp(cl_name.c_str(), std::ios::out);
        internal_assert(fp) << "Error: failed to open file " << cl_name << " for output.\n";
        fp << str << "\n";
        fp.close();

        if (!one_clc.get_target().has_feature(Target::EnableSynthesis)) {
            return;
        }
    }

    // If environment var WAIT is set, give the programmer a chance to modify a.cl
    char *wait = getenv("WAIT");
    if (wait) {
        std::cout << "Modify " << cl_name << ". Then press c to continue\n";
        {
            char c = getchar();
            while (c != 'c') {
                c = getchar();
            }
        }
    }

    // Compile the OpenCL file into a bitstream.
    std::stringstream command;
    char *aoc_option = getenv("AOC_OPTION");
    command << "aoc " << ((aoc_option == NULL) ? "" : aoc_option) << " -g " << cl_name << " -o " << bitstream_file;
    debug(4) << "Compiling for bitstream: " << command.str() << "\n";
    int ret = system(command.str().c_str());
    user_assert(ret != -1) << "Failed in compiling " << cl_name;
}

string CodeGen_OneAPI_Dev::get_current_kernel_name() {
    return cur_kernel_name_oneapi;
}

void CodeGen_OneAPI_Dev::dump() {
    std::cerr << src_stream_oneapi.str() << std::endl;
}

std::string CodeGen_OneAPI_Dev::print_gpu_name(const std::string &name) {
    return name;
}


void CodeGen_OneAPI_Dev::init_module() {
    debug(2) << "OneAPI device codegen init_module\n";

    // wipe the internal kernel source
    src_stream_oneapi.str("");
    src_stream_oneapi.clear();

    const Target &target = one_clc.get_target();

    // Check whether it's compiled for ip kernel
    char *overlay_kenrel = getenv("HL_OVERLAY_KERNEL");
    if (overlay_kenrel != NULL) return;

    // This identifies the program as OpenCL C (as opposed to SPIR).
    src_stream_oneapi << "/*OneAPI C++ " << target.to_string() << "*/\n";

    // Setting up OneAPI DPCPP dependencies
    src_stream_oneapi << "#include <CL/sycl.hpp>\n";
    src_stream_oneapi << "#include <sycl/ext/intel/fpga_extensions.hpp>\n";
    src_stream_oneapi << "#include \"dpc_common.hpp\"\n";
    src_stream_oneapi << "#include \"pipe_array.hpp\"\n";
    src_stream_oneapi << "using namespace sycl;\n\n";

    src_stream_oneapi << "#pragma OPENCL FP_CONTRACT ON\n";

    // Write out the Halide math functions.
    src_stream_oneapi << "// #define float_from_bits(x) as_float(x)\n"
               << "#define float_from_bits(x) cl_float(x)\n"
               << "inline float nan_f32() { return NAN; }\n"
               << "inline float neg_inf_f32() { return -INFINITY; }\n"
               << "inline float inf_f32() { return INFINITY; }\n"
               << "inline bool is_nan_f32(float x) {return isnan(x); }\n"
               << "inline bool is_inf_f32(float x) {return isinf(x); }\n"
               << "inline bool is_finite_f32(float x) {return isfinite(x); }\n"
               << "#define sqrt_f32 sqrt \n"
               << "#define sin_f32 sin \n"
               << "#define cos_f32 cos \n"
               << "#define exp_f32 exp \n"
               << "#define log_f32 log \n"
               << "#define abs_f32 fabs \n"
               << "#define floor_f32 floor \n"
               << "#define ceil_f32 ceil \n"
               << "#define round_f32 round \n"
               << "#define trunc_f32 trunc \n"
               << "#define pow_f32 pow\n"
               << "#define asin_f32 asin \n"
               << "#define acos_f32 acos \n"
               << "#define tan_f32 tan \n"
               << "#define atan_f32 atan \n"
               << "#define atan2_f32 atan2\n"
               << "#define sinh_f32 sinh \n"
               << "#define asinh_f32 asinh \n"
               << "#define cosh_f32 cosh \n"
               << "#define acosh_f32 acosh \n"
               << "#define tanh_f32 tanh \n"
               << "#define atanh_f32 atanh \n"
               << "#define fast_inverse_f32 native_recip \n"
               << "#define fast_inverse_sqrt_f32 native_rsqrt \n";

    // __shared always has address space __local.
    // src_stream_oneapi << "#define __address_space___shared __local\n";

    if (target.has_feature(Target::CLDoubles)) {
        src_stream_oneapi << "#pragma OPENCL EXTENSION cl_khr_fp64 : enable\n"
                   << "inline bool is_nan_f64(double x) {return isnan(x); }\n"
                   << "inline bool is_inf_f64(double x) {return isinf(x); }\n"
                   << "inline bool is_finite_f64(double x) {return isfinite(x); }\n"
                   << "#define sqrt_f64 sqrt\n"
                   << "#define sin_f64 sin\n"
                   << "#define cos_f64 cos\n"
                   << "#define exp_f64 exp\n"
                   << "#define log_f64 log\n"
                   << "#define abs_f64 fabs\n"
                   << "#define floor_f64 floor\n"
                   << "#define ceil_f64 ceil\n"
                   << "#define round_f64 round\n"
                   << "#define trunc_f64 trunc\n"
                   << "#define pow_f64 pow\n"
                   << "#define asin_f64 asin\n"
                   << "#define acos_f64 acos\n"
                   << "#define tan_f64 tan\n"
                   << "#define atan_f64 atan\n"
                   << "#define atan2_f64 atan2\n"
                   << "#define sinh_f64 sinh\n"
                   << "#define asinh_f64 asinh\n"
                   << "#define cosh_f64 cosh\n"
                   << "#define acosh_f64 acosh\n"
                   << "#define tanh_f64 tanh\n"
                   << "#define atanh_f64 atanh\n";
    }

    if (target.has_feature(Target::CLHalf)) {
        src_stream_oneapi << "#pragma OPENCL EXTENSION cl_khr_fp16 : enable\n"
                   << "inline half half_from_bits(unsigned short x) {return __builtin_astype(x, half);}\n"
                   << "inline half nan_f16() { return half_from_bits(32767); }\n"
                   << "inline half neg_inf_f16() { return half_from_bits(31744); }\n"
                   << "inline half inf_f16() { return half_from_bits(64512); }\n"
                   << "inline bool is_nan_f16(half x) {return isnan(x); }\n"
                   << "inline bool is_inf_f16(half x) {return isinf(x); }\n"
                   << "inline bool is_finite_f16(half x) {return isfinite(x); }\n"
                   << "#define sqrt_f16 sqrt\n"
                   << "#define sin_f16 sin\n"
                   << "#define cos_f16 cos\n"
                   << "#define exp_f16 exp\n"
                   << "#define log_f16 log\n"
                   << "#define abs_f16 fabs\n"
                   << "#define floor_f16 floor\n"
                   << "#define ceil_f16 ceil\n"
                   << "#define round_f16 round\n"
                   << "#define trunc_f16 trunc\n"
                   << "#define pow_f16 pow\n"
                   << "#define asin_f16 asin\n"
                   << "#define acos_f16 acos\n"
                   << "#define tan_f16 tan\n"
                   << "#define atan_f16 atan\n"
                   << "#define atan2_f16 atan2\n"
                   << "#define sinh_f16 sinh\n"
                   << "#define asinh_f16 asinh\n"
                   << "#define cosh_f16 cosh\n"
                   << "#define acosh_f16 acosh\n"
                   << "#define tanh_f16 tanh\n"
                   << "#define atanh_f16 atanh\n";
    }

    if (target.has_feature(Target::CLAtomics64)) {
        src_stream_oneapi << "#pragma OPENCL EXTENSION cl_khr_int64_base_atomics : enable\n";
        src_stream_oneapi << "#pragma OPENCL EXTENSION cl_khr_int64_extended_atomics : enable\n";
    }

    src_stream_oneapi << '\n';

    one_clc.add_common_macros(src_stream_oneapi);

    // Not needed for OneAPI DPCPP SYCL
    // if (!target.has_feature(Target::IntelFPGA)) {
    //     Add at least one kernel to avoid errors on some implementations for functions
    //     without any GPU schedules.
    //     src_stream_oneapi << "__kernel void _at_least_one_kernel(int x) { }\n";
    //     // src_stream_oneapi << "// __kernel void _at_least_one_kernel(int x) { }\n";
    // }

    if (target.has_feature(Target::IntelFPGA)) {
        //enable channels support
        src_stream_oneapi << "#pragma OPENCL EXTENSION cl_intel_channels : enable\n";
        //src_stream_oneapi << "// #pragma OPENCL EXTENSION cl_intel_channels : enable\n";
    }

    char *kernel_num = getenv("HL_KERNEL_NUM");
    if (overlay_kenrel == NULL && kernel_num != NULL) {
        int ip_num = std::atoi(kernel_num);
        src_stream_oneapi << "#include \"ihc_apint.h\"\n";
        char *space_dim = getenv("HL_SPACE_DIM");
        char *overlay_dtype = getenv("HL_OVERLAY_DTYPE");
        src_stream_oneapi << "#define DTYPE  " << string(overlay_dtype)
            << "  // default data type\n";

        src_stream_oneapi << "#define K      " << ip_num << "   // number of IPs\n";
        src_stream_oneapi << "#define M      " << std::atoi(space_dim) << "   // number of iter space var\n";
        src_stream_oneapi << R"(#define N      16  // number of constant parameters
#define O      5   // max number of out degree
#define SIZE   16  // max number of tasks in the graph

// Define task index
typedef struct index {
    int     task_id;
    int     space_id[M];
} index_t;

// argument to tasks
typedef struct arg {
    __global DTYPE*  args0;
    __global DTYPE*  args1;
    __global DTYPE*  args2;
    __global DTYPE*  args4;
    __global DTYPE*  args5;
    DTYPE            constants[N];
    int              num_of_args;
    index_t          finish;
} arg_t;

// Task and dependency
typedef struct task {
    int         queue;
    index_t     index;
    index_t     deps[O];
    arg_t       inputs;
    int         num_of_deps;
    bool        done;
    bool        last;
} task_t;

// Task graph
typedef struct graph {
    uint16_t     slots;        // valid bit used for allocation
    uint16_t     issue;        // valid bit used for task issuance
    task_t       tasks[SIZE];  // pending tasks
    uint16_t     deps[SIZE];
} graph_t;

// Command queue from app to scheduler
channel task_t qt __attribute__((depth(64)));
channel int    qf __attribute__((depth(64)));

)";
        // Print req and ack channels
        src_stream_oneapi << "channel arg_t q[" << ip_num << "] __attribute__((depth(64)));\n";
        src_stream_oneapi << "channel index_t q_ret[" << ip_num << "] __attribute__((depth(64)));\n";
        src_stream_oneapi << R"(
// Map from task to array index in the graph
int map( graph_t* graph, index_t key) {
    for (int index = 0; index < SIZE; index++) {
      index_t k = graph->tasks[index].index;
      if (k.task_id == key.task_id) {
        bool match = true;
        for (int i = 0; i < M; i++) {
            if (k.space_id[i] != key.space_id[i]) {
                match = false;
            }
        }
        if (match) {
            return index;
        }
      }
    }
    return -1;
}

// Allocate a task in the graph
void allocate(graph_t* graph, task_t task) {
   int index = 0;
   while (graph->slots & (1 << index)) {
       index++;
   }
   graph->slots = graph->slots | (1 << index);
   graph->tasks[index] = task;

   // set up dependency vector
   // use bitmap to determine the parents op index in the graph
   for (int i = 0; i < task.num_of_deps; i++) {
     int dep_op_index = map(graph, task.deps[i]);
     // break if the op has no dependency
     if (dep_op_index == -1) break;
     if (!graph->tasks[dep_op_index].done) {
       graph->deps[index] |= (1 << dep_op_index);
     }
   }
}

// Find out tasks with all deps satisified
bool dispatch(graph_t* graph, task_t* task) {
    for (int i = 0; i < SIZE; i++) {
        // valid task slot with no dependency
        if ((graph->slots & (1 << i)) && graph->deps[i] == 0x0000) {
            if (!(graph->issue & (1 << i))) {
              graph->issue |= (1 << i);
              *task = graph->tasks[i];
              return true;
            }
        }
    }
    return false;
}

// Update the graph
void update(graph_t* graph, index_t key) {
    int index = map(graph, key);
    // invalidate the slot
    graph->slots &= ~(1 << index);
    graph->issue &= ~(1 << index);

    // set done flag
    graph->tasks[index].done = true;

    // check depending children tasks
    for (int i = 0; i < SIZE; i++) {
      graph->deps[i] &= ~(1 << index);
    }
}

// Free-running scheduler
__attribute__((max_global_work_dim(0)))
__attribute__((autorun))
__kernel void scheduler() {

    // create task graph
    graph_t graph;
    graph.slots = 0x0000;
    graph.issue = 0x0000;

    int task_count = 0;
    bool task_not_end = true;

    // perform scheduling
    while(1) {

        while (task_not_end) {
            if (graph.slots == 0xFFFF) break;   // stop allocating when graph is full
            task_t task = read_channel_intel(qt);   // read task generated by the application

            if (task.last) {
                // printf("Received the last task....\n");
                task_not_end = false;
            } else {
                // printf("Received a new task....\n");
                allocate(&graph, task);
                task_count += 1;
            }
        }

        mem_fence(CLK_CHANNEL_MEM_FENCE);
        while (1) {
            task_t task;
            bool new_task = dispatch(&graph, &task);    // dispatch the pending tasks
            if (!new_task) break;

            switch (task.queue) {
)";
        for (int t = 0; t < ip_num; t++) {
            src_stream_oneapi << string(16, ' ') << "case " << t << ": {\n";
            src_stream_oneapi << string(20, ' ') << "write_channel_intel(q["
                << t << "], task.inputs);\n";
            src_stream_oneapi << string(20, ' ') << "break;\n";
            src_stream_oneapi << string(16, ' ') << "}\n";
        }
        src_stream_oneapi << R"(
            }
        }

        // update the graph with ack information
        mem_fence(CLK_CHANNEL_MEM_FENCE);
        for (int i = 0; i < K; i++) {
            bool ret_valid = true;
            index_t ret;
            switch (i) {
)";
        for (int t = 0; t < ip_num; t++) {
            src_stream_oneapi << string(16, ' ') << "case " << t
                << ": {\n";
            src_stream_oneapi << string(20, ' ') << "ret = read_channel_nb_intel(q_ret["<< t <<"], &ret_valid); \n";
            src_stream_oneapi << string(20, ' ') << "while (ret_valid) {\n";
            src_stream_oneapi << string(24, ' ') << "update(&graph, ret);\n"
                       << string(24, ' ') << "ret = read_channel_nb_intel(q_ret["<< t <<"], &ret_valid); \n";
            src_stream_oneapi << string(20, ' ') << "}\n"
                       << string(20, ' ') << "break;\n";
            src_stream_oneapi << string(16, ' ') << "}\n";
        }
        src_stream_oneapi << R"(
            }
        }

        // send the ending signal
        mem_fence(CLK_CHANNEL_MEM_FENCE);
        if (!task_not_end && graph.slots == 0x0000) {
            write_channel_intel(qf, task_count);
        }
    }
}

)";
        // Include the kernel ip functions
        char *overlay_kenrel_files = getenv("HL_OVERLAY_FILES");
        user_assert(overlay_kenrel_files != NULL) << "HL_OVERLAY_FILES empty...\n";

        std::string text(overlay_kenrel_files);
        std::size_t start = 0, end = 0;
        while ((end = text.find(" ", start)) != std::string::npos) {
        src_stream_oneapi << "#include \"" << text.substr(start, end - start) << ".cl\"\n";
        start = end + 1;
        }
        src_stream_oneapi << "#include \"" << text.substr(start) << ".cl\"\n\n";

    }

    cur_kernel_name_oneapi = "";
}



//=============== CodeGen_OpenCL_OneAPI_C ==> CodeGen_OneAPI_C ===============


namespace {
string simt_intrinsic(const string &name) {
    if (ends_with(name, ".__thread_id_x")) {
        return "get_local_id(0)";
    } else if (ends_with(name, ".__thread_id_y")) {
        return "get_local_id(1)";
    } else if (ends_with(name, ".__thread_id_z")) {
        return "get_local_id(2)";
    } else if (ends_with(name, ".__thread_id_w")) {
        return "get_local_id(3)";
    } else if (ends_with(name, ".__block_id_x")) {
        return "get_group_id(0)";
    } else if (ends_with(name, ".__block_id_y")) {
        return "get_group_id(1)";
    } else if (ends_with(name, ".__block_id_z")) {
        return "get_group_id(2)";
    } else if (ends_with(name, ".__block_id_w")) {
        return "get_group_id(3)";
    }
    internal_error << "simt_intrinsic called on bad variable name: " << name << "\n";
    return "";
}
}  // namespace

// vector_elements are not used for OneAPI
// namespace {
//     // Mapping of integer vector indices to OpenCL ".s" syntax.
//     const char *vector_elements = "0123456789ABCDEF";
// }  // namespace

namespace {
struct BufferSize {
    string name;
    size_t size;

    BufferSize()
        : size(0) {
    }
    BufferSize(string name, size_t size)
        : name(name), size(size) {
    }

    bool operator<(const BufferSize &r) const {
        return size < r.size;
    }
};
}  // namespace

// Check if this is a piece of code for an autorun kernel.
class IsAutorun : public IRVisitor {
    using IRVisitor::visit;
    void visit(const For *op) override {
        if (ends_with(op->name, ".autorun.run_on_device")) {
            is_autorun = true;
            return;
        }
        IRVisitor::visit(op);
    }
    public:
        bool is_autorun;
        IsAutorun() : is_autorun(false) {}
};


// Check if this is a piece of code for an run_on_device kernel
class IsRunOnDevice : public IRVisitor {
    using IRVisitor::visit;
    void visit(const For *op) override {
        if (ends_with(op->name, ".run_on_device")) {
            is_run_on_device = true;
            return;
        }
        IRVisitor::visit(op);
    }
    public:
        bool is_run_on_device;
        IsRunOnDevice() : is_run_on_device(false) {}
};

class KernelStore : public IRVisitor {
    using IRVisitor::visit;
public:
    bool stores_to_memory;

    KernelStore() : stores_to_memory(false) {}

    void visit(const Store *op) override {
        stores_to_memory = true;
        return;
    }
};

class KernelLoad : public IRVisitor {
    using IRVisitor::visit;
public:
    bool stores_to_memory;

    KernelLoad() : stores_to_memory(false) {}

    void visit(const Load *op) override {
        stores_to_memory = true;
        return;
    }
};





std::string CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::vector_index_to_string(int idx) {
    std::ostringstream oss;
    if (idx >= 10 && idx < 16) {
        char c = idx - 10 + 'a';
        oss << string((const char *)&c, 1);
    } else {
        oss << idx;
    }
    return std::move(oss.str());
}

bool CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::is_standard_opencl_type(Type type) {
    int bits = type.bits();
    int lanes = type.lanes();
    bool standard_bits = false;
    bool standard_lanes = false;
    if (type.is_float()) {
        if ((bits == 16 && target.has_feature(Target::CLHalf)) ||
            (bits == 32) || (bits == 64)) {
            standard_bits = true;
        }
    } else {
        if ((bits == 1 && lanes == 1) || (bits == 8) ||
            (bits == 16) || (bits == 32) || (bits == 64)) {
            standard_bits = true;
        }
    }
    if ((lanes == 1) || (lanes == 2) || (lanes == 3) ||(lanes == 4) ||
        (lanes == 8) || (lanes == 16)) {
        standard_lanes = true;
    }
    return standard_bits && standard_lanes;
}

string CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::print_type(Type type, AppendSpaceIfNeeded space) {
    ostringstream oss;
    if (type.is_generated_struct()) {
        int type_id = type.bits();
        const std::pair<std::string, std::vector<Type>> &entry = GeneratedStructType::structs[type_id];
        string struct_name = entry.first;
        oss << struct_name;
    } else if (!is_standard_opencl_type(type)) {
        Type basic_type = type.with_lanes(1);
        string type_name = print_type(basic_type);
        oss << type_name;
    } else if (type.is_float()) {
        if (type.bits() == 16) {
            user_assert(target.has_feature(Target::CLHalf))
                << "OpenCL kernel uses half type, but CLHalf target flag not enabled\n";
            oss << "half";
        } else if (type.bits() == 32) {
            oss << "float";
        } else if (type.bits() == 64) {
            oss << "double";
        } else {
            user_error << "Can't represent a float with this many bits in OpenCL C: " << type << "\n";
        }

    } else {
        if (type.is_uint() && type.bits() > 1) oss << 'u';
        switch (type.bits()) {
        case 1:
            internal_assert(type.lanes() == 1) << "Encountered vector of bool\n";
            oss << "bool";
            break;
        case 8:
            oss << "char";
            break;
        case 16:
            oss << "short";
            break;
        case 32:
            oss << "int";
            break;
        case 64:
            oss << "long";
            break;
        default:
            user_error << "Can't represent an integer with this many bits in OpenCL C: " << type << "\n";
        }
    }
    if (type.lanes() != 1) {
        switch (type.lanes()) {
        case 2:
        case 3:
        case 4:
        case 8:
        case 16:
            oss << type.lanes();
            break;
        default:
            if (GeneratedStructType::nonstandard_vector_type_exists(type)) {
                oss << type.lanes();
                break;
            } else {
                user_error << "Unsupported vector width in OpenCL C: " << type << "\n";
            }
        }
    }
    if (space == AppendSpace) {
        oss << ' ';
    }
    return oss.str();
}

// These are built-in types in OpenCL
void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::add_vector_typedefs(const std::set<Type> &vector_types) {
}

Expr CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::DefineVectorStructTypes::mutate(const Expr &op) {
    Type type = op.type();

    if (type.lanes() > 1 && !parent->is_standard_opencl_type(type)) {
        // This is a nonstandard vector type
        if (!GeneratedStructType::nonstandard_vector_type_exists(type)) {
            GeneratedStructType::record_nonstandard_vector_type(type);
            // Define a vector type like this:
            //   typedef union {
            //      float __attribute__ ((aligned(4*17 rounded up to power of 2))) s[17];
            //      struct {float s0, s1, s2, s3, s4, s5, s6,s7, s8, s9, sa, sb, sc, sd, se, sf, s16;};
            //   } float17;
            // The first 16 elements follow the standard OpenCL vector notation.
             std::ostringstream oss;
             oss << "typedef union {\n"
                 << parent->print_type(type.element_of())
                 << " __attribute__ ((aligned(" << closest_power_of_two(type.lanes() * type.with_lanes(1).bytes())
                 << ")))" << " s[" << type.lanes() << "];\n"
                 << "struct {" << parent->print_type(type.element_of());
             for (int i = 0; i < type.lanes(); i++) {
                 oss << (i == 0 ? "" : ", ") << " s" << parent->vector_index_to_string(i);
             }
             oss << ";};\n"
                 << "} " << parent->print_type(type.element_of()) << type.lanes() << ";\n";
             vectors += oss.str();
        }
    } else if (type.is_generated_struct()) {
        std::ostringstream oss;
        int type_id = type.bits();
        if (std::find(parent->defined_struct_ids.begin(), parent->defined_struct_ids.end(), type_id) == parent->defined_struct_ids.end()) {
            parent->defined_struct_ids.push_back(type_id);
            const std::pair<std::string, std::vector<Type>> &entry = GeneratedStructType::structs[type_id];
            // Define a struct like: struct foo {int f0, char f1, int f2};
            string struct_name = entry.first;
            oss << "typedef struct {\n";
            for (size_t i = 0; i < entry.second.size(); i++) {
                oss << "\t" << parent->print_type(entry.second[i]) << " f" << i << ";\n";
            }
            oss << "} " << struct_name << ";\n";
            structs += oss.str();
        }
    }

    return IRMutator::mutate(op);
}

Stmt CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::DefineVectorStructTypes::mutate(const Stmt &op) {
    return IRMutator::mutate(op);
}

string CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::print_reinterpret(Type type, Expr e) {
    ostringstream oss;
    oss << "as_" << print_type(type) << "(" << print_expr(e) << ")";
    return oss.str();
}






void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::visit(const For *loop) {
    if (is_gpu_var(loop->name)) {
        internal_assert((loop->for_type == ForType::GPUBlock) ||
                        (loop->for_type == ForType::GPUThread))
            << "kernel loop must be either gpu block or gpu thread\n";
        internal_assert(is_zero(loop->min));

        stream << get_indent() << print_type(Int(32)) << " " << print_name(loop->name)
               << " = " << simt_intrinsic(loop->name) << ";\n";

        loop->body.accept(this);

    } else if (ends_with(loop->name, ".run_on_device")) {
        // The loop just tells the compiler to generate an OpenCL kernel for
        // the loop body.
        std::string name = extract_first_token(loop->name);
        if (shift_regs_allocates.find(name) != shift_regs_allocates.end()) {
            for (size_t i = 0; i < shift_regs_allocates[name].size(); i++) {
                stream << get_indent() << shift_regs_allocates[name][i];
            }
        }
        name += "_temp";
        if (shift_regs_allocates.find(name) != shift_regs_allocates.end()) {
            for (size_t i = 0; i < shift_regs_allocates[name].size(); i++) {
                stream << get_indent() << shift_regs_allocates[name][i];
            }
        }
        loop->body.accept(this);
    } else if(ends_with(loop->name, ".infinite")){
        stream << get_indent() << "while(1)\n";
        open_scope();
        loop->body.accept(this);
        close_scope("while "+print_name(loop->name));
    } else if (ends_with(loop->name,"remove")){
        loop->body.accept(this);
    } else if (loop->for_type == ForType::DelayUnroll) {
        Expr extent = simplify(loop->extent);
        Stmt body = loop->body;
        const IntImm *e = extent.as<IntImm>();
        user_assert(e)
                << "Can only unroll for loops over a constant extent.\n"
                << "Loop over " << loop->name << " has extent " << extent << ".\n";
        if (e->value == 1) {
            user_warning << "Warning: Unrolling a for loop of extent 1: " << loop->name << "\n";
        }

        Stmt iters;
        for (int i = 0; i < e->value; i++) {
            Stmt iter = simplify(substitute(loop->name, loop->min + i, body));
            iter.accept(this);
        }
    } else if (loop->for_type == ForType::PragmaUnrolled) {
        stream << get_indent() << "#pragma unroll\n";
        CodeGen_C::visit(loop);
    } else {
        /*
        If not explicitly define a env variable(DELAYUNROLL or PRAGMAUNROLL), the unrolling strategy will be automatically
        decided by the compiler.
        To physically unroll a loop, we require that:
            1. there is a conditional execution of channel read/write inside the current loop, e.g.
                unrolled k = 0 to K
                    if (cond)
                        read/write channel
            2. there is a irregular loop inside the current loop and the irregular bound depends on current loop var, e.g.
                unrolled k = 0 to K
                    unrolled j = k to J
            3. there is a shift register whose bounds depends on current loop var, e.g.,
                float _Z_shreg_0, _Z_shreg_1, _Z_shreg_2, _Z_shreg_3;
                unrolled j = 0 to J
                    access _Z_shreg_j   // j needs to be replaced with 0, 1, 2, 3

        For other cases, we simply insert the #pragma unroll directive before a loop. The offline compiler attempts to fully
        unroll the loop.
        */
        user_assert(loop->for_type != ForType::Parallel) << "Cannot use parallel loops inside OpenCL kernel\n";
        if (loop->for_type == ForType::Unrolled) {
            CheckConditionalChannelAccess checker(this, loop->name);
            loop->body.accept(&checker);

            // Check the condition 1 and 2
            bool needs_unrolling = checker.conditional_access || checker.irregular_loop_dep;
            // Check the condition 3
            for (auto &kv : space_vars) {
                for (auto &v : kv.second) {
                    if (v.as<Variable>()->name == loop->name)
                        needs_unrolling |= true;
                }
            }
            if (needs_unrolling) {
                Expr extent = simplify(loop->extent);
                Stmt body = loop->body;
                const IntImm *e = extent.as<IntImm>();
                user_assert(e)
                        << "Can only unroll for loops over a constant extent.\n"
                        << "Loop over " << loop->name << " has extent " << extent << ".\n";
                if (e->value == 1) {
                    user_warning << "Warning: Unrolling a for loop of extent 1: " << loop->name << "\n";
                }

                Stmt iters;
                for (int i = 0; i < e->value; i++) {
                    Stmt iter = simplify(substitute(loop->name, loop->min + i, body));
                    iter.accept(this);
                }
            } else {
                stream << get_indent() << "#pragma unroll\n";
                CodeGen_C::visit(loop);
            }
        } else {
            CodeGen_C::visit(loop);
        }
    }
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::CheckConditionalChannelAccess::visit(const IfThenElse *op) {
    bool old_cond = in_if_then_else;
    in_if_then_else = true;
    op->then_case.accept(this);

    if (op->else_case.defined()) {
        in_if_then_else = true;
        op->else_case.accept(this);
    }
    in_if_then_else = old_cond;
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::CheckConditionalChannelAccess::visit(const Call *op) {
    if (op->is_intrinsic(Call::read_channel) || op->is_intrinsic(Call::write_channel) ||
        op->is_intrinsic(Call::read_channel_nb) || op->is_intrinsic(Call::write_channel_nb)) {
        conditional_access |= in_if_then_else;
    }
    IRVisitor::visit(op);
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::CheckConditionalChannelAccess::visit(const For *op) {
    if (op->for_type == ForType::Serial || op->for_type == ForType::Unrolled) {
        if (!is_const(op->min)) {
            irregular_loop_dep = true;
            debug(4) << "Loop min: " << op->min << "\n";
            debug(4) << "Physically unroll loop " << current_loop_name << " because of the irregular loop "
                     << op->name << " inside. \n";
        } else if (!is_const(op->extent)) {
            irregular_loop_dep = true;
            debug(4) << "Loop extent: " << op->extent << "\n";
            debug(4) << "Physically unroll loop " << current_loop_name << " because of the irregular loop "
                     << op->name << " inside. \n";
        }
    }
    IRVisitor::visit(op);
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::visit(const Ramp *op) {
    string id_base = print_expr(op->base);
    string id_stride = print_expr(op->stride);

    ostringstream rhs;
    if (!is_standard_opencl_type(op->type)) {
        rhs << "(" << print_type(op->type.with_lanes(op->lanes)) << "){" << id_base;
        for (int i = 1; i < op->lanes; ++i) {
            rhs << ", (" << id_base << " + " << id_stride << " * " << i << ")";
        }
        rhs << "}";
    } else {
        rhs << id_base << " + " << id_stride << " * ("
            << print_type(op->type.with_lanes(op->lanes)) << "){0";
        // Note 0 written above.
        for (int i = 1; i < op->lanes; ++i) {
            rhs << ", " << i;
        }
        rhs << "}";
    }
    print_assignment(op->type.with_lanes(op->lanes), rhs.str());
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::visit(const Broadcast *op) {
    string id_value = print_expr(op->value);
    if (is_standard_opencl_type(op->type)) {
            print_assignment(op->type.with_lanes(op->lanes), id_value);
    } else {
        string s = "{";
        for (int i = 0; i < op->lanes; i++) {
            s += ((i == 0) ? "" : ", ") + id_value;
        }
        s += "}";
        print_assignment(op->type.with_lanes(op->lanes), s);
    }
}




string CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::get_memory_space(const string &buf) {
    // return "__address_space_" + print_name(buf);
    return "";
}

string CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::print_halide_buffer_name(const string &buf) {
    return print_name(buf) + "_halide_buffer";
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::visit(const Call *op) {
    if (op->is_intrinsic(Call::bool_to_mask)) {
        if (op->args[0].type().is_vector()) {
            // The argument is already a mask of the right width. Just
            // sign-extend to the expected type.
            op->args[0].accept(this);
        } else {
            // The argument is a scalar bool. Casting it to an int
            // produces zero or one. Convert it to -1 of the requested
            // type.
            Expr equiv = -Cast::make(op->type, op->args[0]);
            equiv.accept(this);
        }
    } else if (op->is_intrinsic(Call::cast_mask)) {
        // Sign-extension is fine
        Expr equiv = Cast::make(op->type, op->args[0]);
        equiv.accept(this);
    } else if (op->is_intrinsic(Call::select_mask)) {
        internal_assert(op->args.size() == 3);
        string cond = print_expr(op->args[0]);
        string true_val = print_expr(op->args[1]);
        string false_val = print_expr(op->args[2]);

        // Yes, you read this right. OpenCL's select function is declared
        // 'select(false_case, true_case, condition)'.
        ostringstream rhs;
        rhs << "select(" << false_val << ", " << true_val << ", " << cond << ")";
        print_assignment(op->type, rhs.str());
    } else if (op->is_intrinsic(Call::abs)) {
        if (op->type.is_float()) {
            ostringstream rhs;
            rhs << "abs_f" << op->type.bits() << "(" << print_expr(op->args[0]) << ")";
            print_assignment(op->type, rhs.str());
        } else {
            ostringstream rhs;
            rhs << "abs(" << print_expr(op->args[0]) << ")";
            print_assignment(op->type, rhs.str());
        }
    } else if (op->is_intrinsic(Call::absd)) {
        ostringstream rhs;
        rhs << "abs_diff(" << print_expr(op->args[0]) << ", " << print_expr(op->args[1]) << ")";
        print_assignment(op->type, rhs.str());
    } else if (op->is_intrinsic(Call::gpu_thread_barrier)) {
        stream << get_indent() << "barrier(CLK_LOCAL_MEM_FENCE);\n";
        print_assignment(op->type, "0");
    } else if (op->is_intrinsic(Call::shift_left) || op->is_intrinsic(Call::shift_right)) {
        // Some OpenCL implementations forbid mixing signed-and-unsigned shift values;
        // if the RHS is uint, quietly cast it back to int if the LHS is int
        if (op->args[0].type().is_int() && op->args[1].type().is_uint()) {
            Type t = op->args[0].type().with_code(halide_type_int);
            Expr e = Call::make(op->type, op->name, {op->args[0], cast(t, op->args[1])}, op->call_type);
            e.accept(this);
        } else {
            CodeGen_C::visit(op);
        }
    } else if (op->is_intrinsic(Call::read_channel)) {
        std::string string_channel_index;
        std::string string_pipe_index;
        const StringImm *v = op->args[0].as<StringImm>();
        for (unsigned i = 1; i < op->args.size(); i++ ){
            Expr e = op->args[i];
            assert(e.type() == Int(32));
            std::string sindex = print_expr(e);
            assert(!sindex.empty());
            string_channel_index += "["+sindex+"]";
            string_pipe_index += sindex + ",";
        }
        if( string_pipe_index[string_pipe_index.size()-1] == ','){ string_pipe_index.pop_back(); }
        id = '_' + unique_name('_');
        int size = (v->value).rfind(".");
        std::string channel_name = v->value;
        debug(4) << "channel name: " << channel_name << "\n";
        if (size < (int)(v->value.size())-1) {
            std::string suffix = (v->value).substr(size + 1, (int)v->value.size() - size - 1);
            // eliminate useless suffix
            while (suffix != "channel") {
                channel_name = (channel_name).substr(0, size);
                size = (channel_name).rfind(".");
                if (size < (int)(channel_name.size())-1) {
                    suffix = (channel_name).substr(size + 1, (int)channel_name.size() - size - 1);
                } else {
                    break;
                }
            }
            if (suffix != "channel") {
                channel_name = v->value;
            }
        }
        debug(4) << "modified channel name: " << channel_name << "\n";
        string type = print_type(op->type);
        if (op->type.is_handle() && !op->type.is_generated_struct()) {
            type = print_name(channel_name + ".array.t");
        }
        string read_call = "read_channel_intel(" + print_name(channel_name) + string_channel_index + ")";
        stream << get_indent() << "// " << type << " " << id << " = " << read_call << ";\n";

        string read_pipe_call = "";
        if( string_pipe_index == ""){
            read_pipe_call = print_name(channel_name) + "::read()";
        } else {
            read_pipe_call = print_name(channel_name) + "::PipeAt<" +  string_pipe_index + ">::read()";
        }
        stream << get_indent() << type << " " << id << " = " << read_pipe_call << ";\n";
    } else if (op->is_intrinsic(Call::read_channel_nb)) {
        std::string string_channel_index;
        const StringImm *v = op->args[0].as<StringImm>();
        const StringImm *read_success = op->args[1].as<StringImm>();
        for (unsigned i = 2; i < op->args.size(); i++ ){
            Expr e = op->args[i];
            assert(e.type() == Int(32));
            std::string sindex = print_expr(e);
            assert(!sindex.empty());
            string_channel_index += "["+sindex+"]";
        }
        id = '_' + unique_name('_');
        int size = (v->value).rfind(".");
        std::string channel_name = v->value;
        debug(4) << "channel name: " << channel_name << "\n";
        if (size < (int)(v->value.size())-1) {
            std::string suffix = (v->value).substr(size + 1, (int)v->value.size() - size - 1);
            // eliminate useless suffix
            while (suffix != "channel") {
                channel_name = (channel_name).substr(0, size);
                size = (channel_name).rfind(".");
                if (size < (int)(channel_name.size())-1) {
                    suffix = (channel_name).substr(size + 1, (int)channel_name.size() - size - 1);
                } else {
                    break;
                }
            }
            if (suffix != "channel") {
                channel_name = v->value;
            }
        }
        debug(4) << "modified channel name: " << channel_name << "\n";
        // if ((int)suffix.size() > 0 && suffix[0] >= '0' && suffix[0] <= '9') {
        //     channel_name = (v->value).substr(0, size);
        // }
        stream << get_indent() << print_type(op->type) << " " << id << " = read_channel_nb_intel("
                << print_name(channel_name) << string_channel_index << ", &" << print_name(read_success->value) << ");\n";
    } else if (op->is_intrinsic(Call::write_channel)) {
        const StringImm *v = op->args[0].as<StringImm>();
        // Do not directly print to stream: there might have been a cached value useable.
        ostringstream rhs;
        ostringstream rhsPipe;
        std::string pipeIndex = "";
        int size = (v->value).rfind(".");
        std::string channel_name = v->value;
        debug(4) << "channel name: " << channel_name << "\n";
        if (size < (int)(v->value.size())-1) {
            std::string suffix = (v->value).substr(size + 1, (int)v->value.size() - size - 1);
            // eliminate useless suffix
            while (suffix != "channel") {
                channel_name = (channel_name).substr(0, size);
                size = (channel_name).rfind(".");
                if (size < (int)(channel_name.size())-1) {
                    suffix = (channel_name).substr(size + 1, (int)channel_name.size() - size - 1);
                } else {
                    break;
                }
            }
            if (suffix != "channel") {
                channel_name = v->value;
            }
        }
        debug(4) << "modified channel name: " << channel_name << "\n";
        rhs << print_name(channel_name);
        for (unsigned i = 2; i < op->args.size(); i++ ){
            Expr e = op->args[i];
            assert(e.type() == Int(32));
            rhs << "[";
            auto print_e = print_expr(e);
            rhs << print_e;
            rhs << "]";
            pipeIndex += print_e + ",";
        }
        if( pipeIndex[pipeIndex.size()-1] == ','){ pipeIndex.pop_back(); }
        rhs << ", ";
        std::string write_data = print_expr(op->args[1]);
        rhs << write_data;

        if(pipeIndex == ""){
            // Using regular pipes
            rhsPipe << print_name(channel_name) << "::write( " << write_data << " );\n";
        } else {
            // Using pipe array
            rhsPipe << print_name(channel_name) << "::PipeAt<" << pipeIndex << ">" << "::write( " << write_data << " );\n";
        }
        stream << get_indent() << "// write_channel_intel(" << rhs.str() << ");\n";
        stream << get_indent() << rhsPipe.str() ;
    } else if (op->is_intrinsic(Call::write_channel_nb)) {
        const StringImm *v = op->args[0].as<StringImm>();
        const StringImm *write_success = op->args[2].as<StringImm>();
        // Do not directly print to stream: there might have been a cached value useable.
        ostringstream rhs;
        int size = (v->value).rfind(".");
        std::string channel_name = v->value;
        debug(4) << "channel name: " << channel_name << "\n";
        if (size < (int)(v->value.size())-1) {
            std::string suffix = (v->value).substr(size + 1, (int)v->value.size() - size - 1);
            // eliminate useless suffix
            while (suffix != "channel") {
                channel_name = (channel_name).substr(0, size);
                size = (channel_name).rfind(".");
                if (size < (int)(channel_name.size())-1) {
                    suffix = (channel_name).substr(size + 1, (int)channel_name.size() - size - 1);
                } else {
                    break;
                }
            }
            if (suffix != "channel") {
                channel_name = v->value;
            }
        }
        debug(4) << "modified channel name: " << channel_name << "\n";
        rhs << print_name(channel_name);
        for (unsigned i = 3; i < op->args.size(); i++ ){
            Expr e = op->args[i];
            assert(e.type() == Int(32));
            rhs << "[";
            rhs << print_expr(e);
            rhs << "]";
        }
        rhs << ", ";
        std::string write_data = print_expr(op->args[1]);
        rhs << write_data;
        stream << get_indent() << print_name(write_success->value) << " = write_channel_nb_intel(" << rhs.str() << ");\n";
    } else if (op->is_intrinsic(Call::read_array)) {
        std::string arr_name = op->args[0].as<StringImm>()->value;
        // read the entire array as a whole
        if (op->args.size() == 1) {
            id = print_name(arr_name);
        } else {
            std::string string_index = ".s";
            for (size_t i = 1; i < op->args.size(); i++)
                string_index += "[" + print_expr(op->args[i]) + "]";
            id = '_' + unique_name('_');
            stream << get_indent() << print_type(op->type) << " " << id
                   <<" = " << print_name(arr_name) << string_index << ";\n";
        }
    } else if (op->is_intrinsic(Call::write_array)) {
        std::string arr_name = op->args[0].as<StringImm>()->value;
        // write the entire array as a whole
        if (op->args.size() == 2) {
            std::string write_data = print_expr(op->args[1]);
            stream << get_indent() << print_name(arr_name) << " = " << write_data << ";\n";
        } else {
            std::string write_data = print_expr(op->args[1]);
            std::string string_index = ".s";
            for (size_t i = 2; i < op->args.size(); i++)
                string_index += "[" + print_expr(op->args[i]) + "]";
            stream << get_indent() << print_name(arr_name) << string_index << " = " << write_data << ";\n";
        }
    } else if (op->is_intrinsic(Call::read_shift_reg)) {
        std::string string_index;
        const StringImm *v = op->args[0].as<StringImm>();
        std::string reg_name = extract_first_token(v->value);
        // shift reg has regular bounds
        if (space_vars.find(reg_name) == space_vars.end()) {
            for (size_t i = 1; i < op->args.size(); i++) {
                Expr e = op->args[i];
                assert(e.type() == Int(32));
                std::string sindex = print_expr(e);
                assert(!sindex.empty());
                if (i == op->args.size() - 1 && (shift_regs_bounds.find(v->value) != shift_regs_bounds.end())) {
                    // This is the last arg, and every shift reg is a non-standard vector
                    internal_assert(shift_regs_bounds[v->value] == op->args.size() - 1 || // this last arg is indexing a vector
                                    shift_regs_bounds[v->value] == op->args.size() - 2);  // this last arg is indexing a vector element
                    if (shift_regs_bounds[v->value] == op->args.size() - 1) {
                        string_index += "["+sindex+"]";
                    } else {
                        string_index += ".s["+sindex+"]";
                    }
                } else {
                    string_index += "["+sindex+"]";
                }
            }
            id = '_' + unique_name('_');
            ostringstream rhs;

            int size = (v->value).rfind(".");
            std::string shreg_name = v->value;
            debug(4) << "shreg name: " << shreg_name << "\n";
            if (size < (int)(v->value.size())-1) {
                std::string suffix = (v->value).substr(size + 1, (int)v->value.size() - size - 1);
                // eliminate useless suffix
                while (suffix != "shreg") {
                    shreg_name = (shreg_name).substr(0, size);
                    size = (shreg_name).rfind(".");
                    if (size < (int)(shreg_name.size())-1) {
                        suffix = (shreg_name).substr(size + 1, (int)shreg_name.size() - size - 1);
                    } else {
                        break;
                    }
                }
                if (suffix != "shreg") {
                    shreg_name = v->value;
                }
            }
            debug(4) << "modified shreg name: " << shreg_name << "\n";

            rhs << print_name(shreg_name) << string_index;
            print_assignment(op->type, rhs.str());
        } else {
            auto vars = space_vars.at(reg_name);
            // print irregular space loop index
            std::string suffix_index = "";
            for (size_t i = vars.size(); i >= 1; i--) {
                Expr e = op->args[i];
                assert(e.type() == Int(32));
                std::string sindex = print_expr(e);
                assert(!sindex.empty());
                suffix_index += "_"+sindex;
            }
            // print regular time loop index
            for (size_t i = 1 + vars.size(); i < op->args.size(); i++) {
                Expr e = op->args[i];
                assert(e.type() == Int(32));
                std::string sindex = print_expr(e);
                assert(!sindex.empty());
                if (i == op->args.size() - 1 && (shift_regs_bounds.find(v->value) != shift_regs_bounds.end())) {
                    // This is the last arg, and every shift reg is a non-standard vector
                    internal_assert(shift_regs_bounds[v->value] == op->args.size() - 1 || // this last arg is indexing a vector
                                    shift_regs_bounds[v->value] == op->args.size() - 2);  // this last arg is indexing a vector element
                    if (shift_regs_bounds[v->value] == op->args.size() - 1) {
                        string_index += "["+sindex+"]";
                    } else {
                        string_index += ".s["+sindex+"]";
                    }
                } else {
                    string_index += "["+sindex+"]";
                }
            }
            ostringstream rhs;
            int size = (v->value).rfind(".");
            std::string shreg_name = v->value;
            debug(4) << "shreg name: " << shreg_name << "\n";
            if (size < (int)(v->value.size())-1) {
                std::string suffix = (v->value).substr(size + 1, (int)v->value.size() - size - 1);
                // eliminate useless suffix
                while (suffix != "shreg") {
                    shreg_name = (shreg_name).substr(0, size);
                    size = (shreg_name).rfind(".");
                    if (size < (int)(shreg_name.size())-1) {
                        suffix = (shreg_name).substr(size + 1, (int)shreg_name.size() - size - 1);
                    } else {
                        break;
                    }
                }
                if (suffix != "shreg") {
                    shreg_name = v->value;
                }
            }
            debug(4) << "modified shreg name: " << shreg_name << "\n";

            rhs << print_name(shreg_name) << suffix_index << string_index;
            print_assignment(op->type, rhs.str());
        }
    } else if (op->is_intrinsic(Call::write_shift_reg)) {
        const StringImm *v = op->args[0].as<StringImm>();
        std::string reg_name = extract_first_token(v->value);
        // shift reg has regular bounds
        if (space_vars.find(reg_name) == space_vars.end()) {
            ostringstream rhs;
            int size = (v->value).rfind(".");
            std::string shreg_name = v->value;
            debug(4) << "shreg name: " << shreg_name << "\n";
            if (size < (int)(v->value.size())-1) {
                std::string suffix = (v->value).substr(size + 1, (int)v->value.size() - size - 1);
                // eliminate useless suffix
                while (suffix != "shreg") {
                    shreg_name = (shreg_name).substr(0, size);
                    size = (shreg_name).rfind(".");
                    if (size < (int)(shreg_name.size())-1) {
                        suffix = (shreg_name).substr(size + 1, (int)shreg_name.size() - size - 1);
                    } else {
                        break;
                    }
                }
                if (suffix != "shreg") {
                    shreg_name = v->value;
                }
            }
            debug(4) << "modified shreg name: " << shreg_name << "\n";

            rhs << print_name(shreg_name);
            // write shift register should has at leat 2 arguments
            internal_assert((int)op->args.size() >= 2);
            for (size_t i = 1; i < op->args.size()-1; i++) {
                Expr e = op->args[i];
                assert(e.type() == Int(32));
                std::string sindex = print_expr(e);
                assert(!sindex.empty());
                if (i == op->args.size() - 2 && (shift_regs_bounds.find(v->value) != shift_regs_bounds.end())) {
                    // This is the last arg, and every shift reg is a non-standard vector
                    internal_assert(shift_regs_bounds[v->value] == op->args.size() - 2 || // this last arg is indexing a vector
                                    shift_regs_bounds[v->value] == op->args.size() - 3);  // this last arg is indexing a vector element
                    if (shift_regs_bounds[v->value] == op->args.size() - 2) {
                        rhs << "["+sindex+"]";
                    } else {
                        rhs << ".s["+sindex+"]";
                    }
                } else {
                    rhs << "["+sindex+"]";
                }
            }
            std::string write_data = print_expr(op->args[op->args.size()-1]);

            // After writing to a shift register, the original cached value is no longer valid.
            auto cached = cache.find(rhs.str());
            if (cached != cache.end()) {
                cache.erase(cached);
            }
            stream << get_indent() << rhs.str() << " = " << write_data << ";\n";
        } else {
            ostringstream rhs;
            auto vars = space_vars.at(reg_name);
            int size = (v->value).rfind(".");
            std::string shreg_name = v->value;
            debug(4) << "shreg name: " << shreg_name << "\n";
            if (size < (int)(v->value.size())-1) {
                std::string suffix = (v->value).substr(size + 1, (int)v->value.size() - size - 1);
                // eliminate useless suffix
                while (suffix != "shreg") {
                    shreg_name = (shreg_name).substr(0, size);
                    size = (shreg_name).rfind(".");
                    if (size < (int)(shreg_name.size())-1) {
                        suffix = (shreg_name).substr(size + 1, (int)shreg_name.size() - size - 1);
                    } else {
                        break;
                    }
                }
                if (suffix != "shreg") {
                    shreg_name = v->value;
                }
            }
            debug(4) << "modified shreg name: " << shreg_name << "\n";

            rhs << print_name(shreg_name);
            // write shift register should has at leat 2 arguments
            internal_assert((int)op->args.size() >= 2);
            for (size_t i = vars.size(); i >= 1; i--) {
                Expr e = op->args[i];
                assert(e.type() == Int(32));
                rhs << "_";
                rhs << print_expr(e);
            }
            for (size_t i = 1 + vars.size(); i < op->args.size()-1; i++) {
                Expr e = op->args[i];
                assert(e.type() == Int(32));
                std::string sindex = print_expr(e);
                assert(!sindex.empty());
                if (i == op->args.size() - 2 && (shift_regs_bounds.find(v->value) != shift_regs_bounds.end())) {
                    // This is the last arg, and every shift reg is a non-standard vector
                    internal_assert(shift_regs_bounds[v->value] == op->args.size() - 2 || // this last arg is indexing a vector
                                    shift_regs_bounds[v->value] == op->args.size() - 3);  // this last arg is indexing a vector element
                    if (shift_regs_bounds[v->value] == op->args.size() - 2) {
                        rhs << "["+sindex+"]";
                    } else {
                        rhs << ".s["+sindex+"]";
                    }
                } else {
                    rhs << "["+sindex+"]";
                }
            }
            std::string write_data = print_expr(op->args[op->args.size()-1]);
            stream << get_indent() << rhs.str() << " = " << write_data << ";\n";
        }
    } else if (ends_with(op->name, ".ibuffer")) {
        std::string name = op->name.substr(0, op->name.length()-std::string(".ibuffer").size());
        string buffer_name = name + '.' + std::to_string(op->value_index) + ".ibuffer";
        // Do not directly print to stream: there might have been a cached value useable.
        ostringstream rhs;
        rhs << print_name(buffer_name);
        for(size_t i = 0; i < op->args.size(); i++) {
            rhs << "[";
            rhs << print_expr(op->args[i]);
            rhs << "]";
        }
        print_assignment(op->type, rhs.str());
    } else if (ends_with(op->name, ".temp")) {
        std::string name = op->name;
        // Do not directly print to stream: there might have been a cached value useable.
        ostringstream rhs;
        rhs << print_name(name);
        for(size_t i = 0; i < op->args.size(); i++) {
            rhs << "[";
            rhs << print_expr(op->args[i]);
            rhs << "]";
        }
        print_assignment(op->type, rhs.str());
    } else if (op->is_intrinsic(Call::make_struct)) {
        vector<string> values;
        for (size_t i = 0; i < op->args.size(); i++) {
            values.push_back(print_expr(op->args[i]));
        }
        ostringstream rhs;
        rhs << "{";
        for (size_t i = 0; i < op->args.size(); i++) {
            rhs << get_indent() << values[i];
            if (i < op->args.size() - 1) rhs << ", ";
        }
        rhs << "}";
        print_assignment(op->type, rhs.str());
    } else if (op->is_intrinsic(Call::postincrement)) {
        // Args: an expression e, Expr(offset), Expr("addr.temp"), indexes of addr.temp
        // Print: id = expression e
        //        addr.temp[indexes] = addr.temp[indexes] + offset
        string offset = print_expr(op->args[1]);
        string addr_temp = print_name(op->args[2].as<StringImm>()->value);
        for (size_t i = 3; i < op->args.size(); i++) {
            if (op->args[i].as<Variable>() != NULL) {
                addr_temp += "[" + print_name(op->args[i].as<Variable>()->name) + "]";
            } else {
                internal_assert(op->args[i].as<IntImm>() != NULL);
                addr_temp += "[" + std::to_string(op->args[i].as<IntImm>()->value) + "]";
            }
        }
        stream << get_indent() << print_expr(op->args[0]) << ";\n";
        stream << get_indent() << addr_temp << " = " << addr_temp << " + " << offset << ";\n";
    } else if (op->is_intrinsic(Call::fpga_reg)) {
        ostringstream rhs;
        if( op->type.is_vector() ){
            // Needed for complex types in sycl i.e. floatN types
            std::string print_e = print_expr(op->args[0]);
            auto num_lanes = op->type.lanes();
            rhs << "(" << print_type(op->type) << "){\n";
            for(int i = 0; i <num_lanes ; i++ ){
                rhs << get_indent() << "  sycl::ext::intel::fpga_reg( sycl::ext::intel::fpga_reg(" << print_e << "["<< i << "]))";
                if( i != num_lanes-1){ rhs << ","; }
                rhs << "\n";
            }
            rhs << get_indent() << "}";
        } else {
            // Normal fpga register casting
            rhs << "sycl::ext::intel::fpga_reg( sycl::ext::intel::fpga_reg(" << print_expr(op->args[0]) << "))";
        }
        print_assignment(op->type, rhs.str());
    } else if (op->is_intrinsic(Call::overlay_switch)) {
        internal_assert(op->args.size() > 0);

        // Prepare task object to be dispatched
        internal_assert(op->args[0].as<StringImm>());
        std::string type = op->args[0].as<StringImm>()->value;

        if (type == "before_switch") {
            ostringstream rhs;
            rhs << "0";
            print_assignment(op->type, rhs.str());

            stream << get_indent() << "index_t index = {0, {";
            for (unsigned int k = 1; k < op->args.size(); k++) {
                stream << print_expr(op->args[k]);
                if (k != op->args.size() - 1) stream << ", ";
            }
            stream << "}};\n";

            stream << get_indent() << "arg_t inputs;\n";
            stream << get_indent() << "task_t task;\n";
            stream << get_indent() << "task.index = index;\n";
            stream << get_indent() << "task.done = false;\n";
            stream << get_indent() << "task.last = false;\n\n";

        } else if (type == "kernel_begin") {
            ostringstream rhs;
            rhs << "0";
            print_assignment(op->type, rhs.str());

            internal_assert(op->args.size() > 1);
            internal_assert(op->args[1].as<IntImm>());
            int queue_index = op->args[1].as<IntImm>()->value;
            stream << get_indent() << "arg_t inputs = read_channel_intel(q["
                << queue_index << "]);\n";
            stream << get_indent() << "mem_fence(CLK_CHANNEL_MEM_FENCE);\n\n";

        } else if (type == "kernel_end") {
            ostringstream rhs;
            rhs << "0";
            print_assignment(op->type, rhs.str());

            internal_assert(op->args.size() > 1);
            internal_assert(op->args[1].as<IntImm>());
            int queue_index = op->args[1].as<IntImm>()->value;
            // stream << get_indent()
            //     << "mem_fence(CLK_CHANNEL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);\n";
            // stream << get_indent() << "index_t f" << queue_index
            //     << " = read_channel_intel(q" << queue_index << "_end);\n";
            stream << get_indent() << "mem_fence(CLK_CHANNEL_MEM_FENCE);\n";
            stream << get_indent() << "write_channel_intel(q_ret[" << queue_index << "], inputs.finish);\n";

        // Print data from task channel for autorun kernels
        } else if (type == "data") {
            ostringstream rhs;
            internal_assert(op->args[1].as<StringImm>());
            rhs << op->args[1].as<StringImm>()->value;
            print_assignment(op->type, rhs.str());

        } else if (type == "end_signal") {
            ostringstream rhs;
            rhs << "0";
            print_assignment(op->type, rhs.str());
            stream << get_indent() << "push_end = true;\n";

        } else if (type == "before_main_body") {
            ostringstream rhs;
            rhs << "0";
            print_assignment(op->type, rhs.str());
            stream << get_indent() << "bool push_end = false;\n";

        } else if (type == "after_main_body") {
            ostringstream rhs;
            rhs << "0";
            print_assignment(op->type, rhs.str());
            stream << R"(
 if (push_end) {
    task_t end;
    end.last = true;
    write_channel_intel(qt, end);
    mem_fence(CLK_CHANNEL_MEM_FENCE);

    int count = read_channel_intel(qf);
    mem_fence(CLK_CHANNEL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);
 }
)";
        }

    // Print task switching loop body
    } else if (op->is_intrinsic(Call::overlay)) {

        char *overlay_num = getenv("HL_OVERLAY_NUM");
        internal_assert(overlay_num != NULL);

        internal_assert(op->args.size() > 0);
        ostringstream rhs;
        rhs << "0";
        print_assignment(op->type, rhs.str());

        struct status {
            vector<Expr> iter_vars;
            vector<Expr> conditions;
        };

        struct input {
            string  name;
            string  value;
        };

        int dep_queue_index = -1;
        std::map<int, status> info;
        std::vector<input> inputs;

        auto task_id = op->args[0].as<IntImm>()->value;
        auto queueNo = op->args[1].as<IntImm>()->value;

        for (int k = 2; k < (signed)op->args.size(); k++) {
            auto e = op->args[k];
            if (auto name = e.as<StringImm>()) {
                string arg_name = name->value;

                // args as ( dep_0, parent queue index, itervar, cond...)
                // Example:
                // overlay("task_0", 2, (Task2.s0.i-1), (1<+Task2.s0.i), "buffer::b1")
                //
                if (starts_with(arg_name, "inputs.args")) {
                    string delimiter = "::";
                    size_t pos = arg_name.find(delimiter);
                    string id = arg_name.substr(0, pos);

                    string value = arg_name.substr(pos + delimiter.length()) + " + " + print_expr(op->args[k+1]);
                    input entry = {id, value};
                    k++;
                    inputs.push_back(entry);
                } else if (starts_with(arg_name, "inputs.constants")) {
                    input entry = {arg_name, print_expr(op->args[k+1])};
                    k++;
                    inputs.push_back(entry);
                } else {
                    internal_assert(false) << "Unrecognized arg input "
                        << arg_name << " in the enqueue() function given...";
                }

            // Depending queue index
            } else if (auto queue = e.as<IntImm>()) {
                dep_queue_index = queue->value;
                info[dep_queue_index] = {{}, {}};

            // Depending task iter var index
            } else if (e.as<Add>() || e.as<Variable>() || e.as<Sub>()) {
                info[dep_queue_index].iter_vars.push_back(e);

            // Depending task conditions
            } else if (e.as<GT>() || e.as<LT>() || e.as<NE>() || e.as<GE>() || e.as<LE>()) {
                info[dep_queue_index].conditions.push_back(e);
            }
        }

        // Print task dispatching logic
        int dep_num = 0;
        stream << get_indent() << "// Sending task" << task_id << " to the scheduler...\n";
        stream << get_indent() << "task.queue = " << queueNo << ";\n";
        stream << get_indent() << "task.index.task_id = " << task_id << ";\n\n";

        int cond_num = 0;
        ostringstream temp;
        for (auto& kv : info) {
            string cond;
            // Print depending task iter var index
            if (kv.second.conditions.size() > 0) {
                cond_num += 1;
                // TO FIX: incomplete conds
                for (unsigned int o = 0; o < kv.second.conditions.size(); o++) {
                    cond += print_expr(kv.second.conditions[o]);
                    if (o != kv.second.conditions.size() - 1) {
                        cond += " && ";
                    }
                }

                temp << get_indent()  << "index_t dep_" << task_id << "_" << dep_num
                     << " = {" << kv.first << ", {";

                for (unsigned int o = 0; o < kv.second.iter_vars.size(); o++) {
                    temp << cond << " ? "
                        << print_expr(kv.second.iter_vars[o])
                        << " : -1";
                    if (o != kv.second.iter_vars.size() - 1) temp << ", ";
                }
            } else {
                temp << get_indent() << "index_t dep_" << task_id << "_" << dep_num
                     << " = {" << kv.first << ", {";
                for (unsigned int o = 0; o < kv.second.iter_vars.size(); o++) {
                    auto var = print_expr(kv.second.iter_vars[o]);
                    temp << var;
                    if (o != kv.second.iter_vars.size() - 1) temp << ", ";
                }
            }

            temp << "}};\n";
            temp << get_indent() << "task.deps[" << dep_num << "]"
                   << " = dep_" << task_id << "_" << dep_num << ";\n";
            dep_num ++;
        }

        stream << temp.str();
        stream << get_indent() << "task.num_of_deps = " << dep_num << ";\n\n";
        // if (cond_num == 0) {
        //   stream << get_indent() << "task.num_of_deps = "
        //          << dep_num << ";\n\n";
        // } else {
        //   stream << get_indent() << "task.num_of_deps = ("
        //          << cond << " ? " << dep_num << " : "
        //          << dep_num - cond_num << ");\n\n";
        // }

        // Prepare input information
        stream << get_indent() << "inputs.finish = task.index;\n";
        for (auto& input : inputs) {
            string name = input.name;
            string prefix = (starts_with(name, "inputs.args")) ? "_" : "";
            stream << get_indent() << input.name << " = "
                << prefix << input.value << ";\n";
        }
        stream << get_indent() << "task.inputs = inputs;\n";
        stream << get_indent() << "write_channel_intel(qt, task);\n";
        stream << get_indent() << "mem_fence(CLK_CHANNEL_MEM_FENCE);\n\n";
    } else {
        // Other intrinsics
        CodeGen_C::visit(op);
    }
}

string CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::print_extern_call(const Call *op) {
    internal_assert(!function_takes_user_context(op->name));
    vector<string> args(op->args.size());
    for (size_t i = 0; i < op->args.size(); i++) {
        args[i] = print_expr(op->args[i]);
    }
    ostringstream rhs;
    rhs << op->name << "(" << with_commas(args) << ")";
    return rhs.str();
}

string CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::print_load_assignment(Type t, const std::string &rhs){
    auto cached = cache.find(rhs);
    if (cached == cache.end()) {
        id = unique_name('_');
        stream << get_indent() << print_type(t, AppendSpace) << id << ";\n"
               << get_indent() << id << rhs << ";\n";
        cache[rhs] = id;
    } else {
        id = cached->second;
    }
    return id;
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::visit(const Load *op) {
    user_assert(is_one(op->predicate)) << "Predicated load is not supported inside OpenCL kernel.\n";

    // If we're loading a contiguous ramp into a vector, use vload instead.
    Expr ramp_base = strided_ramp_base(op->index);
    if (ramp_base.defined()) {
        internal_assert(op->type.is_vector());
        string id_ramp_base = print_expr(ramp_base);

        ostringstream rhs;

        if (!is_standard_opencl_type(op->type)) {
            // For compiler-generated vector types, read in the way like *((float6*) address)
            rhs << "*((" << get_memory_space(op->name) << " "
                   << print_type(op->type) << "*)(" << print_name(op->name)
                   << " + " << id_ramp_base << "))";
            print_assignment(op->type, rhs.str());
        } else {
            // use simple method to implemement vector type instead of vload
            rhs << " = { \n";
            indent += 2;
            for(int i = 0; i < op->type.lanes(); i++){
                rhs << get_indent() << print_name(op->name) << "[" << id_ramp_base << "+" << i << "]";
                if(i != op->type.lanes() -1){
                    rhs << ",\n";
                } else {
                    rhs << "\n";
                }
            }
            indent -= 2;
            rhs << get_indent()  << "}";
            print_load_assignment(op->type, rhs.str());
        }
        // print_assignment(op->type, rhs.str());
        return;
    }

    string id_index = print_expr(op->index);

    // Get the rhs just for the cache.
    bool type_cast_needed = !(allocations.contains(op->name) &&
                              allocations.get(op->name).type == op->type);
    ostringstream rhs;
    if (pointer_args.count(op->name)) {
        rhs << pointer_args[op->name];
    } else if (type_cast_needed) {
        rhs << "((" << get_memory_space(op->name) << " "
            << print_type(op->type) << " *)"
            << print_name(op->name)
            << ")";
    } else {
        rhs << print_name(op->name);
    }
    rhs << "[" << id_index << "]";

    std::map<string, string>::iterator cached = cache.find(rhs.str());
    if (cached != cache.end()) {
        id = cached->second;
        return;
    }

    if (op->index.type().is_vector()) {
        // If index is a vector, gather vector elements.
        internal_assert(op->type.is_vector());

        id = "_" + unique_name('V');
        cache[rhs.str()] = id;

        stream << get_indent() << print_type(op->type)
               << " " << id << ";\n";

        for (int i = 0; i < op->type.lanes(); ++i) {
            stream << get_indent();
            stream
                // << id << ".s" << vector_elements[i] // OneAPI does not use the .s syntax
                << id << "[" << i << "]"
                << " = ((" << get_memory_space(op->name) << " "
                << print_type(op->type.element_of()) << "*)"
                << print_name(op->name) << ")"
                // << "[" << id_index << ".s" << vector_elements[i] << "];\n"; // OneAPI does not use the .s syntax
                << "[" << id_index << "[" << i << "]];\n";
        }
    } else {
        print_assignment(op->type, rhs.str());
    }
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::visit(const Store *op) {
    user_assert(is_one(op->predicate)) << "Predicated store is not supported inside OpenCL kernel.\n";

    if (emit_atomic_stores) {
        // Currently only support scalar atomics.
        user_assert(op->value.type().is_scalar()) << "OpenCL atomic store does not support vectorization.\n";
        user_assert(op->value.type().bits() >= 32) << "OpenCL only support 32 and 64 bit atomics.\n";
        if (op->value.type().bits() == 64) {
            user_assert(target.has_feature(Target::CLAtomics64))
                << "Enable feature CLAtomics64 for 64-bit atomics in OpenCL.\n";
        }
        // Detect whether we can describe this as an atomic-read-modify-write,
        // otherwise fallback to a compare-and-swap loop.
        // Current only test for atomic add.
        Expr val_expr = op->value;
        Type t = val_expr.type();
        Expr equiv_load = Load::make(t, op->name, op->index, Buffer<>(), op->param, op->predicate, op->alignment);
        Expr delta = simplify(common_subexpression_elimination(op->value - equiv_load));
        // For atomicAdd, we check if op->value - store[index] is independent of store.
        // The atomicAdd operations in OpenCL only supports integers so we also check that.
        bool is_atomic_add = t.is_int_or_uint() && !expr_uses_var(delta, op->name);
        bool type_cast_needed = !(allocations.contains(op->name) &&
                                  allocations.get(op->name).type == t);
        auto print_store_var = [&]() {
            if (type_cast_needed) {
                stream << "(("
                       << get_memory_space(op->name) << " "
                       << print_type(t)
                       << " *)"
                       << print_name(op->name)
                       << ")";
            } else {
                stream << print_name(op->name);
            }
        };
        if (is_atomic_add) {
            string id_index = print_expr(op->index);
            string id_delta = print_expr(delta);
            stream << get_indent();
            // atomic_add(&x[i], delta);
            if (t.bits() == 32) {
                stream << "atomic_add(&";
            } else {
                stream << "atom_add(&";
            }

            print_store_var();
            stream << "[" << id_index << "]";
            stream << "," << id_delta << ");\n";
        } else {
            // CmpXchg loop
            // {
            //   union {unsigned int i; float f;} old_val;
            //   union {unsigned int i; float f;} new_val;
            //   do {
            //     old_val.f = x[id_index];
            //     new_val.f = ...
            //   } while(atomic_cmpxchg((volatile address_space unsigned int*)&x[id_index], old_val.i, new_val.i) != old_val.i);
            // }
            stream << get_indent() << "{\n";
            indent += 2;
            string id_index = print_expr(op->index);
            std::string int_type = t.bits() == 32 ? "int" : "long";
            if (t.is_float() || t.is_uint()) {
                int_type = "unsigned " + int_type;
            }
            if (t.is_float()) {
                stream << get_indent() << "union {" << int_type << " i; " << print_type(t) << " f;} old_val;\n";
                stream << get_indent() << "union {" << int_type << " i; " << print_type(t) << " f;} new_val;\n";
            } else {
                stream << get_indent() << int_type << " old_val;\n";
                stream << get_indent() << int_type << " new_val;\n";
            }
            stream << get_indent() << "do {\n";
            indent += 2;
            stream << get_indent();
            if (t.is_float()) {
                stream << "old_val.f = ";
            } else {
                stream << "old_val = ";
            }
            print_store_var();
            stream << "[" << id_index << "];\n";
            string id_value = print_expr(op->value);
            stream << get_indent();
            if (t.is_float()) {
                stream << "new_val.f = ";
            } else {
                stream << "new_val = ";
            }
            stream << id_value << ";\n";
            indent -= 2;
            std::string old_val = t.is_float() ? "old_val.i" : "old_val";
            std::string new_val = t.is_float() ? "new_val.i" : "new_val";
            stream << get_indent()
                   << "} while(atomic_cmpxchg((volatile "
                   << get_memory_space(op->name) << " " << int_type << "*)&"
                   << print_name(op->name) << "[" << id_index << "], "
                   << old_val << ", " << new_val << ") != " << old_val << ");\n"
                   << get_indent() << "}\n";
            indent -= 2;
        }
        cache.clear();
        return;
    }

    string id_value = print_expr(op->value);
    Type t = op->value.type();

    // If we're writing a contiguous ramp, use vstore instead.
    Expr ramp_base = strided_ramp_base(op->index);
    if (ramp_base.defined()) {
        internal_assert(op->value.type().is_vector());
        string id_ramp_base = print_expr(ramp_base);

        if (!is_standard_opencl_type(t)) {
            // For compiler-generated vector types, write in the way like
            //   *((float6*) address) = value
            stream << get_indent() << "*((" << get_memory_space(op->name) << " "
                   << print_type(t) << "*)(" << print_name(op->name)
                   << " + " << id_ramp_base << ")) = " << id_value << ";\n";
        } else {
            // using simple menthod to store data instead of vstore
            for(int i = 0; i < t.lanes(); i++){
                stream << get_indent() << print_name(op->name)
                       << "[" << id_ramp_base << "+" << i << "] = "
                       <<  id_value << "["<< i << "]"<< ";\n";
            }
        }
    } else if (op->index.type().is_vector()) {
        // If index is a vector, scatter vector elements.
        internal_assert(t.is_vector());

        string id_index = print_expr(op->index);

        for (int i = 0; i < t.lanes(); ++i) {
            stream << get_indent() << "((" << get_memory_space(op->name) << " "
                   << print_type(t.element_of()) << " *)"
                   << print_name(op->name)
                   << ")["
                //    << id_index << ".s" << vector_elements[i] << "] = " // OneAPI does not use the .s syntax
                //    << id_value << ".s" << vector_elements[i] << ";\n"; // OneAPI does not use the .s syntax
                   << id_index << "[" << i << "]] = "
                   << id_value << "[" << i << "];\n";
        }
    } else {
        bool type_cast_needed = !(allocations.contains(op->name) &&
                                  allocations.get(op->name).type == t);

        string id_index = print_expr(op->index);
        stream << get_indent();

        // Avoid type casting for global pointer
        if (pointer_args.count(op->name)) {
            stream << pointer_args[op->name];
        } else if (type_cast_needed) {
            stream << "(("
                   << get_memory_space(op->name) << " "
                   << print_type(t)
                   << " *)"
                   << print_name(op->name)
                   << ")";
        } else {
            stream << print_name(op->name);
        }
        stream << "[" << id_index << "] = "
               << id_value << ";\n";
    }

    cache.clear();
}







// In the following logic operations, we specially handle the non-standard bool vector
// like this:
//      int16  x = y == z;                   // y and z are int16
//      bool16 b = {x.s0, x.s1, ..., x.s15}; // convert from int16 to bool16
void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::cast_to_bool_vector(Type bool_type, Type other_type, string other_var) {
    if (bool_type.is_vector() && bool_type.bits() == 1 && other_type.bits() != 1) {
        internal_assert(other_type.is_vector() && other_type.lanes() == bool_type.lanes());
        std::ostringstream oss;
        for (int i = 0; i < bool_type.lanes(); i++) {
            oss << (i == 0 ? "{" : ", ") << "(bool)" << other_var << ".s" << vector_index_to_string(i);
        }
        oss << "};\n";
        print_assignment(bool_type, oss.str());
    }
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::visit_binop(Type t, Expr a, Expr b, const char *op) {
    string sa = print_expr(a);
    string sb = print_expr(b);
    if (is_standard_opencl_type(t) && is_standard_opencl_type(a.type())) {
        print_assignment(t, sa + " " + op + " " + sb);
    } else {
        // Output something like bool16 x = {a.s0 op b.s0, a.s1 op b.s0, ...}
        internal_assert(t.is_vector() && a.type().is_vector() && t.lanes() == a.type().lanes());
        std::ostringstream oss;
        for (int i = 0; i < t.lanes(); i++) {
            oss << ((i == 0) ? "{" : ", ") << sa << ".s" << vector_index_to_string(i) << op
                << sb << ".s" << vector_index_to_string(i);
        }
        oss << "};\n";
        print_assignment(t, oss.str());
    }
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::visit(const EQ *op) {
    visit_binop(eliminated_bool_type(op->type, op->a.type()), op->a, op->b, "==");
    cast_to_bool_vector(op->type, op->a.type(), id);
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::visit(const NE *op) {
    visit_binop(eliminated_bool_type(op->type, op->a.type()), op->a, op->b, "!=");
    cast_to_bool_vector(op->type, op->a.type(), id);
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::visit(const LT *op) {
    visit_binop(eliminated_bool_type(op->type, op->a.type()), op->a, op->b, "<");
    cast_to_bool_vector(op->type, op->a.type(), id);
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::visit(const LE *op) {
    visit_binop(eliminated_bool_type(op->type, op->a.type()), op->a, op->b, "<=");
    cast_to_bool_vector(op->type, op->a.type(), id);
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::visit(const GT *op) {
    visit_binop(eliminated_bool_type(op->type, op->a.type()), op->a, op->b, ">");
    cast_to_bool_vector(op->type, op->a.type(), id);
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::visit(const GE *op) {
    visit_binop(eliminated_bool_type(op->type, op->a.type()), op->a, op->b, ">=");
    cast_to_bool_vector(op->type, op->a.type(), id);
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::visit(const Cast *op) {
    if (!target.has_feature(Target::CLHalf) &&
        ((op->type.is_float() && op->type.bits() < 32) ||
         (op->value.type().is_float() && op->value.type().bits() < 32))) {
        Expr equiv = lower_float16_cast(op);
        equiv.accept(this);
        return;
    }

    if (op->type.is_vector()) {
        print_assignment(op->type, "convert_" + print_type(op->type) + "(" + print_expr(op->value) + ")");
    } else {
        CodeGen_C::visit(op);
    }
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::visit(const Select *op) {
    // The branch(es) might contain actions with side effects like a channel read.
    // Thus we must guard the branch(es).
    // So first convert to if_then_else.
    user_assert(op->condition.type().is_scalar())
        << "The OpenCL does not support branch divergence. "
        << "Please do not perform vectorization if the value of a URE depends on the incoming data.\n";
    Expr c = Call::make(op->type, Call::if_then_else, {op->condition, op->true_value, op->false_value}, Call::PureIntrinsic);
    c.accept(this);
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::visit(const IfThenElse *op) {
    string cond_id = print_expr(op->condition);
    stream << get_indent() << "if (" << cond_id << ")\n";
    open_scope();
    op->then_case.accept(this);
    close_scope("if " + cond_id);

    if (op->else_case.defined()) {
        stream << get_indent() << "else\n";
        open_scope();
        op->else_case.accept(this);
        close_scope("if " + cond_id + " else");
    }
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::visit(const Allocate *op) {
    user_assert(!op->new_expr.defined()) << "Allocate node inside OpenCL kernel has custom new expression.\n"
                                         << "(Memoization is not supported inside GPU kernels at present.)\n";

    if (op->name == "__shared") {
        // Already handled
        op->body.accept(this);
    } else {
        // Check if the allocation is for global mutable pointer
        if (op->memory_type == MemoryType::CLPtr) {
            user_assert(op->extents.size() == 1);
            user_assert(op->extents[0].as<StringImm>());
            auto arg = op->extents[0].as<StringImm>()->value;
            pointer_args[op->name] = arg;
            debug(2) << "Found pointer arg " << op->name << "... \n";

            op->body.accept(this);
            return;
        }

        open_scope();

        debug(2) << "Allocate " << op->name << " on device\n";

        debug(3) << "Pushing allocation called " << op->name << " onto the symbol table\n";

        // Allocation is not a shared memory allocation, just make a local declaration.
        // It must have a constant size.
        int32_t size = op->constant_allocation_size();
        user_assert(size > 0)
            << "Allocation " << op->name << " has a dynamic size. "
            << "Only fixed-size allocations are supported on the gpu. "
            << "Try storing into shared memory instead.";

        stream << get_indent() << print_type(op->type) << ' '
               << print_name(op->name) << "[" << size << "];\n";
        stream << get_indent() << "#define " << get_memory_space(op->name) << " __private\n";

        Allocation alloc;
        alloc.type = op->type;
        allocations.push(op->name, alloc);

        op->body.accept(this);

        // Should have been freed internally
        internal_assert(!allocations.contains(op->name));

        close_scope("alloc " + print_name(op->name));
    }
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::visit(const Free *op) {
    if (op->name == "__shared") {
        return;
    } else {
        // Should have been freed internally
        internal_assert(allocations.contains(op->name));
        allocations.pop(op->name);
        stream << get_indent() << "#undef " << get_memory_space(op->name) << "\n";
    }
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::visit(const AssertStmt *op) {
    user_warning << "Ignoring assertion inside OpenCL kernel: " << op->condition << "\n";
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::visit(const Shuffle *op) {
    int op_lanes = op->type.lanes();
    internal_assert(!op->vectors.empty());
    int arg_lanes = op->vectors[0].type().lanes();
    // TODO: [SIZE] the following logic for interleave is unnecessary, I think
    if (op->is_interleave()) {
        if (op->vectors.size() == 1) {
            // 1 argument, just do a simple assignment
            internal_assert(op_lanes == arg_lanes);
            print_assignment(op->type, print_expr(op->vectors[0]));
        } else if (op->vectors.size() == 2) {
            // 2 arguments, set the .even to the first arg and the
            // .odd to the second arg
            internal_assert(op->vectors[1].type().lanes() == arg_lanes);
            internal_assert(op_lanes / 2 == arg_lanes);
            string a1 = print_expr(op->vectors[0]);
            string a2 = print_expr(op->vectors[1]);
            id = unique_name('_');
            stream << get_indent() << print_type(op->type) << " " << id << ";\n";
            stream << get_indent() << id << ".even = " << a1 << ";\n";
            stream << get_indent() << id << ".odd = " << a2 << ";\n";
        } else {
            // 3+ arguments, interleave via a vector literal
            // selecting the appropriate elements of the vectors
            int dest_lanes = op->type.lanes();
            internal_assert(dest_lanes <= 16);
            int num_vectors = op->vectors.size();
            vector<string> arg_exprs(num_vectors);
            for (int i = 0; i < num_vectors; i++) {
                internal_assert(op->vectors[i].type().lanes() == arg_lanes);
                arg_exprs[i] = print_expr(op->vectors[i]);
            }
            internal_assert(num_vectors * arg_lanes >= dest_lanes);
            id = unique_name('_');
            stream << get_indent() << print_type(op->type) << " " << id;
            stream << " = (" << print_type(op->type) << ")(";
            for (int i = 0; i < dest_lanes; i++) {
                int arg = i % num_vectors;
                int arg_idx = i / num_vectors;
                internal_assert(arg_idx <= arg_lanes);
                // stream << arg_exprs[arg] << ".s" << vector_elements[arg_idx];  // OneAPI does not use the .s syntax
                stream << arg_exprs[arg] << "[" << arg_idx << "]";
                if (i != dest_lanes - 1) {
                    stream << ", ";
                }
            }
            stream << ");\n";
        }
    } else {
        // implement shuffle for other cases
        internal_assert(op->indices.size() == 1 || (int)op->indices.size() == op_lanes)
            << "Shuffle indices length not match expected lane size\n";
        internal_assert(op_lanes == 1 || op_lanes == 2 || op_lanes == 3 || op_lanes == 4
            || op_lanes == 8 || op_lanes == 16 ||
            // Otherwise, this is to merge several inputs into a non-standard vector type
            (op->is_concat() && !is_standard_opencl_type(op->type)))
            << "Unsupported vector length, only support"
            << " length in [1, 2, 3, 4, 8, 16] in OpenCL\n";
        int num_vectors = op->vectors.size();
        int total_lanes = 0;
        std::vector<std::pair<int, int>> index_table;
        std::vector<bool> is_vec;
        vector<string> arg_exprs(num_vectors);
        for (int i = 0; i < num_vectors; ++i) {
            arg_exprs[i] = print_expr(op->vectors[i]);
            int arg_lanes = op->vectors[i].type().lanes();
            internal_assert(arg_lanes == 1 || arg_lanes == 2 || arg_lanes == 3 || arg_lanes == 4
            || arg_lanes == 8 || arg_lanes == 16) << "Unsupported vector length, only support"
                                                << " length in [1, 2, 3, 4, 8, 16] in OpenCL\n";
            total_lanes += arg_lanes;
            is_vec.push_back(arg_lanes > 1);
            for (int j = 0; j < arg_lanes; ++j) {
                index_table.push_back(std::make_pair(i, j));
            }
        }
        internal_assert(total_lanes >= op_lanes) << "No enough source lanes for shuffle\n";
        // name for result
        id = unique_name('_');
        stream << get_indent() << print_type(op->type) << " " << id;
        stream << " = (" << print_type(op->type) << ")";
        if (!is_standard_opencl_type(op->type)) {
            stream << "{";
        } else {
            stream << "{";
        }

        for (int i = 0; i < op_lanes; ++i) {
            // which vector
            int p_arg = index_table[op->indices[i]].first;
            // which lane
            int p_arg_lane = index_table[op->indices[i]].second;
            if (is_vec[p_arg]) {
                // stream << arg_exprs[p_arg] << ".s" << vector_elements[p_arg_lane]; // OneAPI does not use the .s syntax
                stream << arg_exprs[p_arg] << "[" << p_arg_lane << "]";
            } else {
                stream << arg_exprs[p_arg];
            }
            if (i != op_lanes - 1) {
                stream << ", ";
            }
        }
        if (!is_standard_opencl_type(op->type)) {
            stream << "};\n";
        } else {
            stream << "}; // Converted using c++ standard (){} instead of ()()\n";
        }

        // internal_error << "Shuffle not implemented.\n";
    }
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::visit(const Max *op) {
    print_expr(Call::make(op->type, "max", {op->a, op->b}, Call::Extern));
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::visit(const Min *op) {
    print_expr(Call::make(op->type, "min", {op->a, op->b}, Call::Extern));
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::visit(const Atomic *op) {
    // Most GPUs require all the threads in a warp to perform the same operations,
    // which means our mutex will lead to deadlock.
    user_assert(op->mutex_name.empty())
        << "The atomic update requires a mutex lock, which is not supported in OpenCL.\n";

    // Issue atomic stores.
    ScopedValue<bool> old_emit_atomic_stores(emit_atomic_stores, true);
    IRVisitor::visit(op);
}









void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::add_kernel(Stmt s,
                                                      const string &name,
                                                      const vector<DeviceArgument> &args) {

    // OneAPI combines host and device code.
    // Check if we need to wrapp the Stmt in a q.submit or not.
    // If host code, simple print it out and place memcpy where appropriate

    // Store the kernel information for a kernel wrapped function in compile_to_oneapi
    // Only add device kernels to kernel_args
    kernel_args.push_back( std::tuple<Stmt, std::string, std::vector<DeviceArgument> >(s,name,args) );

    IsRunOnDevice device_run_checker;
    s.accept(&device_run_checker);
    // Emmit Host Code
    if(!device_run_checker.is_run_on_device){
        debug(2) << "Adding OneAPI Host kernel " << name << "\n";
        indent += 2;
        stream << "\n\n";
        stream << get_indent() << "std::cout << \"// kernel " << name << "\\n\";\n";
        stream << get_indent() << "// " << name << " begin \n";

        // Define all the buffer address spaces for the host side
        for (size_t i = 0; i < args.size(); i++) {
            if (args[i].is_buffer) {
                stream << "#define " << print_name(args[i].name) << " " << print_name(args[i].name) << "_host" <<  "\n";
            }
        }

        open_scope();
        stream << get_indent() << "int _addr_temp = 0;\n";
        print(s);
        
        close_scope(name);

        // Undef all the buffer address spaces, in case they're different in another kernel.
        for (size_t i = 0; i < args.size(); i++) {
            if (args[i].is_buffer) {
                stream << "#undef " << print_name(args[i].name) << "\n";
            }
        }
        stream << "\n\n";
        indent -= 2;
        return;
    }
    // Emmit Device Code
    else {
        debug(2) << "Adding OneAPI Device kernel " << name << "\n";
        indent += 2;

        KernelLoad loadcheker;
        KernelStore storechecker;
        s.accept(&loadcheker);
        s.accept(&storechecker);

        // The idea of where to place memory copies is as followes
        // KEY		
        // load	store	
        // 0	0	Do Nothing, no loads or stores used
        // 1	0	host->device & before
        // 0	1	device->host & after
        // 1	1	error 

        assert( !(loadcheker.stores_to_memory && storechecker.stores_to_memory) );

        stream << "\n\n";

        // Check if we need to copy before device kernel execution
        // Place a host->device is required before kernel execution
        if(loadcheker.stores_to_memory && !storechecker.stores_to_memory){
            for (size_t i = 0; i < args.size(); i++) {
                if (args[i].is_buffer) {
                    // e.g. q_device.submit([&](handler& h) { h.memcpy(_I_serializer_device, _I_serializer_host,  _deserializer->size_in_bytes() ); }).wait();
                    stream << get_indent() << "std::cout << \"// host->device memcpy\\n\";\n";
                    stream << get_indent() << "q_device.submit([&](handler& h){ h.memcpy( " 
                            << print_name(args[i].name) << "_device, "   // dst
                            <<  print_name(args[i].name) << "_host, "    // src
                            <<  print_name(args[i].name) << "_size"      // size
                            << "*sizeof(" << print_type(args[i].type) << ")"
                            <<  " ); }).wait();\n\n";
                }
            }
        }




        // Define all the buffer address spaces for the device side
        for (size_t i = 0; i < args.size(); i++) {
            if (args[i].is_buffer) {
                stream << "#define " << print_name(args[i].name) << " " << print_name(args[i].name) << "_device" <<  "\n";
            }
        }


        // Clean up before writting kernel
        vector<BufferSize> constants;
        {
            // Figure out which arguments should be passed in __constant.
            // Such arguments should be:
            // - not written to,
            // - loads are block-uniform,
            // - constant size,
            // - and all allocations together should be less than the max constant
            //   buffer size given by CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE.
            // The last condition is handled via the preprocessor in the kernel
            // declaration.
            for (size_t i = 0; i < args.size(); i++) {
                if (args[i].is_buffer &&
                    CodeGen_GPU_Dev::is_buffer_constant(s, args[i].name) &&
                    args[i].size > 0) {
                    constants.push_back(BufferSize(args[i].name, args[i].size));
                }
            }

            // Sort the constant candidates from smallest to largest. This will put
            // as many of the constant allocations in __constant as possible.
            // Ideally, we would prioritize constant buffers by how frequently they
            // are accessed.
            sort(constants.begin(), constants.end());

            // Compute the cumulative sum of the constants.
            for (size_t i = 1; i < constants.size(); i++) {
                constants[i].size += constants[i - 1].size;
            }

            // Store the buffer arguments so we can create a wrapper at a later time as well
            // Create preprocessor replacements for the address spaces of all our buffers.
            // stream << "\n\n// Address spaces for " << name << "\n";
            for (size_t i = 0; i < args.size(); i++) {
                if (args[i].is_buffer) {
                    // Only add device kernels buffer arguments to the class element `buffer_args`
                    buffer_args.push_back( args[i] );
                    vector<BufferSize>::iterator constant = constants.begin();
                    while (constant != constants.end() &&
                        constant->name != args[i].name) {
                        constant++;
                    }
                }
            }


        }


        // Emit OneAPI queue device submit tasks
        //###################### q.submit start here
        stream << get_indent() << "// " << name << "\n";
        stream << get_indent() << "std::cout << \"// kernel " << name << "\\n\";\n";
        stream << get_indent() << "oneapi_kernel_events.push_back( " << "q_device.submit([&](sycl::handler &h){\n";
        indent += 2;

        // Place anything you want before the kernel is submitied/defined here
        // Reinterpret half args passed as uint16 back to half
        for (size_t i = 0; i < args.size(); i++) {
            if (!args[i].is_buffer &&
                args[i].type.is_float() &&
                args[i].type.bits() < 32) {
                stream << " const " << print_type(args[i].type)
                    << " " << print_name(args[i].name)
                    << " = half_from_bits(" << print_name(args[i].name + "_bits") << ");\n";
            }
        }

        stream << get_indent() << "h.single_task<class " + name + "_class>([=](){\n";
        indent += 2;





        //###################### Output q.submit device kernel code start
        DeclareArrays da(this);
        s.accept(&da);
        stream << da.arrays.str();
        print(s);
        //###################### Output q.submit device kernel code end



        //###################### q.submit ends here
        indent -= 2;
        stream << get_indent() << "}); //  h.single_task " << name + "_class\n";
        indent -= 2;
        stream << get_indent() << "}) ); // q_device.submit\n";


        // Undef all the buffer address spaces, in case they're different in another kernel.
        for (size_t i = 0; i < args.size(); i++) {
            if (args[i].is_buffer) {
                stream << "#undef " << print_name(args[i].name) << "\n";
            }
        }



        // Check if we need to copy after device kernel execution
        // Place a device->host is required after kernel execution
        if(!loadcheker.stores_to_memory && storechecker.stores_to_memory){
            for (size_t i = 0; i < args.size(); i++) {
                if (args[i].is_buffer) {
                    // Wait for any kernels that were executing before to finish before doing a copy from device->host
                    stream << "\n\n";
                    stream << get_indent() << "while(oneapi_kernel_events_index < oneapi_kernel_events.size()){\n";
                    indent += 2;
                    stream << get_indent() << "oneapi_kernel_events.at(oneapi_kernel_events_index).wait();\n";
                    stream << get_indent() << "oneapi_kernel_events_index++;\n";
                    indent -= 2;
                    stream << get_indent() << "}\n";


                    // e.g. q_device.submit([&](handler& h) { h.memcpy(_I_serializer_device, _I_serializer_host,  _deserializer->size_in_bytes() ); }).wait();
                    stream << "\n\n";
                    stream << get_indent() << "std::cout << \"// device->host memcpy\\n\";\n";
                    stream << get_indent() << "q_device.submit([&](handler& h){ h.memcpy( " 
                            << print_name(args[i].name) << "_host, "            // dst
                            << print_name(args[i].name) << "_device, "          // src
                            << print_name(args[i].name) << "_size"              // size
                            << "*sizeof(" << print_type(args[i].type) << ")"
                            <<  " ); }).wait();\n";
                }
            }
        }


        indent -= 2;
        return;
    }

    // (NOTE) Everything below is old code that is no longer being used
    {
        debug(2) << "Adding OneAPI Device kernel " << name << "\n";
        indent += 2;

        // Store the kernel information for a kernel wrapped function in compile_to_oneapi
        // Only add device kernels to kernel_args
        kernel_args.push_back( std::tuple<Stmt, std::string, std::vector<DeviceArgument> >(s,name,args) );

        //debug(2) << "Eliminating bool vectors\n";
        //s = eliminate_bool_vectors(s);
        //debug(2) << "After eliminating bool vectors:\n"
        //         << s << "\n";

        // Figure out which arguments should be passed in __constant.
        // Such arguments should be:
        // - not written to,
        // - loads are block-uniform,
        // - constant size,
        // - and all allocations together should be less than the max constant
        //   buffer size given by CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE.
        // The last condition is handled via the preprocessor in the kernel
        // declaration.
        vector<BufferSize> constants;
        for (size_t i = 0; i < args.size(); i++) {
            if (args[i].is_buffer &&
                CodeGen_GPU_Dev::is_buffer_constant(s, args[i].name) &&
                args[i].size > 0) {
                constants.push_back(BufferSize(args[i].name, args[i].size));
            }
        }

        // Sort the constant candidates from smallest to largest. This will put
        // as many of the constant allocations in __constant as possible.
        // Ideally, we would prioritize constant buffers by how frequently they
        // are accessed.
        sort(constants.begin(), constants.end());

        // Compute the cumulative sum of the constants.
        for (size_t i = 1; i < constants.size(); i++) {
            constants[i].size += constants[i - 1].size;
        }

        // Store the buffer arguments so we can create a wrapper at a later time as well
        // Create preprocessor replacements for the address spaces of all our buffers.
        // stream << "\n\n// Address spaces for " << name << "\n";
        for (size_t i = 0; i < args.size(); i++) {
            if (args[i].is_buffer) {
                // Only add device kernels to buffer_args
                buffer_args.push_back( args[i] );
                vector<BufferSize>::iterator constant = constants.begin();
                while (constant != constants.end() &&
                    constant->name != args[i].name) {
                    constant++;
                }
            }
        }

        // Emit the function prototype.
        IsAutorun checker;
        s.accept(&checker);
        char *overlay_kernel_name = getenv("HL_OVERLAY_KERNEL");
        if (checker.is_autorun || (overlay_kernel_name != NULL && args.size() == 0)) {
            stream << "// __attribute__((max_global_work_dim(0)))\n";
            stream << "// __attribute__((autorun))\n";
        }


        // (TODO) Remove, old version of creating function wrappers around q.submit kernel
        {
            // stream << "\n\n";
            // stream << get_indent() << "sycl::event " << name << "(\n";
            // stream << get_indent() << " sycl::queue &q_device";
            // if(args.size() > 0) stream << ",\n";
            // for (size_t i = 0; i < args.size(); i++) {
            //     if (args[i].is_buffer) {
            //         // stream << get_indent() << " " << get_memory_space(args[i].name) << " ";
            //         stream << get_indent() << " ";
            //         // TODO: Update buffer attributes if written in kernel ip
            //         char *overlay_num = getenv("HL_OVERLAY_NUM");
            //         // print out the extends inputs as they do not currently carry over
            //         if (!args[i].write && overlay_num == NULL){ // !!!(HERE) This does not invoke what you need
            //             size_t dim = (size_t)args[i].dimensions;
            //             for(size_t j = 0; j < dim; j++){
            //                 stream << get_indent() << "const int " << print_name(args[i].name) << "_extent_" << j << ",\n";
            //             }
            //         }

            //         if (!args[i].write && overlay_num == NULL) stream << "const ";
            //         stream << print_type(args[i].type) << " *"
            //             << print_name(args[i].name);
            //         Allocation alloc;
            //         alloc.type = args[i].type;
            //         allocations.push(args[i].name, alloc);
            //     } else {
            //         Type t = args[i].type;
            //         string name = args[i].name;
            //         // Bools are passed as a uint8.
            //         t = t.with_bits(t.bytes() * 8);
            //         // float16 are passed as uints
            //         if (t.is_float() && t.bits() < 32) {
            //             t = t.with_code(halide_type_uint);
            //             name += "_bits";
            //         }
            //         stream << " const "
            //             << print_type(t)
            //             << " "
            //             << print_name(name);
            //     }
            //     if (i < args.size() - 1) stream << ",\n";
            // stream << ")\n";
            // open_scope();
        }

        // (TODO) Remove as we directly run the kernel
        for (size_t i = 0; i < args.size(); i++) {
            if (args[i].is_buffer) {
                Allocation alloc;
                alloc.type = args[i].type;
                allocations.push(args[i].name, alloc);
            }
        }


        // Emit OneAPI queue device submit tasks
        stream << get_indent() << "// " << name << "\n";
        // stream << get_indent() << "sycl::event "<< name << "_event = q_device.submit([&](sycl::handler &h){\n";
        stream << get_indent() << "oneapi_kernel_events.push_back( \n";
        stream <<get_indent() <<  "q_device.submit([&](sycl::handler &h){\n";
        indent += 2;
        stream << get_indent() << "h.single_task<class " + name + "_class>([=]()\n";
        open_scope();

        stream << get_indent() << "int _addr_temp = 0;\n";
        // Reinterpret half args passed as uint16 back to half
        for (size_t i = 0; i < args.size(); i++) {
            if (!args[i].is_buffer &&
                args[i].type.is_float() &&
                args[i].type.bits() < 32) {
                stream << " const " << print_type(args[i].type)
                    << " " << print_name(args[i].name)
                    << " = half_from_bits(" << print_name(args[i].name + "_bits") << ");\n";
            }
        }

        DeclareArrays da(this);
        s.accept(&da);
        stream << da.arrays.str();

        print(s);

        // Close kernel and device queue
        close_scope(name + "_class");
        stream << get_indent() << "); // h.single_task\n";
        indent -= 2;
        stream << get_indent() << "}); // q_device.submit kernel "  << name << "\n";
        stream << get_indent() << "); // oneapi_kernel_events ";


        // (TODO) Remove Not Needed for OneAPI code
        for (size_t i = 0; i < args.size(); i++) {
            // Remove buffer arguments from allocation scope
            if (args[i].is_buffer) {
                allocations.pop(args[i].name);
            }
        }

        // (TODO) Remove Not Needed for OneAPI code
        // Undef all the buffer address spaces, in case they're different in another kernel.
        // for (size_t i = 0; i < args.size(); i++) {
        //     if (args[i].is_buffer) {
        //         stream << "// " << "#undef " << get_memory_space(args[i].name) << "\n";
        //     }
        // }
    }

}


// Creates the wrapper function around all the host/device code
std::string CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::compile_oneapi_lower(const LoweredFunc &f, std::string str){
    // Wrapp existing q_device submit kernel code into a function
    // See void CodeGen_C::compile(const LoweredFunc &f) and
    //     void CodeGen_C::compile(const Module &input) for similar implementation

    std::ostringstream function_top;
    std::ostringstream function_btm;

    function_top << "\n\n";

    const std::vector<LoweredArgument> &args = f.args;

    std::vector<std::string> namespaces;
    std::string simple_name = extract_namespaces(f.name, namespaces);

    // Assuming C++ & compiling with Target::CPlusPlusNameMangling. No need to check for is_c_plus_plus_interface()
    if (!namespaces.empty()) {
        for (const auto &ns : namespaces) {
            function_top << "namespace " << ns << " {\n";
        }
        function_top << "\n";
    }

    // Emit the function prototype
    if (f.linkage == LinkageType::Internal) {
        // If the function isn't public, mark it static.
        function_top << "static ";
    }

    // function wrapper for device queue tasks
    function_top << "double " << simple_name << "(";

    // Print the deviceSelector as first input
    function_top << "const sycl::device_selector &deviceSelector";
    // Print arguments
    std::vector<LoweredArgument> buffer_input_args;
    {
        if( args.size() > 0){ function_top << ", "; }
        for (size_t i = 0; i < args.size(); i++) {
            if (i == 0){ function_top << get_indent(); }
            if(!args[i].is_buffer()) {
                function_top << print_type(args[i].type, AppendSpace)
                    << print_name(args[i].name);
            } else {
                buffer_input_args.push_back( args[i] );
                function_top << " Halide::Runtime::Buffer<" << print_type(args[i].type) << "," << (int)args[i].dimensions << "> *" << print_halide_buffer_name(args[i].name) ;
            }
            if (i < args.size() - 1) function_top << ", ";
        }
        function_top << ") {\n";
    }
    indent += 2;



    // Define dimentions of input buffers
    {
        function_top << "\n\n";
        function_top << get_indent() << "std::cout << \"// define dimentions of input buffers\\n\";\n";
        for(size_t i = 0; i < buffer_input_args.size(); i++){
            size_t dim = (size_t)buffer_input_args[i].dimensions;
            for(size_t j = 0; j < dim; j++){
                // Define each halide runtime buffer's stride, extend, & min
                function_top << get_indent() << "const int " << print_name(buffer_input_args[i].name) << "_stride_"<< j <<" = " << print_halide_buffer_name(buffer_input_args[i].name) << "->raw_buffer()->dim[" << j << "].stride;\n"
                            << get_indent() << "const int " << print_name(buffer_input_args[i].name) << "_min_"<< j <<" = " << print_halide_buffer_name(buffer_input_args[i].name) << "->raw_buffer()->dim[" << j << "].min;\n"
                            << get_indent() << "const int " << print_name(buffer_input_args[i].name) << "_extent_"<< j <<" = " << print_halide_buffer_name(buffer_input_args[i].name) <<  "->raw_buffer()->dim[" << j << "].extent;\n";
            }
        }
    }


    // Create device queue for each of the kernels
    // (NOTE) kernel_args is a class member that records what kernels have been added
    {
        function_top << "\n\n" << get_indent() << "std::cout << \"// creating device queues\\n\";\n";
        function_top << get_indent() << "sycl::queue q_host( sycl::host_selector{}, dpc_common::exception_handler, sycl::property::queue::enable_profiling());\n";
        function_top << get_indent() << "sycl::queue q_device(deviceSelector, dpc_common::exception_handler, sycl::property::queue::enable_profiling() );\n";
        function_top << get_indent() << "std::cout << \"//\\tHost: \" << q_host.get_device().get_info< sycl::info::device::name>() << \"\\n\";\n";
        function_top << get_indent() << "std::cout << \"//\\tDevice: \" << q_device.get_device().get_info< sycl::info::device::name>() << \"\\n\";\n";
    }




    // Allocade Deivce & Host memory
    std::set<std::string> defined_pointers; // contains all defined pointers to be freed at the end
    std::set<std::string> sylc_pointers;    // constins all created sycl malloc pointer names
    {
        function_top << "\n\n";
        function_top << get_indent() << "std::cout << \"// Allocating memory\\n\";\n";
        // for(size_t i = 0, j = 0, i < buffer_input_args.size() && j < buffer_args.size(); i++, j++ ){
        //     std::string halide_buffer_arg = print_halide_buffer_name( buffer_input_args[i].name );
        //     std::string kernel_buffer_arg = print_name(buffer_args[i].name); 
        // }

        // buffer_input_args are from the generall wrapper
        // buffer_args are from the kernels

        // define the neccessary host gerneral pointers 
        // e.g. T* <name>_host = <Name>_halide_buffer->begin();
        for(size_t i = 0, j = 0; i < buffer_input_args.size() && j < buffer_args.size(); i++, j++ ){
            std::string halide_buffer_arg = print_halide_buffer_name( buffer_input_args[i].name );
            function_top << get_indent() << print_type(buffer_input_args[i].type) << " *" << print_name(buffer_input_args[i].name)
                            << "_host = " << print_halide_buffer_name(buffer_input_args[i].name) << "->begin();\n";  
            function_top << get_indent() << print_type(buffer_input_args[i].type) << " *" << print_name(buffer_input_args[i].name)
                        << "_device = sycl::malloc_device<" << print_type(buffer_input_args[i].type) << ">("
                        <<  print_halide_buffer_name(buffer_input_args[i].name) << "->size_in_bytes(), q_device);\n";
            defined_pointers.insert( print_name(buffer_input_args[i].name) + "_host" );
            defined_pointers.insert( print_name(buffer_input_args[i].name) + "_device" );
            sylc_pointers.insert( print_name(buffer_input_args[i].name) + "_device" );
        }

        function_top << "\n\n";
        // define the necessary device pointers
        for(size_t i = 0; i < buffer_args.size(); i++){
            std::string ptr_base = print_name( buffer_args[i].name );
            std::vector<std::string> ptr_vec{ ptr_base + "_device", ptr_base + "_host" };
            for(size_t k = 0; k < ptr_vec.size(); k++){
                std::string curr_ptr = ptr_vec[k];
                if( (defined_pointers.find(curr_ptr) == defined_pointers.end()) ){

                    // find the matching input buffer if you can either get the size for the malloc or 
                    int input_buffer_index = -1;
                    for(size_t j = 0; j < buffer_input_args.size(); j++){
                        if( buffer_input_args[j].name.find_last_of(ptr_base) != std::string::npos ){
                            input_buffer_index = (int)j;
                            break;
                        }
                    }

                    // write the pointer
                    if( input_buffer_index != -1){
                        function_top << get_indent() << "// " << curr_ptr << " derived from " << print_halide_buffer_name(buffer_input_args[input_buffer_index].name) << "\n";
                        if( k == 0 ){
                            function_top << get_indent() << print_type(buffer_input_args[input_buffer_index].type) << " *" << curr_ptr
                                        << " = sycl::malloc_device<" << print_type(buffer_input_args[input_buffer_index].type) << ">("
                                        <<  print_halide_buffer_name(buffer_input_args[input_buffer_index].name) << "->size_in_bytes(), q_device);\n";
                        } else {
                            function_top << get_indent() << print_type(buffer_input_args[input_buffer_index].type) << " *" << curr_ptr
                                        << " = sycl::malloc_host<" << print_type(buffer_input_args[input_buffer_index].type) << ">("
                                        <<  print_halide_buffer_name(buffer_input_args[input_buffer_index].name) << "->size_in_bytes(), q_device);\n";
                            function_top << get_indent() << "// " << ptr_base << "_size derived from " << print_halide_buffer_name(buffer_input_args[input_buffer_index].name) << "\n";
                            function_top << get_indent() << "size_t " << ptr_base << "_size = " << print_halide_buffer_name(buffer_input_args[input_buffer_index].name) << "->size_in_bytes();\n";
                        }
                        defined_pointers.insert( curr_ptr );
                        sylc_pointers.insert( curr_ptr );
                    } else {
                        // Throw a user error when we are unable to figure out the memory
                        user_assert(false) << "unable to define memory `" << curr_ptr << "`\n";
                    }
                }

            }
        }
    }



    // Create sycl events vector to store all the input
    function_top << "\n\n";
    function_top << get_indent() << "std::vector<sycl::event> oneapi_kernel_events;\n";
    function_top << get_indent() << "int oneapi_kernel_events_index = 0;\n";


    // Free all defined sycl memory allocations
    {
        std::set<std::string>::iterator it = sylc_pointers.begin();
        while (it != sylc_pointers.end())
        {
            function_btm << get_indent() <<  "sycl::free( " << (*it) << ", q_device);\n";
            it++;
        }
    }

    // Get and return timing metrics
    {
        function_btm << "\n\n";
        function_btm << get_indent() << "std::cout << \"// return the kernel execution time in nanoseconds\\n\";\n";
        function_btm << ""
                    << get_indent() << "if(oneapi_kernel_events.size() > 0){\n"
                    << get_indent() << "	double k_earliest_start_time = oneapi_kernel_events.at(0).get_profiling_info<sycl::info::event_profiling::command_start>();\n"
                    << get_indent() << "	double k_latest_end_time = oneapi_kernel_events.at(0).get_profiling_info<sycl::info::event_profiling::command_end>();\n"
                    << get_indent() << "	for (unsigned i = 1; i < oneapi_kernel_events.size(); i++) {\n"
                    << get_indent() << "	  double tmp_start = oneapi_kernel_events.at(i).get_profiling_info<sycl::info::event_profiling::command_start>();\n"
                    << get_indent() << "	  double tmp_end = oneapi_kernel_events.at(i).get_profiling_info<sycl::info::event_profiling::command_end>();\n"
                    << get_indent() << "	  if (tmp_start < k_earliest_start_time) {\n"
                    << get_indent() << "         k_earliest_start_time = tmp_start;\n"
                    << get_indent() << "	  }\n"
                    << get_indent() << "	  if (tmp_end > k_latest_end_time) {\n"
                    << get_indent() << "         k_latest_end_time = tmp_end;\n"
                    << get_indent() << "	  }\n"
                    << get_indent() << "	}\n"
                    << get_indent() << "	// Get time in ns\n"
                    << get_indent() << "	double events_time = (k_latest_end_time - k_earliest_start_time);\n"
                    << get_indent() << "	return events_time;\n"
                    << get_indent() << "}\n";
        function_btm << get_indent() << "return (double)0;\n"
                    << "}\n";
    }

    // close out any namespaces used
    {
        if (!namespaces.empty()) {
            function_btm << "\n";
            for (size_t i = namespaces.size(); i > 0; i--) {
                function_btm << "}  // namespace " << namespaces[i - 1] << "\n";
            }
            function_btm << "\n";
        }
    }

    indent -= 2;



    // Insert the global values neededcd
    size_t top_pos = str.find(OneAPIDeclareChannels) + OneAPIDeclareChannels.size();
    if(top_pos == std::string::npos) top_pos = 0;
    str.insert( top_pos , function_top.str() );

    // Append the function to the end of all of this
    // str.append( function_top.str() );
    str.append( function_btm.str() );


    return str;
}






void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::print_global_data_structures_before_kernel(const Stmt *op) {
    // Define the compiler-generated vector and struct types.
    DefineVectorStructTypes def(this);
    def.mutate(*op);
    stream << def.vectors + def.structs;
    stream << OneAPIDefineVectorStructTypes;

    // Declare the output channels of the kernel.
    DeclareChannels decl(this);
    op->accept(&decl);
    stream << decl.channels;
    stream << OneAPIDeclareChannels;
}



void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::DeclareChannels::visit(const Realize *op) {
    if (ends_with(op->name, ".channel") || ends_with(op->name, ".channel.array")) {
        // Get the bounds in which all bounds are for the dimensions of the channel array, except the last one is for the min depth.
        Region bounds = op->bounds;
        std::string bounds_str = "";
        std::string attributes = "";
        std::string pipeAttributes = "";
        std::string pipeBounds = "";
        for (size_t i = 0; i < bounds.size(); i++) {
            Range b = bounds[i];
            Expr extent = b.extent;
            const IntImm *e_extent = extent.as<IntImm>();
            internal_assert(e_extent != NULL);
            if (i < bounds.size() - 1) {
                internal_assert(e_extent->value > 0);
                bounds_str += "[" + std::to_string(e_extent->value) + "]";
                pipeBounds += std::to_string(e_extent->value) + ",";
            } else {
                attributes = " __attribute__((depth(" + std::to_string(e_extent->value) +  "))) ";
                pipeAttributes = std::to_string(e_extent->value);
            }
        }
        if( pipeBounds[pipeBounds.size()-1] == ','){ pipeBounds.pop_back(); }
        internal_assert(op->types.size() == 1) << "In generating Intel OpenCL for FPGAs, a single type is expected for a channel.\n";
        std::string type = parent->print_type(op->types[0]);

        std::ostringstream oss;
        if (ends_with(op->name, ".channel")) {
            // oss << "channel " << type << " " << parent->print_name(op->name) << bounds_str << attributes << ";\n";
            if( pipeBounds != ""){
                // Using pipe arrays
                oss << "using " << parent->print_name(op->name) << " = PipeArray<class " << parent->print_name(op->name) << "Pipe, " << type << ", " << pipeAttributes << ", " << pipeBounds << ">;\n";
            } else {
                // Using regular pipes
                oss << "using " << parent->print_name(op->name) << " = sycl::ext::intel::pipe<class " << parent->print_name(op->name) << "Pipe, " << type << ", " << pipeAttributes << ">;\n";
            }
            channels += oss.str();
        } else {
            string printed_name = parent->print_name(op->name);
            string type_name = printed_name + "_t";
            size_t pos_last_token = printed_name.rfind('_');
            string channel_name = printed_name.substr(0, pos_last_token);
            oss << "typedef struct { " << type << " s" << bounds_str << "; } " << type_name << ";\n";
            oss << "using " << channel_name << " = sycl::ext::intel::pipe<class " << channel_name << "Pipe, " << type_name << ", " << pipeAttributes <<  ">;\n";
            channels += oss.str();
        }
    }
    IRVisitor::visit(op);
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::DeclareArrays::visit(const Call *op) {
    if (op->is_intrinsic(Call::write_array)) {
        string printed_name = parent->print_name(op->args[0].as<StringImm>()->value);
        string type_name = printed_name + "_t";
        if (array_set.find(printed_name) == array_set.end()) {
            array_set.insert(printed_name);
            arrays << parent->get_indent() << type_name << " " << printed_name << ";\n";
        }
    }
    return IRVisitor::visit(op);
}

/*
Check if shift reg has irregular bounds.
e.g.    realize shift regs [k, J-k] [0, K] [0, T]
          ...
            unrolled k = 0 to K
              unrolled j = k to J
                ...
The corresponding systolic array of above case is triangular in shape.
*/
bool CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::is_irregular(Region &bounds) {
    bool irregular_bounds = false;
    for (int i = bounds.size()-1; i >= 0; i--) {
        Expr extent = bounds[i].extent;
        if (!is_const(extent)) {
            irregular_bounds = true;
            break;
        }
    }
    return irregular_bounds;
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::gather_shift_regs_allocates(const Stmt *op) {
    GatherShiftRegsAllocates gatherer(this, shift_regs_allocates, shift_regs_bounds, space_vars);
    op->accept(&gatherer);
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::GatherShiftRegsAllocates::print_irregular_bounds_allocates(std::string reg_name, std::string type, std::string name, Region space_bounds, Region time_bounds, int space_bound_level) {
    Expr min = space_bounds[space_bound_level].min;
    Expr extent = space_bounds[space_bound_level].extent;
    const IntImm *e_min = min.as<IntImm>();
    const IntImm *e_extent = extent.as<IntImm>();
    internal_assert(e_min != NULL);
    internal_assert(e_extent != NULL);
    internal_assert(e_extent->value > 0);
    if (space_bound_level > 0) {
        Expr space_var = space_vars.at(reg_name)[space_bound_level];
        for (int i = 0; i < e_extent->value; i++) {
            int iter = e_min->value + i;
            std::string new_name = name + "_" + std::to_string(iter);
            Region new_space_bounds(space_bounds);

            for (int j = 0; j < space_bound_level; j++) {
                debug(4) << space_bounds[j].min << " " << space_bounds[j].extent <<"\n";
                new_space_bounds[j].min = simplify(substitute(space_var, iter, space_bounds[j].min));
                new_space_bounds[j].extent = simplify(substitute(space_var, iter, space_bounds[j].extent));
                debug(4) << new_space_bounds[j].min << " " << new_space_bounds[j].extent <<"\n";
            }
            print_irregular_bounds_allocates(reg_name, type, new_name, new_space_bounds, time_bounds, space_bound_level - 1);
        }
    } else {
        std::string bounds_str = "" ;
        // allocate time bounds as usual
        for (size_t i = 0; i < time_bounds.size(); i++) {
            Range b = time_bounds[i];
            Expr extent = b.extent;
            const IntImm *e_extent = extent.as<IntImm>();
            internal_assert(e_extent != NULL);
            internal_assert(e_extent->value > 0);
            bounds_str = bounds_str + "[" + std::to_string(e_extent->value) + "]";
        }

        for (int i = 0; i < e_extent->value; i++) {
            std::string new_name = name + "_" + std::to_string(e_min->value + i);
            debug(3) << "allocate shift regs " << type << " " << new_name << bounds_str << ";\n";
            ostringstream rhs;
            rhs << type << " " << new_name << bounds_str << ";\n";
            shift_regs_allocates[reg_name].push_back(rhs.str());
        }

    }
}

// gather loop info from the annotation
void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::GatherShiftRegsAllocates::visit(const Call *op) {
    if (op->is_intrinsic(Call::annotate) && op->args[0].as<StringImm>()->value == "Bounds") {
        std::string reg_name = op->args[1].as<StringImm>()->value;
        std::vector<Expr> vars(op->args.begin() + 2, op->args.end());
        space_vars[reg_name] = vars;
    } else {
        IRVisitor::visit(op);
    }
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::GatherShiftRegsAllocates::visit(const Realize *op) {
    if (ends_with(op->name, ".channel") || ends_with(op->name, ".channel.array")) {
    } else if (ends_with(op->name, ".shreg")) {
        ostringstream rhs;
        Region bounds = op->bounds;
        std::string bounds_str = "" ;
        std::string type = parent->print_type(op->types[0]);

        if (parent->is_irregular(bounds)) {
            debug(3) << op->name << " has irregular bounds. \n";
            /*
            For irregular case, we physically unroll the space dimension of shift registers.
            e.g.
            realize shreg[k, 3-k][0, 3][0, 100] of type float32
            [k, 3-k] and [0, 3] are space dimensions.
            The generated code should be like this:
            float shreg_0_0[100]; float shreg_0_1[100]; float shreg_0_2[100];
                                  float shreg_1_1[100]; float shreg_1_2[100];
                                                        float shreg_2_2[100];
            */

            std::string reg_name = extract_first_token(op->name);

            internal_assert(space_vars.find(reg_name) != space_vars.end());
            size_t space_var_num = space_vars[reg_name].size();

            // for (auto var : space_vars[reg_name])
            //     debug(2) << "space var " << var << "\n";

            // Divide reg bounds into space bounds and time bounds.
            // We only unroll the space part.
            Region space_bounds, time_bounds;
            for (size_t i = 0; i < bounds.size(); i++) {
                if (i < space_var_num)
                    space_bounds.push_back(bounds[i]);
                else
                    time_bounds.push_back(bounds[i]);
            }

            // recursively add index as suffix of reg name
            print_irregular_bounds_allocates(reg_name, type, parent->print_name(op->name), space_bounds, time_bounds, space_bounds.size() - 1);

            // debug(3) << shift_regs_allocates[reg_name];

        } else {
            for (int i = bounds.size()-1; i >= 0; i--) {
                Range b = bounds[i];
                Expr extent = b.extent;
                const IntImm *e_extent = extent.as<IntImm>();
                internal_assert(e_extent != NULL);
                internal_assert(e_extent->value > 0);
                bounds_str = "[" + std::to_string(e_extent->value) + "]" + bounds_str;
            }
            rhs << type << " " << parent->print_name(op->name) << bounds_str << ";\n";
            std::vector<std::string> names = split_string(op->name, ".");
            shift_regs_allocates[names[0]].push_back(rhs.str());
        }
        if (!parent->is_standard_opencl_type(op->types[0]) && op->types[0].is_vector()) {
            // Remember the bounds of the shift registers.
            shift_regs_bounds[op->name] = bounds.size();
        }
    } else if(ends_with(op->name,".temp")){
    } else if(ends_with(op->name,".ibuffer")){
    } else if (ends_with(op->name,".break")){
    } else {
        // Not really shift registers. But we can allocate them as shift regs as well.
        ostringstream rhs;
        Region bounds = op->bounds;
        std::string bounds_str = "" ;
        for (size_t i = 0; i < bounds.size(); i++) {
            Range b = bounds[i];
            Expr extent = b.extent;
            const IntImm *e_extent = extent.as<IntImm>();
            internal_assert(e_extent != NULL);
            internal_assert(e_extent->value > 0);
            bounds_str += "[" + std::to_string(e_extent->value) + "]";
        }
        std::string type = parent->print_type(op->types[0]);
        rhs << type << " " << parent->print_name(op->name) << bounds_str << ";\n";
        std::vector<std::string> names = split_string(op->name, ".");
        shift_regs_allocates[names[0]].push_back(rhs.str());
        if (!parent->is_standard_opencl_type(op->types[0]) && op->types[0].is_vector()) {
            // Remember the bounds of the shift registers.
            shift_regs_bounds[op->name] = bounds.size();
        }
    }
    IRVisitor::visit(op);
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::visit(const Realize *op) {
    if (ends_with(op->name, ".channel") || ends_with(op->name, ".channel")) {
        // We have already declared the channel before the kernel with print_global_data_structures_before_kernel().
        // Just skip it and get into the body.
        print_stmt(op->body);
    } else if (ends_with(op->name, ".shreg")) {
        // We have already gathered shift regs allocates with gather_shift_regs_allocates().
        // Just skip it and get into the body.
        print_stmt(op->body);
    } else if(ends_with(op->name,".temp")){
        std::string string_bound = "" ;
        std::vector<std::string> access_exprs;
        for (Range b : op->bounds) {
            access_exprs.push_back(print_expr(b.extent));
        }
        for (std::string ae : access_exprs) {
            string_bound += "[" + ae + "]";
        }
        for(size_t i=0;i<op->types.size();i++) {
            // do_indent();
            std::string name = op->name;
            stream << get_indent() << print_type(op->types[i]) << " " << print_name(name)
                << string_bound << ";\n";
        }
        print_stmt(op->body);
    } else if(ends_with(op->name,".ibuffer")){
        std::string string_bound = "" ;
        std::vector<std::string> access_exprs;
        for (Range b : op->bounds) {
            access_exprs.push_back(print_expr(b.extent));
        }
        for (std::string ae : access_exprs) {
            string_bound += "[" + ae + "]";
        }
        for(size_t i=0;i<op->types.size();i++) {
            std::string name = op->name.substr(0, op->name.length()-std::string(".ibuffer").size());
            string buffer_name = name + '.' + std::to_string(i) + ".ibuffer";
            stream << get_indent() << "// " << print_type(op->types[i]) << " ";
            stream << "__attribute__((memory, numbanks("
                   << access_exprs.back()
                   << "), singlepump, numwriteports(1), numreadports(1))) "
                   << print_name(buffer_name) << string_bound << ";\n";
            stream <<  get_indent() << "[[intel::fpga_memory(), "
                   << "intel::numbanks(" << access_exprs.back() << "), "
                   << "intel::singlepump, intel::simple_dual_port]] " <<  print_type(op->types[i])  << " "
                   << print_name(buffer_name) << string_bound << ";\n";
        }
        print_stmt(op->body);
    } else if (ends_with(op->name,".break")){
        print_stmt(op->body);
    } else {
        // We have treated this case as shift regs allocates with gather_shift_regs_allocates().
        // Just skip it and get into the body.
        print_stmt(op->body);
    }
}

void CodeGen_OneAPI_Dev::CodeGen_OneAPI_C::visit(const Provide *op){
    if (ends_with(op->name, ".ibuffer")) {
        internal_assert(op->values.size() == 1);
        string id_value = print_expr(op->values[0]);
        std::string name = op->name.substr(0, op->name.length()-std::string(".ibuffer").size());
        std::vector<std::string> access_exprs;
        for(size_t i = 0; i < op->args.size(); i++) {
            access_exprs.push_back(print_expr(op->args[i]));
        }
        string buffer_name = name + '.' + std::to_string(0) + ".ibuffer";
        stream << get_indent() << print_name(buffer_name);
        for(size_t i = 0; i < op->args.size(); i++) {
            stream << "[";
            stream << access_exprs[i];
            stream << "]";
        }
        stream << " = " << id_value << ";\n";
        cache.clear();
    } else if (ends_with(op->name, ".temp")) {
        internal_assert(op->values.size() == 1);
        string id_value = print_expr(op->values[0]);
        std::string name = op->name;
        std::vector<std::string> access_exprs;
        for(size_t i = 0; i < op->args.size(); i++) {
            access_exprs.push_back(print_expr(op->args[i]));
        }
        stream << get_indent() << print_name(name);
        // do_indent();
        for(size_t i = 0; i < op->args.size(); i++) {
            stream << "[";
            stream << access_exprs[i];
            stream << "]";
        }
        stream << " = " << id_value ;
        stream<< ";\n";
        cache.clear();
    }else if(ends_with(op->name,".break")){
        stream<<"break;\n";
    }
    else {
        CodeGen_C::visit(op);
    }
}





}  // namespace Internal
}  // namespace Halide
