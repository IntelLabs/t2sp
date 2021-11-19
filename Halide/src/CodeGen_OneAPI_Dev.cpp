#include <algorithm>
#include <sstream>
#include <fstream>

#include "CSE.h"
#include "CodeGen_Internal.h"
#include "CodeGen_OpenCL_Dev.h"
#include "CodeGen_OneAPI_Dev.h"
#include "Debug.h"
#include "EliminateBoolVectors.h"
#include "EmulateFloat16Math.h"
#include "ExprUsesVar.h"
#include "IRMutator.h"
#include "IROperator.h"
#include "Simplify.h"
#include "Substitute.h"
#include "../../t2s/src/DebugPrint.h"
#include "../../t2s/src/Utilities.h"

namespace Halide {
namespace Internal {

using std::ostringstream;
using std::sort;
using std::string;
using std::vector;

CodeGen_OneAPI_Dev::CodeGen_OneAPI_Dev(Target t)
    : CodeGen_OpenCL_Dev(t) {
}

}  // namespace Internal
}  // namespace Halide
