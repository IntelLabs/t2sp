#ifndef T2S_STENSOR_H
#define T2S_STENSOR_H

#include "../../Halide/src/Var.h"
#include "../../Halide/src/ImageParam.h"

namespace Halide {

enum SMemType {
    NONE,
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
    Var v_banks;
    Var v_width;
    vector<Expr> dims;
    int schain_idx = -1;

    Stensor(std::string _n, SMemType _p)
        : name(_n), position(_p) {}
    Stensor(std::string _n)
        : Stensor(_n, NONE) {}

    void realize(Buffer<> dst, Starget t);
    Stensor &scope(Var v);
    Stensor &banks(Var v);
    Stensor &width(Var v);
    Stensor &width(int n);
    Stensor &operator()(const std::vector<Expr> &dims);

    template<typename... Args>
    HALIDE_NO_USER_CODE_INLINE typename std::enable_if<Internal::all_are_convertible<Expr, Args...>::value, Stensor &>::type
    operator()(Expr e, Args &&... args) {
        std::vector<Expr> collected_args{e, std::forward<Expr>(args)...};
        return this->operator()(collected_args);
    }

    Stensor &operator>>(Stensor &s);
    friend Stensor &operator>>(const ImageParam &im, Stensor &s);
    friend Stensor &operator>>(Func &f, Stensor &s);
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
