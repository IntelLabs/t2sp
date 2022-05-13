// auto-regressive-filter-testing-ures-in-t2s.cpp

// The only header file to include
#include "Halide.h"
using namespace Halide;

int main()
{
    const int T = 10;
    const int I = 3;
    const int c = 2;
    
    /**** Build a dataflow graph for computing the auto regressive filter. ****/

    // Inputs are 2-dimensional arrarys.
    ImageParam phi(Int(32), 2), epsilon(Int(32), 2);

    // UREs
    Var  i("i"), t("t");
    Func Phi("Phi",Int(32),{i,t}), Chi("Chi",Int(32),{i,t}), X("X", Int(32), {i,t});
    Phi(i, t) = select(t-1 == 0, phi(0, i), Phi(i, t-1));
    Chi(i, t) = select(t-1 == 0, 0, select(i-1==0, X(i-(1-I), t-1), Chi(i-1, t-1)));
    X(i, t)   = (select(i-1 == 0, c+epsilon(0,t), X(i-1,t))) + Phi(i,t) * Chi(i, t);

    // Put all the UREs inside the same loop nest, and explicitly set the loop bounds.
    Phi.merge_ures(Chi, X)
       .set_bounds(i, 1, I, t, 1, T);

    /**** Instantiate the dataflow graph ****/

    // Set inputs and run the dataflow graph on a CPU to compute the output.
    Buffer<int> _phi(1, I+1), _epsilon(1, T+1), _X(I+1, T+1);
    for (int i = 1; i <= I; i++) { _phi(0, i) = rand(); }
    for (int t = 1; t <= T; t++) { _epsilon(0, t) = rand(); }
    phi.set(_phi);
    epsilon.set(_epsilon);
    _X = X.realize(get_host_target()/*Get a CPU*/);

    /**** Validate correctness ****/
    
    Buffer<int> golden(T+1);
    for (int t= 1; t <= T; t++) {
      golden(t) = c + _epsilon(0, t);
      for (int i = 1; i <= I; i++) {
        if (t - i > 0) {
           golden(t) += _phi(0, i) * golden(t - i);
        }
      }
      assert(golden(t) == _X(I, t));
    }
    printf("Success!\n");
    return 0;
}
