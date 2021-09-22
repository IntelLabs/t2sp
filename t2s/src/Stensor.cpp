#include "Stensor.h"
#include "DebugPrint.h"
#include "Utilities.h"

namespace Halide {

using namespace Internal;
using std::vector;
using std::map;
using std::string;

struct Schain {
    bool is_output;
    Func outf;
    ImageParam imp;
    vector<Stensor> stensors;
};
vector<Schain> schains;

Stensor &Stensor::scope(Var v) {
    v_scope = v;
    return *this;
}

Stensor &Stensor::banks(Var v) {
    v_banks = v;
    return *this;
}

Stensor &Stensor::width(Var v) {
    v_width = v;
    return *this;
}

Stensor &Stensor::width(int n) {
    user_assert(n == 1)
        << "The bankwidth is the innermost var or 1";
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
    return *this;
}

Stensor &operator>>(const ImageParam &im, Stensor &s) {
    Schain tmp;
    s.schain_idx = schains.size();
    tmp.is_output = false;
    tmp.imp = im;
    tmp.stensors.push_back(s);
    schains.push_back(std::move(tmp));
    return s;
}

Stensor &operator>>(Func &f, Stensor &s) {
    Schain tmp;
    s.schain_idx = schains.size();
    tmp.is_output = true;
    tmp.outf = f;
    tmp.stensors.push_back(s);
    schains.push_back(std::move(tmp));
    return s;
}


struct FindVars
{
    Func ure;
    vector<Var> free_vars;

    int var_index(Var v) {
        for (size_t i = 0; i < free_vars.size(); i++) {
            if (v.same_as(free_vars[i]))
                return i;
        }
        return -1;
    }

    bool empty(Var var) {
        for (auto &v : free_vars)
            if (v.same_as(var)) return false;
        return true;
    }

    vector<VarOrRVar> find_non_used_vars(string imp, Var scope) {
        vector<VarOrRVar> non_used_vars;
        auto &used_vars = fuv.used_vars;
        internal_assert(used_vars.count(imp) > 0);

        for (Var v : free_vars) {
            if (v.same_as(scope)) break;
            if (used_vars[imp].count(v.name()) == 0)
                non_used_vars.push_back(Var(v));
        }
        return std::move(non_used_vars);
    }

    class FindUsedVars : public IRVisitor
    {
        string image_param;
    public:
        using IRVisitor::visit;
        map<string, std::set<string>> used_vars;

        void visit(const Variable *op) override {
            if (!image_param.empty())
                used_vars[image_param].insert(op->name);
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
            if (f.function().has_merged_defs()) {
                ure = f;
                for (auto &e : f.function().definition().args()) {
                    auto v = e.as<Variable>();
                    free_vars.push_back(Var(v->name));
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
        if (c.stensors[0].position != NONE) {
            // Automatically generate host stensor
            string host_name = c.imp.name() + "_serializer";
            Stensor s_host(host_name);
            s_host.schain_idx = c.stensors[0].schain_idx;
            c.stensors.insert(c.stensors.begin(), s_host);
        }
        vector<Func> producers;
        for (auto &s : c.stensors) {
            Place place = s.position == SMemType::NONE ? Place::Host : Place::Device;
            Func dev(s.name, place);
            producers.push_back(std::move(dev));
        }
        fv.ure.isolate_producer_chain({c.imp}, producers);
        debug(1) << fv.ure.name() << ".isolate_producer_chain({"
                 << c.imp.name() << "}, " << names_to_string(producers) << ");\n";
        return std::move(producers);
    }

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

    vector<Func> isolate_consumer(Schain &c) {
        vector<Func> consumers;
        // Generate the output array
        std::string dev_name = "drainer";
        Func f_dev(dev_name, Place::Device);
        Stensor s_dev(dev_name);
        s_dev.schain_idx = c.stensors[0].schain_idx;
        c.outf.isolate_consumer(f_dev);
        debug(1) << c.outf.name() << ".isolate_consumer("
                 << f_dev.name() << ");\n";
        generate_output_array(c.outf, f_dev);

        // Isolate other consumers
        for (auto &s : c.stensors) {
            Place place = s.position == SMemType::NONE ? Place::Host : Place::Device;
            Func dev(s.name, place);
            consumers.push_back(std::move(dev));
        }
        f_dev.isolate_consumer_chain(consumers);
        debug(1) << f_dev.name() << ".isolate_consumer_chain("
                 << names_to_string(consumers) << ");\n";
        consumers.insert(consumers.begin(), f_dev);
        c.stensors.insert(c.stensors.begin(), s_dev);
        return consumers;
    }

    void remove(Schain &c, vector<Func> &producers) {
        vector<VarOrRVar> loops;
        Var scope = c.stensors.back().v_scope;

        for (int i = producers.size()-2; i >= 0; i--) {
            loops = fv.find_non_used_vars(c.imp.name(), scope);
            producers[i].remove(loops);
            debug(1) << producers[i].name() << ".remove("
                     << names_to_string(loops) << ");\n";
            scope = (i > 0) ? c.stensors[i].v_scope : scope;
        }
    }

    void scatter(Schain &c, vector<Func> &producers) {
        vector<Var> prev_dims;
        for (size_t i = 0; i < c.stensors.size(); i++) {
            Var v_banks = c.stensors[i].v_banks;
            if (!fv.empty(v_banks) && prev_dims.empty()) {
                Func prev = producers[i-1];
                producers[i].scatter(prev, v_banks);
                debug(1) << producers[i].name() << ".scatter("
                         << prev.name() << ", " << v_banks << ");\n";
                prev_dims = {v_banks};
            }
        }
    }

    void gather(Schain &c, vector<Func> &consumers) {
        auto &prev_dims = output_array_dims;
        for (size_t i = 1; i < c.stensors.size(); i++) {
            Var v_banks = c.stensors[i].v_banks;
            Var gather_dim;
            if (prev_dims.size() == 1 && fv.empty(v_banks)) {
                gather_dim = prev_dims[0];
                prev_dims = {};
            }
            if (prev_dims.size() == 2 && !fv.empty(v_banks)) {
                gather_dim = (prev_dims[0].name() == v_banks.name())
                            ? prev_dims[1] : prev_dims[0];
                prev_dims = {v_banks};
            }
            if (!fv.empty(gather_dim)) {
                Func prev_1 = consumers[i-1];
                Func prev_2 = (i == 1) ? c.outf : consumers[i-2];
                prev_1.gather(prev_2, gather_dim);
                debug(1) << prev_1.name() << ".gather("
                         << prev_2.name() << ", " << gather_dim << ");\n";
            }
        }
    }

    void buffer(Schain &c, vector<Func> &producers) {
        for (size_t i = 0; i < c.stensors.size(); i++) {
            Var v_scope = c.stensors[i].v_scope;
            if (!fv.empty(v_scope) && c.stensors[i].position == SRAM) {
                Func prev = producers[i-1];
                producers[i].buffer(prev, v_scope);
                debug(1) << producers[i].name() << ".buffer("
                         << prev.name() << ", " << v_scope << ");\n";
            }
        }
    }

    void vectorize(Schain &c, vector<Func> &funcs) {
        internal_assert(c.stensors.size() == funcs.size());
        size_t last = -1, sz = c.stensors.size();
        for (size_t i = 0; i < sz; i++) {
            Var dim = c.stensors[i].v_width;
            if (!fv.empty(dim)) {
                funcs[i].vectorize(dim);
                debug(1) << funcs[i].name() << ".vectorize("
                         << dim << ");\n";
                last = i;
            }
        }
        Var dim = c.stensors[last].v_width;
        Func consumer;
        // The PE array is the final consumer
        if (!c.is_output && last == sz-1)
            consumer = fv.ure;
        // The DRAM stensors is the final consumer
        if (c.is_output && last == sz-3)
            consumer = funcs[sz-2];
        // Perform vectorization to keep the consistent between producer and consumer
        if (consumer.defined()) {
            consumer.vectorize(dim);
            debug(1) << consumer.name() << ".vectorize("
                     << dim << ");\n";
        }
    }

    void check_correctness(Schain &c) {
        if (!c.is_output) {
            Var scope("__outermost");
            int i = fv.free_vars.size()-1;
            for (auto &s: c.stensors) {
                if (fv.empty(s.v_scope)) {
                    s.v_scope = scope;
                    continue;
                }
                while (i >= 0 && !fv.free_vars[i--].same_as(s.v_scope));
                user_assert(i >= 0)
                    << "Cannot find scope " << s.v_scope << " in the shrinking order\n";
                scope = s.v_scope;
            }
        } else {
            Var scope(fv.free_vars[0].name());
            size_t i = 0;
            for (auto &s: c.stensors) {
                if (fv.empty(s.v_scope)) {
                    s.v_scope = scope;
                    continue;
                }
                while (i < fv.free_vars.size() && !fv.free_vars[i++].same_as(s.v_scope));
                user_assert(i < fv.free_vars.size())
                    << "Cannot find scope " << s.v_scope << " in the enlarging order\n";
                scope = s.v_scope;
            }
        }
    }

public:
    RealizeOnFPGA(FindVars &_f) : fv(_f) {}

    Func realize() {
        Func out;
        for (auto &c: schains) {
            check_correctness(c);
            if (!c.is_output) {
                vector<Func> producers;
                producers = isolate_producer(c);
                remove(c, producers);
                scatter(c, producers);
                buffer(c, producers);
                vectorize(c, producers);
            } else {
                vector<Func> consumers;
                consumers = isolate_consumer(c);
                gather(c, consumers);
                vectorize(c, consumers);
                out = consumers.back();
            }
        }
        internal_assert(out.defined());
        return out;
    }
};

void Stensor::realize(Buffer<> dst, Starget t) {
    int c = this->schain_idx;
    user_assert(schains[c].is_output)
        << "Please realize the stensors on the output path\n";
    user_assert(this->position == SMemType::NONE)
        << "Stensors must be realized on the host\n";

    map<string, Func> env;
    Func outf = schains[c].outf;
    env = outf.pipeline().compute_environment();

    FindVars fv(env);
    Target acc = get_host_target();
    if (t == Starget::IntelFPGA) {
        RealizeOnFPGA fpga(fv);
        Func f = fpga.realize();
        internal_assert(f.function().place() == Place::Host);

        acc.set_feature(Target::IntelFPGA);
        acc.set_feature(Target::Debug);
        f.realize(dst, acc);
    }
}

}
