#ifndef T2S_TRIANGULAR_LOOP_OPTIMIZE_H
#define T2S_TRIANGULAR_LOOP_OPTIMIZE_H

/** \file
 *
 * Defines a pass to apply triangular loop optimization to the IR..
 *
 */

#include "../../Halide/src/IR.h"

namespace Halide {
namespace Internal {

extern Stmt flatten_tirangualr_loop_nest(Stmt s, const std::map<std::string, Function> &env);

}
}

#endif
