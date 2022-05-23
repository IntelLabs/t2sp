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
#include "BuildCallRelation.h"
#include "IRVisitor.h"
#include "../../t2s/src/DebugPrint.h"

namespace Halide{
namespace Internal {

/* Find all the calls to Halide Funcs in a Halide Func */
class FindCallees : public IRVisitor {
public:
    vector<string> callees;
    using IRVisitor::visit;

    void visit(const Call *call) override {
        IRVisitor::visit(call);
        if (call->call_type == Call::Halide && call->func.defined()) {
            string callee = call->name;
            if (std::find(callees.begin(), callees.end(), callee) == callees.end()) {
                callees.push_back(callee);
            }
        }
    }
};

map<string, string> map_func_to_representative(const map<string, Function> &env) {
    map<string, string> func_to_representative;
    for (auto itr : env) {
        const Function &func = itr.second;
        if (func.has_merged_defs()) {
            string representative = func.name();
            func_to_representative[representative] = representative; // Map to itself.
            for (auto m : func.merged_func_names()) {
                user_assert(func_to_representative.find(m) == func_to_representative.end()) <<
                        "Func " << m << " is merged into two different Funcs ("
                        << representative << ", " << func_to_representative.at(m) << "). "
                        << "A Func, if to be merged, can only be merged into one Func.";
                func_to_representative[m] = representative;
            }
        }
    }

    for (auto itr : env) {
        const string &func_name = itr.first;
        if (func_to_representative.find(func_name) == func_to_representative.end()) {
            // The function is not merged with any other function at all.
            // Map it to itself.
            func_to_representative[func_name] = func_name;
        }
    }

    debug(4) << "Funcs to their representatives:\n";
    for (auto entry : func_to_representative) {
        debug(4) << "\t " << entry.first << " --> " << entry.second << "\n";
    }

    return func_to_representative;
}

void print_call_graph(const map<string, vector<string>> &call_graph, const string &head) {
    debug(4) << head << "\n";
    for (auto entry : call_graph) {
        debug(4) << "\t" << entry.first << ": " << to_string<string>(entry.second, true) << "\n";
    }
}

void add_element_without_duplicate(vector<string>& v, const string &element) {
    if (std::find(v.begin(), v.end(), element) == v.end()) {
        v.push_back(element);
    }
}

map<string, vector<string>> modify_call_graph_to_use_representatives(const map<string, vector<string>> &call_graph, const map<string, Function> &env) {
    map<string, vector<string>> merged_call_graph;
    map<string, string> func_to_representative = map_func_to_representative(env);
    for (auto entry : call_graph) {
        const string &f = entry.first;
        const vector<string> &callees = entry.second;
        const string &representative = func_to_representative.at(f);
        if (f == representative) {
            vector<string> callees_rep;
            for (auto c : callees) {
                const string c_rep = func_to_representative.at(c);
                add_element_without_duplicate(callees_rep, c_rep);
            }
            merged_call_graph[f] = std::move(callees_rep);
        }
    }

    // Merge the entries of the funcs with the same representative
    for (auto entry : call_graph) {
        const string &f = entry.first;
        const vector<string> &callees = entry.second;
        const string &representative = func_to_representative.at(f);
        if (f != representative) {
            vector<string> callees_rep = merged_call_graph.at(representative);
            for (auto c : callees) {
                const string c_rep = func_to_representative.at(c);
                add_element_without_duplicate(callees_rep, c_rep);
            }
            merged_call_graph[representative] = std::move(callees_rep);
        }
    }
    print_call_graph(merged_call_graph, "Call graph after merged UREs are treated as a single func (mapping from a func -> funcs that produce input data for it):");

    return merged_call_graph;
}

map<string, vector<string>> remove_self_calls_from_call_graph(const map<string, vector<string>> &call_graph) {
    map<string, vector<string>> new_call_graph;
    for (auto entry : call_graph) {
        const string &f = entry.first;
        const vector<string> &callees = entry.second;
        vector<string> new_callees;
        for (auto c : callees) {
            if (c != f) {
                new_callees.push_back(c);
            }
        }
        new_call_graph[f] = std::move(new_callees);
    }
    print_call_graph(new_call_graph, "Call graph after removing self calls:");

    return new_call_graph;
}

map<string, vector<string>> build_call_graph(const map<string, Function> &env, bool use_representative_for_merged_ures, bool ignore_self_calls) {
    map<string, vector<string>> call_graph;
    for (auto itr : env) {
        Function func = itr.second;
        FindCallees finder;
        func.accept(&finder);
        internal_assert(call_graph.find(func.name()) == call_graph.end());
        call_graph[func.name()] = std::move(finder.callees);
    }
    print_call_graph(call_graph, "Call graph (mapping from a func -> funcs that produce input data for it): ");

    if (use_representative_for_merged_ures) {
        call_graph = modify_call_graph_to_use_representatives(call_graph, env);
    }

    if (ignore_self_calls) {
        call_graph = remove_self_calls_from_call_graph(call_graph);
    }

    return call_graph;
}

map<string, vector<string>> build_reverse_call_graph(const map<string, vector<string>> &call_graph) {
    map<string, vector<string>> reverse_call_graph;
    // Ensure every func has an entry in the reverse call graph.
    for (auto itr : call_graph) {
        const string &func  = itr.first;
        vector<string> empty;
        reverse_call_graph[func] = std::move(empty);
    }

    // Now build the reverse relationship
    for (auto itr : call_graph) {
        const string         &caller  = itr.first;
        const vector<string> &callees = itr.second;
        for (auto callee : callees) {
            reverse_call_graph[callee].push_back(caller);
        }
    }
    debug(4) << "Reverse call graph (mapping from a func -> its consumers): \n";
    for (auto entry : reverse_call_graph) {
        debug(4) << "\t" << entry.first << ": ";
        for (auto c : entry.second) {
            debug(4) << c << " ";
        }
        debug(4) << "\n";
    }
    return reverse_call_graph; //std::move(reverse_call_graph);
}

}
}
