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
#ifndef T2S_STENSOR_H
#define T2S_STENSOR_H

#include "../../Halide/src/Var.h"
#include "../../Halide/src/ImageParam.h"

namespace Halide {

enum SMemType {
    HOST,
    DRAM,
    SRAM,
    REG
};

enum Starget {
    IntelGPU,
    IntelFPGA
};

struct Stensor
{
    std::string name;
    SMemType position;
    Var v_scope;
    vector<Var> v_width;
    vector<Var> v_banks;
    vector<Var> v_outs;
    vector<Expr> dims;
    int schain_idx = -1;
    int fifo_depth = 0;

    Stensor(std::string _n, SMemType _p)
        : name(_n), position(_p) {}
    Stensor(std::string _n)
        : Stensor(_n, HOST) {}

    Func stensor_realize_wrapper(Starget t);
    void realize(Buffer<> dst, Starget t);
    void compile_jit(Starget t);
    void compile_to_host(string file_name, const vector<Argument> &args,
                         const std::string fn_name, Starget t);
    Stensor &scope(Var v);
    Stensor &banks(const std::vector<Var> &banks);
    Stensor &out(const std::vector<Var> &bankwidth_and_banks);
    Stensor &operator()(const std::vector<Expr> &dims);

    template<typename... Args>
    HALIDE_NO_USER_CODE_INLINE typename std::enable_if<Internal::all_are_convertible<Expr, Args...>::value, Stensor &>::type
    operator()(Expr e, Args &&... args) {
        std::vector<Expr> collected_args{e, std::forward<Args>(args)...};
        return this->operator()(collected_args);
    }
    template<typename... Args>
    HALIDE_NO_USER_CODE_INLINE typename std::enable_if<Internal::all_are_convertible<Var, Args...>::value, Stensor &>::type
    banks(Var v, Args &&... args) {
        std::vector<Var> collected_args{v, std::forward<Args>(args)...};
        return this->banks(collected_args);
    }
    template<typename... Args>
    HALIDE_NO_USER_CODE_INLINE typename std::enable_if<Internal::all_are_convertible<Var, Args...>::value, Stensor &>::type
    out(Var v, Args &&... args) {
        std::vector<Var> collected_args{v, std::forward<Args>(args)...};
        return this->out(collected_args);
    }

    Stensor &operator>>(Stensor &s);
    friend Stensor &operator>>(const ImageParam &im, Stensor &s);
    friend Stensor &operator>>(const vector<ImageParam> &im, Stensor &s);
    friend Stensor &operator>>(Func &f, Stensor &s);
};

struct FIFO
{
    size_t depth;

    FIFO(size_t _d)
        : depth(_d) {}

    friend Func &operator>>(Func &func, const FIFO &fifo);
    friend Stensor &operator>>(Stensor &s, const FIFO &fifo);
};

class URE : public Func
{
public:
    explicit URE(const std::string &name)
        : Func(name, Place::Device) {}
    explicit URE(const std::string &name, Type return_type, const std::vector<Var> &args)
        : Func(name, return_type, args, Place::Device) {}
};

}

#endif
