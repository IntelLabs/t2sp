#ifndef T2S_RELAY_H
#define T2S_RELAY_H

#include "../../Halide/src/IR.h"

namespace Halide {
namespace Internal {

Stmt relay_data(Stmt s, std::map<std::string, Function> &env, const map<string, ShiftRegAlloc> &func_to_regalloc);

}
}

#endif