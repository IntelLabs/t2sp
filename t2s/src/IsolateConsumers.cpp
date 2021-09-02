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
#include "./DebugPrint.h"

namespace Halide {

using std::max;
using std::min;
using std::map;
using std::string;
using std::vector;
using std::set;
using std::pair;
using std::make_pair;
using std::ofstream;
using std::tuple;

using namespace Internal;

// Check preconditions for isolating the output of Func p to a chains of consumers.
void check_preconditions_of_isolate_consumers(Func p, const vector<Func> &consumers) {
    user_assert(!consumers.empty()) << "Isolating consumer(s): no consumers specified.\n";

    // Func p must have already been defined
    user_assert(p.defined()) << "Isolating the output of an undefined Func " << p.name() << "\n";

    // Func p has only one definition
    user_assert(!p.has_update_definition()) <<  "Func " << p.name() << " is expected to have only 1 definition in order to isolate its output out.\n";

    // The consumers must not be defined yet.
    for (auto c : consumers) {
        user_assert(!c.defined()) << "Isolating the output of Func " << p.name()
                    << " into an already defined Func " << c.name()
                    << ", which is expected to be undefined before the isolation.\n";
    }
}

// Define the consumer Func c based on Func p.
void define_consumer_func(Func p, Func c) {
    vector<Expr> args = p.function().definition().args();
    c(args) = p(args);
}

// Isolate the output of this Func to consumer Func c
Func &Func::isolate_consumer(Func c) {
    check_preconditions_of_isolate_consumers(*this, {c});
    invalidate_cache();

    // Define the new Func c
    // For example,
    //     f(x, y) = whatever
    //     f.isolate_consumer(c)
    // defines Func c as
    //     c(x, y) = f(x, y)
    // Note that the args of c are the same as f. If f has other UREs merged
    // with it, there might be other args due to the other UREs, but in this
    // case, f represents the final results of the UREs, and thus we only use
    // f to create c.
    define_consumer_func(*this, c);

    c.function().definition().schedule().is_output() = func.definition().schedule().is_output();
    c.function().definition().schedule().splits() = func.definition().schedule().splits();
    c.function().definition().schedule().dims() = func.definition().schedule().dims();
    c.function().definition().schedule().transform_params() = func.definition().schedule().transform_params();
    c.function().arg_min_extents() = func.arg_min_extents();
    c.function().isolated_from_as_consumer() = name();

    // This Func will not be inlined into the consumer
    this->compute_root();

    return *this;
}

Func &Func::isolate_consumer_chain(std::vector<Func> &consumers) {
    check_preconditions_of_isolate_consumers(*this, consumers);
    invalidate_cache();

    // If the consumers are, e.g. {c1, c2, c3}, that means a pipeline this Func->c1->c2->c3. So we
    // isolate into c1 first. And then from c1, isolate into c2, and so on.
    for (size_t i = 0; i < consumers.size(); i++) {
        if (i == 0) {
            this->isolate_consumer(consumers[i]);
        } else {
            consumers[i- 1].isolate_consumer(consumers[i]);
        }
        internal_assert(consumers[i].defined()) << "Consumer " << consumers[i].name()
            << " is undefined after isolating consumers from Func " << this->name() << "\n";
    }
    return *this;
}

}
