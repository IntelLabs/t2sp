#ifndef T2S_MEM_FETCH_H
#define T2S_MEM_FETCH_H

namespace Halide {
namespace Internal {

struct Adaptor {
    std::vector<Expr> &loop_vars;
    std::vector<Expr> &loop_mins;
    std::vector<Expr> &loop_extents;
};

Stmt do_prepare_memory_schedule(Stmt s, const std::map<std::string, Function> &env, const Adaptor &stt);
Stmt do_memory_schedule(Stmt s, const std::map<std::string, Function> &env);

} // namespace Internal
} // namespace Halide

#endif
