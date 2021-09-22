#ifndef T2S_REMOVE_CONDS_H
#define T2S_REMOVE_CONDS_H

#include "../../Halide/src/IR.h"

namespace Halide {
namespace Internal {

Stmt remove_conditions(Stmt s);

}
}


#endif