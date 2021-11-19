#ifndef HALIDE_CODEGEN_ONEAPI_DEV_H
#define HALIDE_CODEGEN_ONEAPI_DEV_H

/** \file
 * Defines the code-generator for producing OpenCL C kernel code
 */

#include <sstream>

#include "CodeGen_C.h"
#include "CodeGen_GPU_Dev.h"
#include "Target.h"
#include "IRMutator.h"
#include "CodeGen_OpenCL_Dev.h"

namespace Halide {
namespace Internal {

class CodeGen_OneAPI_Dev : public CodeGen_OpenCL_Dev {
public:
    CodeGen_OneAPI_Dev(Target target);

    std::string api_unique_name() override {
        return "oneapi";
    }

protected:

private:


};

}  // namespace Internal
}  // namespace Halide

#endif
