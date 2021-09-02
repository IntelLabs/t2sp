#include <algorithm>
#include <set>

#include "FindCalls.h"
#include "Func.h"
#include "IREquality.h"
#include "IRVisitor.h"
#include "RealizationOrder.h"

namespace Halide {
namespace Internal {

using std::map;
using std::pair;
using std::set;
using std::string;
using std::vector;

namespace {

void find_fused_groups_dfs(string current,
                           const map<string, set<string>> &fuse_adjacency_list,
                           set<string> &visited,
                           vector<string> &group) {
    visited.insert(current);
    group.push_back(current);

    map<string, set<string>>::const_iterator iter = fuse_adjacency_list.find(current);
    internal_assert(iter != fuse_adjacency_list.end());

    for (const string &fn : iter->second) {
        if (visited.find(fn) == visited.end()) {
            find_fused_groups_dfs(fn, fuse_adjacency_list, visited, group);
        }
    }
}

pair<map<string, vector<string>>, map<string, string>>
find_fused_groups(const map<string, Function> &env,
                  const map<string, set<string>> &fuse_adjacency_list) {
    set<string> visited;
    map<string, vector<string>> fused_groups;
    map<string, string> group_name;

    for (const auto &iter : env) {
        const string &fn = iter.first;
        if (visited.find(fn) == visited.end()) {
            vector<string> group;
            find_fused_groups_dfs(fn, fuse_adjacency_list, visited, group);

            // Create a unique name for the fused group.
            string rename = unique_name("_fg");
            fused_groups.emplace(rename, group);
            for (const auto &m : group) {
                group_name.emplace(m, rename);
            }
        }
    }
    return {fused_groups, group_name};
}

void realization_order_dfs(string current,
                           const map<string, vector<string>> &graph,
                           set<string> &visited,
                           set<string> &result_set,
                           vector<string> &order) {
    visited.insert(current);

    const auto &iter = graph.find(current);
    internal_assert(iter != graph.end());

    for (const string &fn : iter->second) {
        internal_assert(fn != current);
        if (visited.find(fn) == visited.end()) {
            realization_order_dfs(fn, graph, visited, result_set, order);
        } else {
            internal_assert(result_set.find(fn) != result_set.end())
                << "Stuck in a loop computing a realization order. "
                << "Perhaps this pipeline has a loop involving " << current << "?\n";
        }
    }

    result_set.insert(current);
    order.push_back(current);
}

// Check the validity of a pair of fused stages.
void validate_fused_pair(const string &fn, size_t stage_index,
                         const map<string, Function> &env,
                         const map<string, map<string, Function>> &indirect_calls,
                         const FusedPair &p,
                         const vector<FusedPair> &func_fused_pairs) {
    internal_assert((p.func_1 == fn) && (p.stage_1 == stage_index));

    user_assert(env.count(p.func_2))
        << "Illegal compute_with: \"" << p.func_2 << "\" is scheduled to be computed with \""
        << p.func_1 << "\" but \"" << p.func_2 << "\" is not used anywhere.\n";

    // Assert no compute_with of updates of the same Func and no duplicates
    // (These technically should not have been possible from the front-end).
    {
        internal_assert(p.func_1 != p.func_2);
        const auto &iter = std::find(func_fused_pairs.begin(), func_fused_pairs.end(), p);
        internal_assert(iter == func_fused_pairs.end())
            << "Found duplicates of fused pair (" << p.func_1 << ".s" << p.stage_1 << ", "
            << p.func_2 << ".s" << p.stage_2 << ", " << p.var_name << ")\n";
    }

    // Assert no dependencies among the functions that are computed_with.
    const auto &callees_1 = indirect_calls.find(p.func_1);
    if (callees_1 != indirect_calls.end()) {
        user_assert(callees_1->second.find(p.func_2) == callees_1->second.end())
            << "Invalid compute_with: there is dependency between "
            << p.func_1 << " and " << p.func_2 << "\n";
    }
    const auto &callees_2 = indirect_calls.find(p.func_2);
    if (callees_2 != indirect_calls.end()) {
        user_assert(callees_2->second.find(p.func_1) == callees_2->second.end())
            << "Invalid compute_with: there is dependency between "
            << p.func_1 << " and " << p.func_2 << "\n";
    }
}

// Populate 'func_fused_pairs' and 'fuse_adjacency_list': a directed and
// non-directed graph representing the compute_with dependencies between
// functions.
void collect_fused_pairs(const FusedPair &p,
                         vector<FusedPair> &func_fused_pairs,
                         map<string, vector<string>> &graph,
                         map<string, set<string>> &fuse_adjacency_list) {
    fuse_adjacency_list[p.func_1].insert(p.func_2);
    fuse_adjacency_list[p.func_2].insert(p.func_1);

    func_fused_pairs.push_back(p);

    // If there is a compute_with dependency between two functions, we need
    // to update the pipeline DAG so that the computed realization order
    // respects this dependency.
    graph[p.func_1].push_back(p.func_2);
}

// Populate the 'fused_pairs' list in Schedule of each function stage.
void populate_fused_pairs_list(const string &func, const Definition &def,
                               size_t stage_index, map<string, Function> &env) {
    internal_assert(def.defined());
    const LoopLevel &fuse_level = def.schedule().fuse_level().level;
    if (fuse_level.is_inlined() || fuse_level.is_root()) {
        // 'func' is not fused with anyone.
        return;
    }

    auto iter = env.find(fuse_level.func());
    user_assert(iter != env.end())
        << "Illegal compute_with: \"" << func << "\" is scheduled to be computed with \""
        << fuse_level.func() << "\", which is not used anywhere.\n";

    Function &parent = iter->second;
    user_assert(!parent.has_extern_definition())
        << "Illegal compute_with: Func \"" << func << "\" is scheduled to be "
        << "computed with extern Func \"" << parent.name() << "\"\n";

    FusedPair pair(fuse_level.func(), fuse_level.stage_index(),
                   func, stage_index, fuse_level.var().name());
    if (fuse_level.stage_index() == 0) {
        parent.definition().schedule().fused_pairs().push_back(pair);
    } else {
        internal_assert(fuse_level.stage_index() > 0);
        parent.update(fuse_level.stage_index() - 1).schedule().fused_pairs().push_back(pair);
    }
}

// Make sure we don't have cyclic compute_with: if Func 'f' is computed after
// Func 'g', Func 'g' should not be computed after Func 'f'.
void check_no_cyclic_compute_with(const map<string, vector<FusedPair>> &fused_pairs_graph) {
    for (const auto &iter : fused_pairs_graph) {
        for (const auto &pair : iter.second) {
            internal_assert(pair.func_1 != pair.func_2);
            const auto &o_iter = fused_pairs_graph.find(pair.func_2);
            if (o_iter == fused_pairs_graph.end()) {
                continue;
            }
            const auto &it = std::find_if(o_iter->second.begin(), o_iter->second.end(),
                                          [&pair](const FusedPair &other) {
                                              return (pair.func_1 == other.func_2) && (pair.func_2 == other.func_1);
                                          });
            user_assert(it == o_iter->second.end())
                << "Found cyclic dependencies between compute_with of "
                << pair.func_1 << " and " << pair.func_2 << "\n";
        }
    }
}

}  // anonymous namespace

vector<Func> find_all_merged_ures(string func_name, const map<string, vector<string>> &merge_relation,
                                  map<string, Function> &env, set<string> &visitied) {
    internal_assert(env.find(func_name) != env.end());
    user_assert(visitied.find(func_name) == visitied.end()) << "The merge relationship can contain loop\n";
    Function &func = env.at(func_name);
    visitied.insert(func_name);

    vector<Func> total_ures;
    for (Func& merged_func : func.definition().schedule().merged_ures()) {
        total_ures.push_back(merged_func);
        if (merge_relation.find(merged_func.name()) != merge_relation.end()) {
            vector<Func> this_total_ures = find_all_merged_ures(merged_func.name(), merge_relation, env, visitied);
            total_ures.insert(total_ures.end(), this_total_ures.begin(), this_total_ures.end());
        }
    }
    return total_ures;
}

void clear_out_merged_ures(map<string, Function> &env) {
    map<string, vector<string>> merge_relation;
    map<string, string> reverse_merge_relation;
    for (auto element : env) {
        string func_name = element.first;
        const Function &func = element.second;
        if (func.has_merged_defs()) {
            vector<string> merged_func_names = func.merged_func_names();
            for (string name : merged_func_names) {
                user_assert(env.find(name) != env.end()) << "The merged URE " << name
                        << " is not used in the final output.";
                user_assert(reverse_merge_relation.find(name) == reverse_merge_relation.end())
                    << "Func " << name << " is merged more than once.\n";
                reverse_merge_relation[name] = func_name;
                merge_relation[func_name] = merged_func_names;
            }
        }
    }

    for (auto element : env) {
        string func_name = element.first;
        Function &func = element.second;
        if (func.has_merged_defs() &&
            reverse_merge_relation.find(func_name) == reverse_merge_relation.end()) {
            // This is a control URE
            set<string> visitied;
            vector<Func> &merged_ures = func.definition().schedule().merged_ures();
            vector<Func> all_merged_ures = find_all_merged_ures(func_name, merge_relation, env, visitied);
            merged_ures.clear();
            for (Func& ure : all_merged_ures) {
                merged_ures.push_back(ure);
                ure.function().definition().schedule().merged_ures().clear();
                env.at(ure.name()).definition().schedule().merged_ures().clear();
            }

            internal_assert(env[func_name].has_merged_defs());
        }
    }
}

map<string, vector<string>> find_and_insert_merge_group(map<string, vector<string>> &graph,
    map<string, string> &group_name, const map<string, Function> &env) {
    map<string, vector<string>> merged_order;
    map<string, string> delete_node_to_merge_group;
    for (auto element : env) {
        const Function &func = element.second;
        if (func.has_merged_defs()) {
            vector<string> merged_func_names = func.merged_func_names();
            string group;
            set<string> deleted_node;
            vector<string> output_func_names;
            for (auto func_name : merged_func_names) {
                if (env.find(func_name) != env.end()) {
                    const Function &f = env.at(func_name);
                    if (f.definition().schedule().is_output()) {
                        if (group == "") {
                            group = group_name[func_name];
                        } else {
                            user_assert(group == group_name[func_name]);
                        }
                        deleted_node.insert(func_name);
                        output_func_names.push_back(func_name);
                    }
                }
                
            }
            for (auto func_name : output_func_names) {
                merged_func_names.erase(find(merged_func_names.begin(), merged_func_names.end(), func_name));
            }
            
            merged_func_names.insert(merged_func_names.begin(), element.first);
            
            for (string merged_name : merged_func_names) {
                string group2 = group_name[merged_name];
                user_assert(group == group2) << "Func " << merged_name << " and output Func are merged together, but have different compute_with group\n";
                graph.erase(merged_name);
                deleted_node.insert(merged_name);
            }

            // vector<string> new_func_links;
            // for (string name : graph[pair.first]) {
            //     if (deleted_node.find(name) == deleted_node.end()) {
            //         new_func_links.push_back(name);
            //     }
            // }
            // graph[pair.first] = new_func_links;

            vector<string> new_group_links;
            for (string name : graph[group]) {
                if (deleted_node.find(name) == deleted_node.end()) {
                    new_group_links.push_back(name);
                }
            }
            graph[group] = new_group_links;

            string merged_group_name = unique_name("_mg");
            graph[merged_group_name] = {group};
            group_name[merged_group_name] = group;
            for (auto name : output_func_names) {
                graph[name] = {group, merged_group_name};
            }

            merged_order[merged_group_name] = merged_func_names;

            for (string node : merged_func_names) {
                delete_node_to_merge_group[node] = merged_group_name;
            }
        }
    }

    for (auto element : graph) {
        if (delete_node_to_merge_group.empty())
            break;
        vector<string> new_links;
        for (string link : element.second) {
            set<string> link_to_group;
            if (delete_node_to_merge_group.find(link) == delete_node_to_merge_group.end()) {
                new_links.push_back(link);
            } else {
                string merge_group = delete_node_to_merge_group[link];
                if (link_to_group.find(merge_group) == link_to_group.end()) {
                    new_links.push_back(merge_group);
                    link_to_group.insert(merge_group);
                }
            }
        }
        graph[element.first] = new_links;
    }
    return merged_order;
}

pair<vector<string>, vector<vector<string>>> realization_order(
    const vector<Function> &outputs, map<string, Function> &env) {

    // Populate the fused_pairs list of each function definition (i.e. list of
    // all function definitions that are to be computed with that function).
    for (auto &iter : env) {
        if (iter.second.has_extern_definition()) {
            // Extern function should not be fused.
            continue;
        }
        populate_fused_pairs_list(iter.first, iter.second.definition(), 0, env);
        for (size_t i = 0; i < iter.second.updates().size(); ++i) {
            populate_fused_pairs_list(iter.first, iter.second.updates()[i], i + 1, env);
        }
    }

    // Collect all indirect calls made by all the functions in "env".
    map<string, map<string, Function>> indirect_calls;
    for (const pair<string, Function> &caller : env) {
        map<string, Function> more_funcs = find_transitive_calls(caller.second);
        indirect_calls.emplace(caller.first, more_funcs);
    }

    // 'graph' is a DAG representing the pipeline. Each function maps to the
    // set describing its inputs.
    map<string, vector<string>> graph;

    // Make a directed and non-directed graph representing the compute_with
    // dependencies between functions. Each function maps to the list of
    // functions computed_with it.
    map<string, vector<FusedPair>> fused_pairs_graph;
    map<string, set<string>> fuse_adjacency_list;

    for (const pair<string, Function> &caller : env) {
        // Find all compute_with (fused) pairs. We have to look at the update
        // definitions as well since compute_with is defined per definition (stage).
        vector<FusedPair> &func_fused_pairs = fused_pairs_graph[caller.first];
        fuse_adjacency_list[caller.first];  // Make sure every Func in 'env' is allocated a slot
        if (!caller.second.has_extern_definition()) {
            for (auto &p : caller.second.definition().schedule().fused_pairs()) {
                //validate_fused_pair(caller.first, 0, env, indirect_calls,
                //                    p, func_fused_pairs);
                collect_fused_pairs(p, func_fused_pairs, graph, fuse_adjacency_list);
            }
            for (size_t i = 0; i < caller.second.updates().size(); ++i) {
                for (auto &p : caller.second.updates()[i].schedule().fused_pairs()) {
                    //validate_fused_pair(caller.first, i + 1, env, indirect_calls,
                    //                    p, func_fused_pairs);
                    collect_fused_pairs(p, func_fused_pairs, graph, fuse_adjacency_list);
                }
            }
        }
    }

    check_no_cyclic_compute_with(fused_pairs_graph);

    // Determine groups of functions which loops are to be fused together.
    // 'fused_groups' maps a fused group to its members.
    // 'group_name' maps a function to the name of the fused group it belongs to.
    map<string, vector<string>> fused_groups;
    map<string, string> group_name;
    std::tie(fused_groups, group_name) = find_fused_groups(env, fuse_adjacency_list);

    // Compute the DAG representing the pipeline
    for (const pair<string, Function> &caller : env) {
        const string &caller_rename = group_name.at(caller.first);
        // Create a dummy node representing the fused group and add input edge
        // dependencies from the nodes representing member of the fused group
        // to this dummy node.
        graph[caller.first].push_back(caller_rename);
        // Direct the calls to calls from the dummy node. This forces all the
        // functions called by members of the fused group to be realized first.
        vector<string> &s = graph[caller_rename];
        for (const pair<string, Function> &callee : find_direct_calls(caller.second)) {
            if ((callee.first != caller.first) &&  // Skip calls to itself (i.e. update stages)
                (std::find(s.begin(), s.end(), callee.first) == s.end())) {
                s.push_back(callee.first);
            }
        }
    }

    // To get the right realization order of merged UREs.
    clear_out_merged_ures(env);
    map<string, vector<string>> merged_order = 
        find_and_insert_merge_group(graph, group_name, env);

    // Compute the realization order of the fused groups (i.e. the dummy nodes)
    // and also the realization order of the functions within a fused group.
    vector<string> temp;
    set<string> result_set;
    set<string> visited;
    for (Function f : outputs) {
        if (visited.find(f.name()) == visited.end()) {
            realization_order_dfs(f.name(), graph, visited, result_set, temp);
        }
    }

    vector<string> before_merge = temp;
    temp.clear();
    for (string name : before_merge) {
        if (merged_order.find(name) != merged_order.end()) {
            for (string merged_name: merged_order[name]) {
                temp.push_back(merged_name);
            }
        } else {
            temp.push_back(name);
        }
    }

    // Collect the realization order of the fused groups.
    vector<vector<string>> group_order;
    for (const auto &fn : temp) {
        const auto &iter = fused_groups.find(fn);
        if (iter != fused_groups.end()) {
            group_order.push_back(iter->second);
        }
    }
    // Sort the functions within a fused group based on the compute_with
    // dependencies (i.e. parent of the fused loop should be realized after its
    // children).
    for (auto &group : group_order) {
        std::sort(group.begin(), group.end(),
                  [&](const string &lhs, const string &rhs) {
                      const auto &iter_lhs = std::find(temp.begin(), temp.end(), lhs);
                      const auto &iter_rhs = std::find(temp.begin(), temp.end(), rhs);
                      return iter_lhs < iter_rhs;
                  });
    }

    // Collect the realization order of all functions within the pipeline.
    vector<string> order;
    for (const auto &group : group_order) {
        for (const auto &f : group) {
            order.push_back(f);
        }
    }

    return {order, group_order};
}

vector<string> topological_order(const vector<Function> &outputs,
                                 const map<string, Function> &env) {

    // Make a DAG representing the pipeline. Each function maps to the
    // set describing its inputs.
    map<string, vector<string>> graph;

    for (const pair<string, Function> &caller : env) {
        vector<string> s;
        for (const pair<string, Function> &callee : find_direct_calls(caller.second)) {
            if ((callee.first != caller.first) &&  // Skip calls to itself (i.e. update stages)
                (std::find(s.begin(), s.end(), callee.first) == s.end())) {
                s.push_back(callee.first);
            }
        }
        graph.emplace(caller.first, s);
    }

    vector<string> order;
    set<string> result_set;
    set<string> visited;
    for (Function f : outputs) {
        if (visited.find(f.name()) == visited.end()) {
            realization_order_dfs(f.name(), graph, visited, result_set, order);
        }
    }

    return order;
}

}  // namespace Internal
}  // namespace Halide
