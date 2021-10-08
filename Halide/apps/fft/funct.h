#ifndef FUNCT_H
#define FUNCT_H

#include <string>
#include <vector>

#include "Halide.h"
#include "../../../../Halide/src/Func.h"

struct ComplexExpr;
template<typename T>
class FuncRefT : public T {
    Halide::FuncRef untyped;

public:
    typedef Halide::Stage Stage;
    typedef Halide::Tuple Tuple;

    FuncRefT(const Halide::FuncRef &untyped)
        : T(untyped.function().has_pure_definition() ? T(Tuple(untyped)) : T()),
          untyped(untyped) {
    }

    Stage operator=(T x) {
        return untyped = x;
    }
    Stage operator+=(T x) {
        return untyped = T(Tuple(untyped)) + x;
    }
    Stage operator-=(T x) {
        return untyped = T(Tuple(untyped)) - x;
    }
    Stage operator*=(T x) {
        return untyped = T(Tuple(untyped)) * x;
    }
    Stage operator/=(T x) {
        return untyped = T(Tuple(untyped)) / x;
    }
    T toT(){
        if(typeid(ComplexExpr)==typeid(T)){
            return ComplexExpr(Halide::Call::make(untyped.function(), untyped.arguments(), 0), Halide::Call::make(untyped.function(), untyped.arguments(), 1));
        }
    }
};


template<typename T>
class FuncT : public Halide::Func {
public:
    typedef Halide::Var Var;
    typedef Halide::Expr Expr;
    typedef Halide::Func Func;
    typedef Halide::Place Place;
    typedef Halide::Type Type;
    typedef Halide::FuncRef FuncRef;

    explicit FuncT(const std::string &name)
        : Func(name) {
    }
    FuncT() {
    }
    explicit FuncT(Expr e)
        : Func(e) {
    }
    explicit FuncT(Func f)
        : Func(f) {
    }

    explicit FuncT(Place place)
        : Func(place) {
    }
    explicit FuncT(const std::string &name, Place place)
        : Func(name, place) {
    }

    explicit FuncT(Halide::Internal::Function f)
        : Func(f) {
    }

    explicit FuncT(const std::vector<Type> &return_types, const std::vector<Var> &args, Place place)
        : Func(return_types, args, place) {
    }

    explicit FuncT(const std::string &name, const std::vector<Type> &return_types, const std::vector<Var> &args, Place place)
        : Func(name, return_types, args, place) {
    }

    template<typename... Args>
    FuncRefT<T> operator()(Args &&... args) const {
        return Func::operator()(std::forward<Args>(args)...);
    }

    FuncRefT<T> operator()(std::vector<Expr> vars) const {
        return Func::operator()(vars);
    }
    FuncRefT<T> operator()(std::vector<Var> vars) const {
        return Func::operator()(vars);
    }
};

// Forward operator overload invocations on FuncRefT to
// the type the user intended (T).

// TODO(dsharlet): This is obscene. Find a better way... but it is unlikely
// there is one.
template<typename T>
T operator-(FuncRefT<T> x) {
    return -x.toT();
}
template<typename T>
T operator~(FuncRefT<T> x) {
    return ~static_cast<T>(x);
}

template<typename T>
T operator+(FuncRefT<T> a, T b) {
    return a.toT()+b;
}
template<typename T>
T operator-(FuncRefT<T> a, T b) {
    return a.toT()-b;
}
template<typename T>
T operator*(FuncRefT<T> a, T b) {
    return a.toT()*b;
}
template<typename T>
T operator/(FuncRefT<T> a, T b) {
    return a.toT()/b;
}
template<typename T>
T operator%(FuncRefT<T> a, T b) {
    return a.toT()%b;
}
template<typename T>
T operator+(T a, FuncRefT<T> b) {
    return a+b.toT();
}
template<typename T>
T operator-(T a, FuncRefT<T> b) {
    return a-b.toT();
}
template<typename T>
T operator*(T a, FuncRefT<T> b) {
    return a*b.toT();
}
template<typename T>
T operator/(T a, FuncRefT<T> b) {
    return a/b.toT();
}
template<typename T>
T operator%(T a, FuncRefT<T> b) {
    return a%b.toT();
}

template<typename T>
T operator+(FuncRefT<T> a, FuncRefT<T> b) {
    return a.toT()+b.toT();
}
template<typename T>
T operator-(FuncRefT<T> a, FuncRefT<T> b) {
    return a.toT()-b.toT();
}
template<typename T>
T operator*(FuncRefT<T> a, FuncRefT<T> b) {
    return a.toT()*b.toT();
}
template<typename T>
T operator/(FuncRefT<T> a, FuncRefT<T> b) {
    return a.toT()/b.toT();
}
template<typename T>
T operator%(FuncRefT<T> a, FuncRefT<T> b) {
    return a.toT()%b.toT();
}

template<typename T>
Halide::Expr operator==(FuncRefT<T> a, T b) {
    return static_cast<T>(a) == b;
}
template<typename T>
Halide::Expr operator!=(FuncRefT<T> a, T b) {
    return static_cast<T>(a) != b;
}
template<typename T>
Halide::Expr operator<=(FuncRefT<T> a, T b) {
    return static_cast<T>(a) <= b;
}
template<typename T>
Halide::Expr operator>=(FuncRefT<T> a, T b) {
    return static_cast<T>(a) >= b;
}
template<typename T>
Halide::Expr operator<(FuncRefT<T> a, T b) {
    return static_cast<T>(a) < b;
}
template<typename T>
Halide::Expr operator>(FuncRefT<T> a, T b) {
    return static_cast<T>(a) > b;
}
template<typename T>
Halide::Expr operator==(T a, FuncRefT<T> b) {
    return a == static_cast<T>(b);
}
template<typename T>
Halide::Expr operator!=(T a, FuncRefT<T> b) {
    return a != static_cast<T>(b);
}
template<typename T>
Halide::Expr operator<=(T a, FuncRefT<T> b) {
    return a <= static_cast<T>(b);
}
template<typename T>
Halide::Expr operator>=(T a, FuncRefT<T> b) {
    return a >= static_cast<T>(b);
}
template<typename T>
Halide::Expr operator<(T a, FuncRefT<T> b) {
    return a < static_cast<T>(b);
}
template<typename T>
Halide::Expr operator>(T a, FuncRefT<T> b) {
    return a > static_cast<T>(b);
}

#endif
