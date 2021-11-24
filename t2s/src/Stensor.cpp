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
    ImageParam imp;                 // The input chain starts from external input
    vector<Stensor> stensors;
};
vector<Schain> schains;

Stensor &Stensor::scope(Var v) {
    v_scope = v;
    return *this;
}

Stensor &Stensor::banks(const vector<Var> &v) {
    v_banks = v;
    return *this;
}

Stensor &Stensor::out(const vector<Var> &v) {
    if (v.empty()) {
        // By default, this stensor will output a scalar each time.
        return *this;
    }
    // Format of the vector: bankwidth, then banks
    v_width = v[0];
    if (v.size() > 1) {
        vector<Var> banks = {v.begin() + 1, v.end()};
        v_banks = banks;
    }
    return *this;
}

Stensor &Stensor::operator()(const vector<Expr> &d) {
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

Stensor &operator>>(const ImageParam &im, Stensor &s) {
    Schain tmp;
    s.schain_idx = schains.size();
    tmp.is_output = false;
    tmp.imp = im;
    tmp.stensors.push_back(s);
    schains.push_back(std::move(tmp));
    return schains.back().stensors.back();
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
                    // Skip loops whose extent == 1
                    auto bound = f.function().get_bounds(d.var);
                    if (!is_one(bound.second)) {
                        free_vars.push_back(d.var);
                    }
                }
            }
        }
        user_assert(ure.defined())
            << "Cannot find merged UREs. Do you forget to apply merge_ures?";
    }
};

class RealizeOnFPGA
{
    vector<Var> output_array_dims;
    FindVars &fv;

    vector<Func> isolate_producer(Schain &c) {
        if (c.stensors[0].position != HOST) {
            // The device stensors needs serialized inputs
            // If the host stensor is not specified, we automatically generate it
            string host_name = c.imp.name() + "_serializer";
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
        fv.ure.isolate_producer_chain({c.imp}, producers);
        debug(1) << fv.ure.name() << ".isolate_producer_chain({"
                 << c.imp.name() << "}, " << names_to_string(producers) << ");\n";
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
        // Isolate subsequent consumers
        for (auto &s : c.stensors) {
            Place place = s.position == SMemType::HOST ? Place::Host : Place::Device;
            Func f_dev(s.name, place);
            consumers.push_back(std::move(f_dev));
        }
        if (c.stensors.back().position != HOST) {
            // If the host stensor is not specified, we automatically generate it
            string host_name = c.outf.name() + "_deserializer";
            Stensor s_host(host_name);
            s_host.schain_idx = c.stensors[0].schain_idx;
            c.stensors.push_back(s_host);
        }

        if (c.stensors[0].position == REG && !c.stensors[0].v_banks.empty()) {
            // The output stensor inherits loops of the output URE, generally less than that of systolic array
            // So we isolate the first consumer alone and apply space-time transform to regenerate loop structure,
            // then the subsequent stensors could be isolated based on that
            Func regf = consumers[0];
            c.outf.isolate_consumer(regf);
            debug(1) << c.outf.name() << ".isolate_consumer("
                     << regf.name() << ");\n";
            // generate_output_array(outf, f_dev);
            regf.space_time_transform(c.stensors[0].v_banks);
            debug(1) << regf.name() << ".space_time_transform("
                     << names_to_string(c.stensors[0].v_banks) << ");\n";
            vector<Func> other_cons(consumers.begin()+1, consumers.end());
            regf.isolate_consumer_chain(other_cons);
            debug(1) << regf.name() << ".isolate_consumer_chain("
                     << names_to_string(other_cons);
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
            loops = fv.find_reuse_vars(c.imp.name(), scope);
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
            Var v_width = c.stensors[i].v_width;
            if (fv.exists(v_width)) {
                funcs[i].vectorize(v_width);
                debug(1) << funcs[i].name() << ".vectorize("
                         << v_width << ");\n";
            }
        }

        // To make UREs be consistent with its producer, we vectorize UREs as well
        Var last_width = c.stensors.back().v_width;
        if (!c.is_output && fv.exists(last_width)) {
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
            Var scope("__outermost");
            int i = fv.free_vars.size();
            for (auto &s: c.stensors) {
                if (!fv.exists(s.v_scope)) {
                    s.v_scope = scope;
                    continue;
                }
                int j = fv.var_index(s.v_scope);
                user_assert(j > 0 && j <= i)
                    << "The scope of " << s.name << " is beyond its predecessor\n";
                scope = s.v_scope;
            }
        } else {
            Var scope(fv.free_vars[0].name());
            int i = 0;
            for (auto &s: c.stensors) {
                if (!fv.exists(s.v_scope)) {
                    s.v_scope = scope;
                    continue;
                }
                int j = fv.var_index(s.v_scope);
                user_assert(j > 0 && j >= i)
                    << "The scope of " << s.name << " is below its predecessor\n";
                scope = s.v_scope;
            }
        }
    }

public:
    RealizeOnFPGA(FindVars &_f) : fv(_f) {}

    Func realize() {
        Func out;
        for (auto &c: schains) {
            check_inclusiveness(c);
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

    void mem_fetch(Schain &c) {
        for (auto &s : c.stensors) {
            // The SRAM stensor determines the allocated registers
            if (s.position == SRAM) {
                // Find a loop whose extent > 1
                int idx = fv.var_index(s.v_banks.back());
                Var dim = fv.free_vars[idx + 1];
                c.imp.mem_fetch(dim, MemoryType::Register);
                debug(1) << c.imp.name() << ".mem_fetch("
                         << dim << ");\n";
            }
        }
    }

    void mem_store(Schain &c) {
        for (auto &s : c.stensors) {
            if (s.dims.size() > 0) {
                c.outf.mem_store(s.dims);
                debug(1) << c.outf.name() << ".mem_store("
                         << to_string(s.dims) << ");\n";
            }
        }
    }

public:
    RealizeOnGPU(FindVars &_f) : fv(_f) {}

    Func realize() {
        Func out;
        for (auto &c : schains) {
            // check_correctness(c);
            if (!c.is_output) {
                mem_fetch(c);
            } else {
                mem_store(c);
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
        RealizeOnFPGA fpga(fv);
        f = fpga.realize();
        internal_assert(f.function().place() == Place::Host);
    }
    if (t == Starget::IntelGPU) {
        for (auto &p : env) {
            if (p.second.function().place() == Place::Device) {
                // Placing on device is only valid for FPGAs
                p.second.function().place(Place::Host);
            }
            reorder_gpu_loops(p.second);
        }
        FindVars fv(env);
        RealizeOnGPU gpu(fv);
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
