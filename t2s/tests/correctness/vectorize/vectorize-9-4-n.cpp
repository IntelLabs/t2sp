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
// A negative test: vectorize GPUBlocks/Threads/Lanes loop.

#include "util.h"

#ifndef I
#define I 32
#endif

int main(void) {
    // Define the compute.
    ImageParam a(Int(32), 1, "a");
    Func A(PLACE0);
    Var i, oi, ii;
    A(i) = a(i);
    A.split(i, oi, ii, 32);

    // Vectorize. Re-compile and run.
#ifdef BLOCKS
    A.gpu_blocks(ii);
#else
    #ifdef THREADS
        A.gpu_threads(ii);
    #else
        A.gpu_lanes(ii);
    #endif
#endif
    try {
        A.vectorize(ii);
    } catch (Halide::CompileError & e) {
        cout << e.what() << "Success!\n";
        exit(0);
    }
    cout << "Failed!\n";
    exit(1);
}


