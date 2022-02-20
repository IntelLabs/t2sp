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
#include "./DebugPrint.h"
#include "./Utilities.h"
#include "./PreprocessBeforeLower.h"
#include "./Stensor.h"

namespace Halide {

using namespace Internal;
using std::vector;
using std::map;
using std::string;

struct Schain {
    bool is_output;                 // Output chain needs different primitives
    Func outf;                      // The output chain starts from a function
    vector<ImageParam> imp;         // The input chain starts from external input
    vector<Stensor> stensors;
};
vector<Schain> schains;

Stensor &Stensor::scope(Var v) {
    v_scope = v;
    return *this;
}

Stensor &Stensor::banks(const vector<Var> &v) {
    if (v.empty()) {
        // By default, this stensor will output a scalar each time.
        return *this;
    }
    v_banks = v;
    return *this;
}

Stensor &Stensor::out(const vector<Var> &v) {
    if (v.empty()) {
        // By default, this stensor will output a scalar each time.
        return *this;
    }
    v_outs = v;
    return *this;
}

Stensor &Stensor::operator()(const vector<Expr> &d) {
    if (d.empty()) {
        // By default, this stensor will use the original layout.
        return *this;
    }
    dims = d;
    return *this;
}

Stensor &Stensor::operator>>(Stensor &s) {
    int c = this->schain_idx;
    internal_assert(c >= 0);
    s.schain_idx = c;
    schains[c].stensors.push_back(s);
    return schains[c].stensors.back();
}

Stensor &operator>>(const vector<ImageParam> &im, Stensor &s) {
    Schain tmp;
    s.schain_idx = schains.size();
    tmp.is_output = false;
    tmp.imp = im;
    tmp.stensors.push_back(s);
    schains.push_back(std::move(tmp));
    return schains.back().stensors.back();
}

Stensor &operator>>(const ImageParam &im, Stensor &s) {
    return vector<ImageParam>{im} >> s;
}

Stensor &operator>>(Func &f, Stensor &s) {
    Schain tmp;
    s.schain_idx = schains.size();
    tmp.is_output = true;
    tmp.outf = f;
    tmp.stensors.push_back(s);
    schains.push_back(std::move(tmp));
    return schains.back().stensors.back();
}

struct FindVars
{
    Func ure;
    vector<Var> free_vars;      // Variables appeared in the function definition

    int var_index(Var v) {
        for (size_t i = 0; i < free_vars.size(); i++) {
            if (v.same_as(free_vars[i]))
                return i;
        }
        return -1;
    }

    bool exists(const vector<Var> &vars) {
        for (auto &v : vars) {
            if (var_index(v) == -1) {
                return false;
            }
        }
        return true;
    }

    bool exists(Var v) {
        return this->exists(vector<Var>{v});
    }

    vector<VarOrRVar> find_reuse_vars(string imp, Var scope) {
        vector<VarOrRVar> reuse_vars;
        auto &used_vars = fuv.used_vars;
        internal_assert(used_vars.count(imp) > 0);

        for (Var v : free_vars) {
            if (v.same_as(scope)) {
                break;
            }
            if (used_vars[imp].count(v.name()) == 0) {
                reuse_vars.push_back(Var(v));
            }
        }
        return std::move(reuse_vars);
    }

    // Find the first var below/above the given var v whose extent is not 1
    Var find_non_unit_var(Var v, bool above = true) {
        size_t j = var_index(v);
        while (j > 0 && j < free_vars.size()) {
            auto bound = ure.function().get_bounds(free_vars[j].name());
            if (!is_one(bound.second)) break;
            j = above ? j + 1 : j - 1;
        }
        return free_vars[j];
    }

    // Find variables appeared in the arguments of inputs
    class FindUsedVars : public IRVisitor
    {
        string image_param;
    public:
        using IRVisitor::visit;
        map<string, std::set<string>> used_vars;

        void visit(const Variable *op) override {
            if (!image_param.empty()) {
                used_vars[image_param].insert(op->name);
            }
        }

        void visit(const Call *op) override {
            if (ends_with(op->name, "_im")) {
                image_param = remove_postfix(op->name, "_im");
                for (size_t i = 0; i < op->args.size(); i++) {
                    op->args[i].accept(this);
                }
                image_param.clear();
            }
        }
    } fuv;

    FindVars(const map<string, Func> &env) {
        for (auto &p : env) {
            const Func &f = p.second;
            f.value().accept(&fuv);
            // UREs have the same iteration space, so we just check the one applied merge_ures
            if (f.function().has_merged_defs()) {
                ure = f;
                for (auto &d : f.function().definition().schedule().dims()) {
                    free_vars.push_back(d.var);
                }
            }
        }
        user_assert(ure.defined())
            << "Cannot find merged UREs. Do you forget to apply merge_ures?";
    }
};

// We assume an output function always follows such a pattern:
// Out(...) = select(cond, Z(...)),
// in this case, we find and set func_name as Z
class FindProducerForOutput : public IRVisitor {
    const map<string, Func> env;
public:
    using IRVisitor::visit;
    Func producer;

    void visit(const Select *op) override {
        if (!op->false_value.defined()) {
            auto call = op->true_value.as<Call>();
            internal_assert(call);
            if (call->call_type == Call::CallType::Halide) {
                producer = env.at(call->name);
            }
        }
    }

    FindProducerForOutput(const map<string, Func> &_e)
        : env(_e) {}
};


class RealizeOnFPGA
{
    vector<Var> output_array_dims;
    FindVars &fv;
    FindProducerForOutput &fpo;

    vector<Func> isolate_producer(Schain &c) {
        if (c.stensors[0].position != HOST) {
            // The device stensors needs serialized inputs
            // If the host stensor is not specified, we automatically generate it
            string host_name = c.imp[0].name() + "_serializer";
            Stensor s_host(host_name);
            s_host.schain_idx = c.stensors[0].schain_idx;
            c.stensors.insert(c.stensors.begin(), s_host);
        }
        vector<Func> producers;
        for (auto &s : c.stensors) {
            Place place = s.position == SMemType::HOST ? Place::Host : Place::Device;
            Func isolated_func(s.name, place);
            producers.push_back(std::move(isolated_func));
        }
        vector<FuncOrExpr> imp;
        std::copy(c.imp.begin(), c.imp.end(), std::back_inserter(imp));
        fv.ure.isolate_producer_chain(imp, producers);
        debug(1) << fv.ure.name() << ".isolate_producer_chain({"
                 << names_to_string(c.imp) << "}, " << names_to_string(producers) << ");\n";
        return std::move(producers);
    }

#if 0
    void generate_output_array(Func out, Func drainer) {
        // TODO: check non-output-stationary dataflow
        auto src_vars = fv.ure.function().definition().schedule().transform_params()[0].src_vars;
        vector<string> pe_array_dims(src_vars.begin(), src_vars.end()-1);
        auto func_dims = out.function().args();

        for (auto u : pe_array_dims) {
            for (auto o : func_dims) {
                if (o == u)
                    output_array_dims.push_back(Var(o));
            }
        }
        drainer.space_time_transform(output_array_dims);
        debug(1) << drainer.name() << ".space_time_transform("
                 << names_to_string(output_array_dims) << ");\n";
    }
#endif

    vector<Func> isolate_consumer(Schain &c) {
        vector<Func> consumers;
        if (c.stensors.back().position != HOST) {
            // If the host stensor is not specified, we automatically generate it
            string host_name = c.outf.name() + "_deserializer";
            Stensor s_host(host_name);
            s_host.schain_idx = c.stensors[0].schain_idx;
            c.stensors.push_back(s_host);
        }

        // Isolate subsequent consumers
        for (auto &s : c.stensors) {
            Place place = s.position == SMemType::HOST ? Place::Host : Place::Device;
            Func new_func(s.name, place);
            consumers.push_back(std::move(new_func));
        }
        if (c.stensors[0].v_banks.size() == 1) {
            // This is a special case where the single dimension banks are inside systolic array
            user_assert(c.stensors[1].position == SMemType::DRAM);
            Var bank = c.stensors[0].v_banks[0];
            c.outf.value().accept(&fpo);
            c.outf.relay(fpo.producer, bank);
            debug(1) << c.outf.name() << ".relay("
                     << fpo.producer.name() << ", " << bank.name() << ");\n";
            // The channel is inside the systolic array
            if (c.stensors[0].fifo_depth != 0) {
                c.outf.min_depth(c.stensors[0].fifo_depth);
            }
            // Remove the first stensor as it is inside systolic array
            c.stensors.erase(c.stensors.begin());
            consumers.erase(consumers.begin());
            // Vectorize all the subsequent stensors
            c.outf.isolate_consumer_chain(consumers);
            debug(1) << c.outf.name() << ".isolate_consumer_chain("
                     << names_to_string(consumers) << ");\n";
            for (auto &f : consumers) {
                f.vectorize(bank);
                debug(1) << f.name() << ".vectorize("
                         << bank.name() << ");\n";
            }
        } else if (c.stensors[0].v_banks.size() == 2) {
            // The output stensor inherits loops of the output URE, generally less than that of systolic array
            // So we isolate the first consumer alone and apply space-time transform to regenerate loop structure,
            // then the subsequent stensors could be isolated based on that
            Func first_func = consumers[0];
            c.outf.isolate_consumer(first_func);
            debug(1) << c.outf.name() << ".isolate_consumer("
                     << first_func.name() << ");\n";
            // generate_output_array(outf, f_dev);
            first_func.space_time_transform(c.stensors[0].v_banks);
            debug(1) << first_func.name() << ".space_time_transform("
                     << names_to_string(c.stensors[0].v_banks) << ");\n";
            vector<Func> other_cons(consumers.begin()+1, consumers.end());
            first_func.isolate_consumer_chain(other_cons);
            debug(1) << first_func.name() << ".isolate_consumer_chain("
                     << names_to_string(other_cons) << ");\n";
        } else {
            c.outf.isolate_consumer_chain(consumers);
            debug(1) << c.outf.name() << ".isolate_consumer_chain("
                     << names_to_string(consumers) << ");\n";
        }
        return std::move(consumers);
    }

    // Remove reuse variables from stensors as per their scope
    void remove(Schain &c, vector<Func> &producers) {
        vector<VarOrRVar> loops;
        Var scope = c.stensors.back().v_scope;

        for (int i = producers.size()-2; i >= 0; i--) {
            loops = fv.find_reuse_vars(c.imp[0].name(), scope);
            producers[i].remove(loops);
            debug(1) << producers[i].name() << ".remove("
                     << names_to_string(loops) << ");\n";
            scope = (i > 0) ? c.stensors[i].v_scope : scope;
        }
    }

    Var find_differences(vector<Var> set_a, vector<Var> set_b) {
        for (auto &a : set_a) {
            bool found = false;
            for (auto &b : set_b)
                if (a.name() == b.name()) found = true;
            if (!found) return a;
        }
        return Var("");
    }

    // The scatter primitive only applies to the stensors with increasing dimensional banks (0->1, 1->2)
    void scatter(Schain &c, vector<Func> &producers) {
        internal_assert(c.stensors.size() == producers.size());
        vector<Var> &prev_dims = c.stensors[0].v_banks;

        for (size_t i = 1; i < c.stensors.size(); i++) {
            auto v_banks = c.stensors[i].v_banks;
            if (v_banks.size() == prev_dims.size()+1) {
                Func prev = producers[i-1];
                Var v_scatter = find_differences(v_banks, prev_dims);
                producers[i].scatter(prev, v_scatter);
                debug(1) << producers[i].name() << ".scatter("
                        << prev.name() << ", " << v_scatter << ");\n";
            }
            prev_dims = v_banks;
        }
    }

    // The gather primitive only applies to the stensors with decreasing dimensional banks (2->1, 1->0)
    void gather(Schain &c, vector<Func> &consumers) {
        internal_assert(c.stensors.size() == consumers.size());
        auto &prev_dims = c.stensors[0].v_banks;

        for (size_t i = 1; i < c.stensors.size(); i++) {
            auto v_banks = c.stensors[i].v_banks;
            if (v_banks.size() == prev_dims.size()-1) {
                Func prev_1 = consumers[i-1];
                Func prev_2 = (i == 1) ? c.outf : consumers[i-2];
                Var v_gather = find_differences(prev_dims, v_banks);
                prev_1.gather(prev_2, v_gather);
                debug(1) << prev_1.name() << ".gather("
                         << prev_2.name() << ", " << v_gather << ");\n";
                // Trick: The behavior of gather depends on bank dimensions
                // 2->1: Values transferred one by one via shift registers
                // 1->0: Values are gathered across banks and sent as a vector,
                //       to simplify vectorize phase, we perform it here
                if (v_banks.size() == 0) {
                    // producer
                    prev_1.vectorize(v_gather);
                    debug(1) << prev_1.name() << ".vectorize("
                             << v_gather << ");\n";
                    // consumer
                    consumers[i].vectorize(v_gather);
                    debug(1) << consumers[i].name() << ".vectorize("
                             << v_gather << ");\n";
                }
            }
            prev_dims = v_banks;
        }
    }

    void buffer(Schain &c, vector<Func> &producers) {
        internal_assert(c.stensors.size() == producers.size());
        for (size_t i = 0; i < c.stensors.size(); i++) {
            Var v_scope = c.stensors[i].v_scope;
            if (fv.exists(v_scope) && c.stensors[i].position == SRAM) {
                Func prev = producers[i-1];
                producers[i].buffer(prev, v_scope);
                debug(1) << producers[i].name() << ".buffer("
                         << prev.name() << ", " << v_scope << ");\n";
            }
        }
    }

    void vectorize(Schain &c, vector<Func> &funcs) {
        internal_assert(c.stensors.size() == funcs.size());
        // In general, each stensor could independently specify bankwidth,
        // so we do not check the consistency between the producer and consumer,
        // NOTE: Currently the producer and consumer must be consistent with manual work,
        // and we leave the sophisticated vectorization to future work
        for (size_t i = 0; i < c.stensors.size(); i++) {
            if (c.stensors[i].v_width.size() == 0)
                continue;
            user_assert(c.stensors[i].v_width.size() == 1)
                << "Currently we only support packing one dimension as a vector\n";
            Var v_width = c.stensors[i].v_width[0];
            if (fv.exists(v_width)) {
                funcs[i].vectorize(v_width);
                debug(1) << funcs[i].name() << ".vectorize("
                         << v_width << ");\n";
            }
        }

        // To make UREs be consistent with its producer, we vectorize UREs as well
        auto &last_stensor = c.stensors.back();
        if (!c.is_output && last_stensor.v_width.size() > 0) {
            Var last_width = last_stensor.v_width[0];
            fv.ure.vectorize(last_width);
            debug(1) << fv.ure.name() << ".vectorize("
                     << last_width << ");\n";
        }
    }

    void min_depth(Schain &c, vector<Func> &funcs) {
        internal_assert(c.stensors.size() == funcs.size());
        for (size_t i = 0; i < c.stensors.size(); i++) {
            size_t d = c.stensors[i].fifo_depth;
            if (d > 0) {
                funcs[i].min_depth(d);
                debug(1) << funcs[i].name() << ".min_depth("
                         << d << ");\n";
            }
        }
    }

    // Check if the stensors are inclusive cache
    // Namely, for input chain the scope of consumer cannot be beyond its predecessor,
    // for output chain the scope of consumer cannot below its predecessor
    // If not specified, the scope is inherited
    void check_inclusiveness(Schain &c) {
        if (!c.is_output) {
            // start from the outermost loop
            int i = fv.free_vars.size()-1;
            for (auto &s: c.stensors) {
                if (!fv.exists(s.v_scope)) {
                    s.v_scope = fv.free_vars[i];
                    continue;
                }
                int j = fv.var_index(s.v_scope);
                user_assert(j > 0 && j <= i)
                    << "The scope of " << s.name << " is beyond its predecessor\n";
                // Find a loop whose extent is not 1, otherwise this loop would be removed in lowering
                s.v_scope = fv.find_non_unit_var(s.v_scope);
                i = j;
            }
        }
    }

    void find_banks(Schain &c) {
        // The dst_vars includes space loops plus one time loop
        auto dst_vars = fv.ure.function().definition().schedule().transform_params()[0].dst_vars;
        for (auto &s : c.stensors) {
            for (auto &v : s.v_outs) {
                auto p = std::find_if(dst_vars.begin(), dst_vars.end()-1,
                                    [&](string &n){ return v.name() == n; });
                if (p != dst_vars.end()-1) {
                    // Find it is in space loops, so we view it as a bank
                    s.v_banks.push_back(Var(*p));
                } else {
                    // Otherwise view it as bankwidth
                    s.v_width.push_back(Var(*p));
                }
            }
        }
    }

public:
    RealizeOnFPGA(FindVars &_v, FindProducerForOutput &_p)
        : fv(_v), fpo(_p) {}

    Func realize() {
        Func out;
        for (auto &c: schains) {
            check_inclusiveness(c);
            find_banks(c);
            if (!c.is_output) {
                vector<Func> producers;
                producers = isolate_producer(c);
                remove(c, producers);
                scatter(c, producers);
                buffer(c, producers);
                vectorize(c, producers);
                min_depth(c, producers);
            } else {
                vector<Func> consumers;
                consumers = isolate_consumer(c);
                gather(c, consumers);
                vectorize(c, consumers);
                min_depth(c, consumers);
                out = consumers.back();
            }
        }
        internal_assert(out.defined());
        return out;
    }
};

class RealizeOnGPU
{
    FindVars &fv;
    int num_gpu_vars;

    // Check if the stensors are inclusive cache
    // Namely, for input chain the scope of consumer cannot be beyond its predecessor,
    // for output chain the scope of consumer cannot below its predecessor
    // If not specified, the scope is inherited
    void check_inclusiveness(Schain &c) {
        if (!c.is_output) {
            // start from the outermost loop
            int i = fv.free_vars.size()-1;
            for (auto &s: c.stensors) {
                if (!fv.exists(s.v_scope)) {
                    s.v_scope = fv.free_vars[i];
                    continue;
                }
                int j = fv.var_index(s.v_scope);
                user_assert(j > 0 && j <= i)
                    << "The scope of " << s.name << " is beyond its predecessor\n";
                // Find a loop whose extent is not 1, otherwise this loop would be removed in lowering
                s.v_scope = fv.find_non_unit_var(s.v_scope);
                i = j;
            }
        }
    }

    void gpu_fetch(Schain &c) {
        for (auto &s : c.stensors) {
            // Currently, we separately allocate registers in each thread, and view registers
            // throughout threads as an unified SRAM storage, to realize stensors on GPUs.
            if (s.position == SRAM) {
            for (auto &p : c.imp) {
                    int gpu_var_index = fv.free_vars.size() - num_gpu_vars;
                    Var loop = fv.var_index(s.v_scope) < gpu_var_index ? s.v_scope : fv.free_vars[gpu_var_index];
                    p.gpu_fetch(loop, MemoryType::Register, s.v_outs);
                    debug(1) << p.name() << ".gpu_fetch("
                             << loop.name() << ", {" << names_to_string(s.v_outs) << "});\n";
                }
            }
        }
    }

    void gpu_store(Schain &c) {
        for (auto &s : c.stensors) {
            if (s.dims.size() > 0) {
                c.outf.gpu_store(s.dims);
                debug(1) << c.outf.name() << ".gpu_store("
                         << to_string(s.dims) << ");\n";
            }
        }
    }

public:
    RealizeOnGPU(FindVars &_f, int _n)
        : fv(_f), num_gpu_vars(_n) {}

    Func realize() {
        Func out;
        for (auto &c : schains) {
            check_inclusiveness(c);
            if (!c.is_output) {
                gpu_fetch(c);
            } else {
                gpu_store(c);
                out = c.outf;
            }
        }
        return out;
    }
};

Func &operator>>(Func &func, const FIFO &fifo) {
    func.min_depth(fifo.depth);
    debug(1) << func.name() << ".min_depth("
             << fifo.depth << ");\n";
    return func;
}

Stensor &operator>>(Stensor &s, const FIFO &fifo) {
    s.fifo_depth = fifo.depth;
    return s;
}

Func Stensor::stensor_realize_wrapper(Starget t) {
    int c = this->schain_idx;
    user_assert(schains[c].is_output)
        << "Please realize the stensors on the output path\n";
    user_assert(this->position == SMemType::HOST)
        << "Stensors must be realized on the host\n";

    map<string, Func> env;
    Func outf = schains[c].outf;
    env = outf.pipeline().compute_environment();

    Func f;
    if (t == Starget::IntelFPGA) {
        FindVars fv(env);
        FindProducerForOutput fpo(env);
        RealizeOnFPGA fpga(fv, fpo);
        f = fpga.realize();
        internal_assert(f.function().place() == Place::Host);
    }
    if (t == Starget::IntelGPU) {
        int num_gpu_vars = 0;
        for (auto &p : env) {
            if (p.second.function().place() == Place::Device) {
                // Placing on device is only valid for FPGAs
                p.second.function().place(Place::Host);
            }
            reorder_gpu_loops(p.second, num_gpu_vars);
        }
        FindVars fv(env);
        RealizeOnGPU gpu(fv, num_gpu_vars);
        f = gpu.realize();
    }
    return f;
}

void Stensor::realize(Buffer<> dst, Starget t) {
    Func f = stensor_realize_wrapper(t);
    if (t == Starget::IntelFPGA) {
        Target acc = get_host_target();
        acc.set_feature(Target::IntelFPGA);
        acc.set_feature(Target::EnableSynthesis);
        f.realize(dst, acc);
    }
    if (t == Starget::IntelGPU) {
        user_error << "Currently the GPU runtime is under developement\n";
    }
}

void Stensor::compile_jit(Starget t) {
    Func f = stensor_realize_wrapper(t);
    if (t == Starget::IntelFPGA) {
        Target acc = get_host_target();
        acc.set_feature(Target::IntelFPGA);
        acc.set_feature(Target::EnableSynthesis);
        f.compile_jit(acc);
    }
}

void Stensor::compile_to_host(string file_name, const vector<Argument> &args,
                              const std::string fn_name, Starget t) {
    Func f = stensor_realize_wrapper(t);
    if (t == Starget::IntelFPGA) {
        Target acc = get_host_target();
        acc.set_feature(Target::IntelFPGA);
        acc.set_feature(Target::EnableSynthesis);
        f.compile_to_host(file_name, args, fn_name, acc);
    }
    if (t == Starget::IntelGPU) {
        user_warning << "Currently the GPU runtime is under developement, "
                        "so we just emit out the source code in " << fn_name << "_genx.cpp\n";
        Target acc = get_host_target();
        acc.set_feature(Target::IntelGPU);
        f.compile_to_cm(fn_name, std::move(args), acc);
    }
}

}
