#ifndef T2S_CHANNEL_PROMOTION_H
#define T2S_CHANNEL_PROMOTION_H

/** \file
 *
 * Defines a pass to promote channel read/write.
 *
 */

#include "../../Halide/src/IR.h"

namespace Halide {
namespace Internal {

/* Promote channels */
extern Stmt channel_promotion(Stmt s);

}
}

#endif
