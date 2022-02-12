#include <numeric>
#include <set>
#include "DebugPrint.h"
#include "Func.h"
#include "IR.h"
#include "IRMutator.h"
#include "IROperator.h"
#include "MinimizeShregs.h"
#include "Simplify.h"
#include "Substitute.h"
#include "Utilities.h"

namespace Halide {

using namespace Internal;
using std::vector;

Func &Func::relay(Func f, VarOrRVar loop) {
    vector<RelayItem> &relay_params = func.definition().schedule().relay_params();
    relay_params.push_back(RelayItem(f.name(), this->name(), loop.name()));
    return *this;
}

namespace Internal {

string undecorated_arg(Expr arg) {
    auto var = arg.as<Variable>();
    internal_assert(var);
    return extract_last_token(var->name);
}

int get_dim(const string &undecorated_arg, const ShiftRegAlloc &alloc) {
    auto it = std::find_if(alloc.args.begin(), alloc.args.end(),
                           [&](const Expr &e){ return extract_last_token(to_string(e)) == undecorated_arg; });
    return it - alloc.args.begin();
}

vector<int> find_linearized_output_dims(const vector<Dim> &dim_args, const ShiftRegAlloc &alloc) {
    // Get the index of dims to the alloc.args array
    vector<int> dims;
    for (auto &d : dim_args) {
        dims.push_back(get_dim(d.var, alloc));
    }
    // Get the index to the alloc.linearized_dims array
    vector<int> output_dims;
    for (size_t i = 0; i < alloc.linearized_dims.size(); i++) {
        auto &ldim = alloc.linearized_dims[i];

        bool found = true;
        for (auto &d : ldim) {
            auto it = std::find(dims.begin(), dims.end(), d);
            if (it == dims.end()) {
                found = false;
            }
        }
        if (found) {
            output_dims.push_back(i);
        }
    }
    return output_dims;
}

vector<string> find_output_loops(string bank_loop, const vector<int> &output_dims, const ShiftRegAlloc &alloc) {
    vector<string> loops;
    loops.push_back(bank_loop);
    // Linearized dims in output
    for (size_t o : output_dims) {
        for (size_t l : alloc.linearized_dims[o]) {
            loops.push_back(undecorated_arg(alloc.args[l]));
        }
    }
    // PE dims except bank loop
    for (int o : alloc.PE_dims) {
        if (o != get_dim(bank_loop, alloc)) {
            loops.push_back(undecorated_arg(alloc.args[o]));
        }
    }
    // Get the index of dims not in linearized_dims and PE_dims
    std::set<int> dims;
    for (auto &lin_dim : alloc.linearized_dims) {
        for (auto &d : lin_dim)
            dims.insert(d);
    }
    for (auto &d : alloc.PE_dims) {
        dims.insert(d);
    }
    for (size_t i = 0; i < alloc.args.size(); i++) {
        if (dims.count(i) == 0) {
            loops.push_back(undecorated_arg(alloc.args[i]));
        }
    }
    return loops;
}

// Data relaying executes like a pipeline. For example, for the PE dims iii and jjj, we would
// generate jjj pipelines where iii PEs share the one. PEs put its output value onto the pipeline,
// and the pipeline move forward. At the head of pipeline, we could collect data from all pipelines.
class DataRelaying : public IRMutator {
    const RelayItem &param;
    const ShiftRegAlloc &alloc;
    const vector<int> &output_dims;

    bool inside_pipe = false;
    struct {
        string name;
        string flag_name;
        Type t;
        Expr cond;
        Expr bank_extent;
        Expr PE_arg;
        Expr PE_extent;
        Expr lin_extent;
        Expr depth;
    } pipe_alloc;

    void get_pipe_alloc() {
        pipe_alloc.name = param.from_func + ".pipe.shreg";
        pipe_alloc.flag_name = param.from_func + ".pipe.flag.shreg";
        // Bank loop
        auto bank_it = std::find(alloc.PE_dims.begin(), alloc.PE_dims.end(),
                                get_dim(param.bank_loop, alloc));
        size_t bank = bank_it - alloc.PE_dims.begin();
        pipe_alloc.bank_extent = alloc.PE_extents[bank];
        // Other PE dims
        pipe_alloc.PE_arg = 0, pipe_alloc.PE_extent = 1;
        for (size_t i = 0; i < alloc.PE_dims.size(); i++) {
            if (i != bank) {
                pipe_alloc.PE_arg += (alloc.args[alloc.PE_dims[i]]) * pipe_alloc.PE_extent;
                pipe_alloc.PE_extent *= alloc.PE_extents[i];
            }
        }
        pipe_alloc.PE_arg    = simplify(pipe_alloc.PE_arg);
        pipe_alloc.PE_extent = simplify(pipe_alloc.PE_extent);
        // Linearized dims
        vector<Expr> lin_extents;
        for (auto &i : output_dims) {
            lin_extents.push_back(alloc.linearized_extents[i]);
        }
        pipe_alloc.lin_extent = std::accumulate(lin_extents.begin(), lin_extents.end(), Expr(1), std::multiplies<Expr>());
        pipe_alloc.lin_extent = simplify(pipe_alloc.lin_extent);
        pipe_alloc.depth = simplify(pipe_alloc.lin_extent * (pipe_alloc.PE_extent-1) + 1);
        debug(4) << "Pipe_alloc: "      << pipe_alloc.name
                 << "\n\t Bank extent: " << pipe_alloc.bank_extent
                 << "\n\t PE arg: "     << pipe_alloc.PE_arg
                 << "\n\t PE extent: "  << pipe_alloc.PE_extent
                 << "\n\t lin extent: " << pipe_alloc.lin_extent
                 << "\n\t depth: "      << pipe_alloc.depth;
    }

    // unrolled for (Z.pipe.b, 0, pipe_alloc.bank_extent) {
    //   unrolled for (Z.pipe.p, 0, pipe_alloc.PE_extent -1) {
    //     unrolled for (Z.pipe.l, 0, pipe_alloc.lin_extent -1) {
    //       write_shift_reg(_Z.pipe, Z.pipe.b, Z.pipe.p*pipe_alloc.lin_extent + Z.pipe.l,  read_shift_reg(_Z.pipe, Z.pipe.b, Z.pipe.p*pipe_alloc.lin_extent + Z.pipe.l + 1));
    //       if (Z.pipe.b == 0)
    //         write_shift_reg(_Z.pipe.flag, Z.pipe.b, Z.pipe.p*pipe_alloc.lin_extent + Z.pipe.l, read_shift_reg(_Z.pipe.flag, Z.pipe.b, Z.pipe.p*pipe_alloc.lin_extent + Z.pipe.l + 1));
    //     } // Z.pipe.l
    //     write_shift_reg(_Z.pipe, Z.pipe.b, Z.pipe.p*pipe_alloc.lin_extent + pipe_alloc.lin_extent-1, fpga_reg(read_shift_reg(_Z.pipe, Z.pipe.b, Z.pipe.p*pipe_alloc.lin_extent + pipe_alloc.lin_extent)));
    //     if (Z.pipe.b == 0)
    //       write_shift_reg(_Z.pipe.flag, Z.pipe.b, Z.pipe.p*pipe_alloc.lin_extent + pipe_alloc.lin_extent-1, fpga_reg(read_shift_reg(_Z.pipe.flag, Z.pipe.b, Z.pipe.p*pipe_alloc.lin_extent + pipe_alloc.lin_extent)));
    //   } // Z.pipe.p
    // } // Z.pipe.b
    // write_shift_reg(_Z.pipe.flag, 0, Z.pipe.depth-1, 0)
    //
    Stmt make_pipe_shift() {
        string lin_name  = pipe_alloc.name + ".l";
        string PE_name   = pipe_alloc.name + ".p";
        string bank_name = pipe_alloc.name + ".b";
        Expr var_lin  = Variable::make(Int(32), lin_name);
        Expr var_PE   = Variable::make(Int(32), PE_name);
        Expr var_bank = Variable::make(Int(32), bank_name);
        // Shift operation  
        Expr arg_shift = simplify(var_PE * pipe_alloc.lin_extent + var_lin);
        Expr read_shift = Call::make(pipe_alloc.t, Call::IntrinsicOp::read_shift_reg,
                                    { pipe_alloc.name, var_bank, arg_shift+1 }, Call::CallType::PureIntrinsic);
        Expr write_shift = Call::make(pipe_alloc.t, Call::IntrinsicOp::write_shift_reg, 
                                    { pipe_alloc.name, var_bank, arg_shift, read_shift }, Call::CallType::PureIntrinsic);
        // Shift operation for flag
        Expr read_flag_shift = Call::make(Bool(1), Call::IntrinsicOp::read_shift_reg, 
                                    { pipe_alloc.flag_name, var_bank, arg_shift+1 }, Call::CallType::PureIntrinsic);
        Expr write_flag_shift = Call::make(Bool(1), Call::IntrinsicOp::write_shift_reg,
                                    { pipe_alloc.flag_name, var_bank, arg_shift, read_flag_shift }, Call::CallType::PureIntrinsic);
        // Stmt bank_0 = IfThenElse::make(var_bank == 0, Evaluate::make(write_flag_shift));
        // Linear loop
        Stmt block_lin = Block::make(Evaluate::make(write_shift), Evaluate::make(write_flag_shift));
        Stmt for_lin = For::make(lin_name, 0, pipe_alloc.lin_extent-1, ForType::Unrolled, DeviceAPI::None, block_lin);
        // Shift operation at PE edges
        Expr arg_edge = simplify(var_PE * pipe_alloc.lin_extent + (pipe_alloc.lin_extent-1));
        Expr read_edge = Call::make(pipe_alloc.t, Call::IntrinsicOp::read_shift_reg,
                                    { pipe_alloc.name, var_bank, arg_edge+1 }, Call::CallType::PureIntrinsic);
        Expr read_edge_regs = Call::make(pipe_alloc.t, Call::IntrinsicOp::fpga_reg, { read_edge }, Call::CallType::PureIntrinsic);
        Expr write_edge = Call::make(pipe_alloc.t, Call::IntrinsicOp::write_shift_reg,
                                    { pipe_alloc.name, var_bank, arg_edge, read_edge_regs }, Call::CallType::PureIntrinsic);
        // Shift operation for flag at PE edges
        Expr read_flag_edge = Call::make(Bool(1), Call::IntrinsicOp::read_shift_reg,
                                    { pipe_alloc.flag_name, var_bank, arg_edge+1 }, Call::CallType::PureIntrinsic);
        Expr read_flag_edge_regs = Call::make(pipe_alloc.t, Call::IntrinsicOp::fpga_reg, { read_flag_edge }, Call::CallType::PureIntrinsic);
        Expr write_flag_edge = Call::make(Bool(1), Call::IntrinsicOp::write_shift_reg,
                                    { pipe_alloc.flag_name, var_bank, arg_edge, read_flag_edge_regs }, Call::CallType::PureIntrinsic);
        // Stmt edge_bank_0 = IfThenElse::make(var_bank == 0, Evaluate::make(write_flag_edge));
        // PE loop
        Stmt block_PE = Block::make({ for_lin, Evaluate::make(write_edge), Evaluate::make(write_flag_edge) });
        Stmt for_PE = For::make(PE_name, 0, pipe_alloc.PE_extent-1, ForType::Unrolled, DeviceAPI::None, block_PE);
        // Bank loop
        Expr write_flag_last = Call::make(Bool(1), Call::IntrinsicOp::write_shift_reg,
                                    { pipe_alloc.flag_name, var_bank, pipe_alloc.depth-1, 0 }, Call::CallType::PureIntrinsic);
        Stmt block_bank = Block::make(for_PE, Evaluate::make(write_flag_last));
        Stmt for_bank = For::make(bank_name, 0, pipe_alloc.bank_extent, ForType::Unrolled, DeviceAPI::None, block_bank);
        return for_bank;
    }

    // unrolled for (Z.pipe.b, 0, pipe_alloc.bank_extent) {
    //   if (read_shift_reg(Z.pipe.flag, 0, 0)) {
    //     write_channel(Out.channel, read_shift_reg(Z.pipe, jjj, 0), jjj, 0)
    //   }
    // }
    Stmt make_write() {
        string bank_name = pipe_alloc.name + ".b";
        Expr var_bank = Variable::make(Int(32), bank_name);
        // Write data to channel
        Expr read_pipe = Call::make(pipe_alloc.t, Call::IntrinsicOp::read_shift_reg,
                                    { pipe_alloc.name, var_bank, 0 }, Call::CallType::PureIntrinsic);
        vector<Expr> write_args;
        write_args.push_back(param.to_func + ".channel");
        write_args.push_back(read_pipe);
        write_args.push_back(var_bank);
        Expr write_chn = Call::make(pipe_alloc.t, Call::IntrinsicOp::write_channel, write_args, Call::CallType::PureIntrinsic);
        // Flag is true
        Expr read_flag = Call::make(Bool(1), Call::IntrinsicOp::read_shift_reg,
                                    { pipe_alloc.flag_name, var_bank, 0 }, Call::CallType::PureIntrinsic);
        Stmt if_flag = IfThenElse::make(read_flag, Evaluate::make(write_chn));
        // Bank loop
        Stmt for_bank = For::make(bank_name, 0, pipe_alloc.bank_extent, ForType::Unrolled, DeviceAPI::None, if_flag);
        return for_bank;
    }

    // unrolled for (Z.pipe.b, 0, pipe_alloc.bank_extent) {
    //   unrolled for (Z.pipe.d, 0, pipe_alloc.depth) {
    //     write_shift_reg(Z.pipe.flag, Z.pipe.b, Z.pipe.d, 0)
    //   }
    // }
    Stmt make_flag_init() {
        string bank_name = pipe_alloc.name + ".b";
        string depth_name = pipe_alloc.name + ".d";
        Expr var_bank = Variable::make(Int(32), bank_name);
        Expr var_depth = Variable::make(Int(32), depth_name);
        // Initialize
        Expr write_flag = Call::make(Bool(1), Call::IntrinsicOp::write_shift_reg,
                                    { pipe_alloc.flag_name, var_bank, var_depth, 0 }, Call::CallType::PureIntrinsic);
        Stmt for_depth = For::make(depth_name, 0, pipe_alloc.depth, ForType::Unrolled, DeviceAPI::None, Evaluate::make(write_flag));
        Stmt for_bank = For::make(bank_name, 0, pipe_alloc.bank_extent, ForType::Unrolled, DeviceAPI::None, for_depth);
        return for_bank;
    }

public:
    using IRMutator::visit;

    Expr visit(const Call *op) override {
        if (op->is_intrinsic(Call::IntrinsicOp::read_channel)) {
            // Add a guarding condition to the read_channel
            if (inside_pipe) {
                Expr sele = Select::make(pipe_alloc.cond, op, FloatImm::make(op->type, 0));
                return sele;
            }
            auto name = op->args[0].as<StringImm>();
            internal_assert(name);
            if (name->value == param.to_func + ".channel") {
                vector<Expr> args;
                args.push_back(name->value);
                for (auto &e : op->args) {
                    auto v = e.as<Variable>();
                    if (v && extract_last_token(v->name) == param.bank_loop) {
                        // Other dimensions are encapsulated as the pipes inside the systolic array
                        args.push_back(e);
                    }
                }
                return Call::make(op->type, Call::IntrinsicOp::read_channel, args, Call::CallType::PureIntrinsic);
            }
        }
        return IRMutator::visit(op);
    }

    Stmt visit(const Evaluate *op) override {
        auto call = op->value.as<Call>();
        if (call && call->is_intrinsic(Call::IntrinsicOp::write_channel)) {
            // Replace the write_channel with write_shift_reg to the pipe
            auto p = call->args[0].as<StringImm>();
            internal_assert(p);
            if (inside_pipe && p->value == param.to_func + ".channel") {
                vector<Expr> args;
                args.push_back(pipe_alloc.name);
                args.push_back(alloc.args[get_dim(param.bank_loop, alloc)]);
                args.push_back(pipe_alloc.lin_extent * pipe_alloc.PE_arg);
                args.push_back(call->args[1]);    // Value
                Expr write_pipe = Call::make(call->type, Call::IntrinsicOp::write_shift_reg, args, Call::CallType::PureIntrinsic);
                args[0] = pipe_alloc.flag_name, args[3] = 1;
                Expr write_flag = Call::make(Bool(1), Call::IntrinsicOp::write_shift_reg, args, Call::CallType::PureIntrinsic);
                return Block::make(Evaluate::make(write_pipe), Evaluate::make(write_flag));
            }
        }
        return IRMutator::visit(op);
    }

    Stmt visit(const For *op) override {
        // Record the outermost loop's extent to be used for guarding read_channel
        if (op->name == to_string(alloc.args.back())) {
            pipe_alloc.cond = Variable::make(Int(32), op->name) < op->extent;
        }
        Stmt body = mutate(op->body);
        // Modify the outermost loop's extent to keep the systolic array running 
        // when all computations are finished, to relay data out of systolic array
        if (op->name == to_string(alloc.args.back())) {
            body = For::make(op->name, op->min, op->extent + 1,
                             op->for_type, op->device_api, body);
            return Block::make(make_flag_init(), body);
        }
        if (op->name == to_string(alloc.args[alloc.PE_dims.back()])) {
            body = For::make(op->name, op->min, op->extent,
                             op->for_type, op->device_api, body);
            body = Block::make({ body, make_write(), make_pipe_shift() });
            return body;
        }
        return IRMutator::visit(op);
    }

    Stmt visit(const ProducerConsumer *op) override {
        if (op->is_producer && op->name == param.from_func) {
            inside_pipe = true;
            Stmt body = mutate(op->body);
            inside_pipe = false;
            return ProducerConsumer::make(op->name, op->is_producer, body);
        }
        return IRMutator::visit(op);
    }

    Stmt visit(const Realize *op) override {
        if (op->name == param.from_func + ".shreg") {
            pipe_alloc.t = op->types[0];
        }
        Stmt body = mutate(op->body);
        // Other dimensions are encapsulated inside the systolic array
        if (op->name == param.to_func + ".channel") {
            Region channel_bounds;
            channel_bounds.push_back(Range(0, pipe_alloc.bank_extent));
            channel_bounds.push_back(Range(0, pipe_alloc.lin_extent));
            return Realize::make(op->name, op->types, op->memory_type, channel_bounds, op->condition, body);
        }
        // We need to reserve sufficient space to accomodate output values
        // Like Z_pipe[0..JJJ][0..JJ*II*(III-1)+1], where JJJ and III are the
        // exetent of PE dims, and JJ*II is the extent of linearized dims.
        if (op->name == param.from_func + ".shreg") {
            Region pipe_bounds;
            pipe_bounds.push_back(Range(0, pipe_alloc.bank_extent));
            pipe_bounds.push_back(Range(0, pipe_alloc.depth));
            body = Realize::make(pipe_alloc.name, op->types, op->memory_type, pipe_bounds, const_true(), body);
            // Track status for each lanes
            body = Realize::make(pipe_alloc.flag_name, { Bool(1) }, op->memory_type, pipe_bounds, const_true(), body);
        }
        return Realize::make(op->name, op->types, op->memory_type, op->bounds, op->condition, body);
    }

    DataRelaying(const RelayItem &_p, const ShiftRegAlloc &_a, const vector<int> &_o)
        : param(_p), alloc(_a), output_dims(_o) {
            get_pipe_alloc();
        }
};


class LateReorder : public IRMutator {
    const vector<string> &funcs;
    const vector<string> &loops;
    int loop_level;
    string func;

    struct LoopInfo {
        Expr min, extent;
        ForType for_type;
        DeviceAPI device_api;
    };
    map<string, LoopInfo> ori_loops;

public:
    using IRMutator::visit;

    LateReorder(const vector<string> &_f, const vector<string> &_l)
        : funcs(_f), loops(_l) {}

    Stmt visit(const ProducerConsumer *op) override {
        if (op->is_producer) {
            auto it = std::find(funcs.begin(), funcs.end(), op->name);
            loop_level = it != funcs.end() ? loops.size()-1 : -1;
            func = it != funcs.end() ? op->name : "";
            ori_loops.clear();
        }
        return IRMutator::visit(op);
    }

    Expr visit(const Call *op) override {
        auto it = std::find(funcs.begin(), funcs.end(), op->name);
        if (it != funcs.end()) {
            vector<Expr> args;
            for (auto &l : loops) {
                string name = func + ".s0." + l;
                args.push_back(Variable::make(Int(32), name));
            }
            return Call::make(op->type, op->name, args, op->call_type, op->func);
        }
        return IRMutator::visit(op);
    }

    Stmt visit(const Provide *op) override {
        auto it = std::find(funcs.begin(), funcs.end(), op->name);
        if (it != funcs.end()) {
            vector<Expr> values;
            for (auto &v : op->values) {
                values.push_back(mutate(v));
            }
            vector<Expr> args;
            for (auto &l : loops) {
                string name = func + ".s0." + l;
                args.push_back(Variable::make(Int(32), name));
            }
            return Provide::make(op->name, values, args);
        }
        return IRMutator::visit(op);
    }

    Stmt visit(const For *op) override {
        if (func.empty() || ends_with(op->name, "run_on_device")) {
            return IRMutator::visit(op);
        }
        ori_loops[op->name] = { op->min, op->extent, op->for_type, op->device_api };
        loop_level -= 1;
        Stmt body = mutate(op->body);
        loop_level += 1;

        internal_assert(loop_level >= 0 && loop_level < (int)loops.size());
        string name = extract_before_tokens(op->name, 2) + "." + loops[loop_level];
        LoopInfo &l = ori_loops.at(name);
        return For::make(name, l.min, l.extent, l.for_type, l.device_api, body);
    }
};

Stmt late_reorder_along_consumer_chain(Stmt s, const std::map<std::string, Function> &env, string func, const vector<string> &loops) {
    // Construct the consumer chain
    vector<string> consumers;
    while (1) {
        bool is_last = true;
        for (auto &kv : env) {
            if (kv.second.isolated_from_as_consumer() == func) {
                is_last = false;
                consumers.push_back(kv.first);
                func = kv.first;
                break;
            }
        }
        if (is_last) break;
    }
    LateReorder lr(consumers, loops);
    s = lr.mutate(s);
    return s;
}

Stmt relay_data(Stmt s, const std::map<std::string, Function> &env, const map<string, ShiftRegAlloc> &func_to_regalloc) {
    for (auto &kv : env) {
        const vector<RelayItem> &relay_params = kv.second.definition().schedule().relay_params();
        if (!relay_params.empty()) {
            user_assert(relay_params.size() == 1)
                << "Please call relay on the output function only once.\n";
            auto from_func = relay_params[0].from_func;
            const auto &dim_args = kv.second.definition().schedule().dims();
            const auto &alloc = func_to_regalloc.at(from_func);
            auto output_dims = find_linearized_output_dims(dim_args, alloc);

            DataRelaying dr(relay_params[0], alloc, output_dims);
            s = dr.mutate(s);

            // The data layout is altered after data relaying, so we reorder loops in the consumer
            vector<string> loops = find_output_loops(relay_params[0].bank_loop, output_dims, alloc);
            internal_assert(loops.size() == dim_args.size()-1)
                << "We are performing data relaying. However, your spec follows a pattern we have "
                << "not seen before, and thus correctness cannot be guaranteed.\n";
            s = late_reorder_along_consumer_chain(s, env, kv.first, loops);
        }
    }
    return s;
}

} // Internal
} // Halide