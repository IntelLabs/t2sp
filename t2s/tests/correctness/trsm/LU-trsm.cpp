#include "util.h"

#define N 6
#define K 3

#define TYPE        float
#define HALIDE_TYPE Float(32)

int main(void) {
    // U01 = L00-1 A01
    ImageParam A01(type_of<TYPE>(), 2);
    ImageParam L00(type_of<TYPE>(), 2);

    // Macros: for convenient use.
    #define X                      j,     i,     k
    #define X_no_k                 j,     i      
    #define X_k_minus_1            j,     i,     k - 1
    #define X_j_minus_1            j - 1, i,     k
    #define X_i_minus_1            j,     i - 1, k
    #define FUNC_DECL              HALIDE_TYPE, {X}, PLACE1

    Var  X;
    Func SL(FUNC_DECL), PrevAi(FUNC_DECL), U01(PLACE1);

    SL(j, i, k) = select(i == 0, A01(j, k), SL(j, i - 1, k) - PrevAi(j, i - 1, k) * L00(j, i - 1));
    PrevAi(j, i, k) = select(j == i, SL(j, i, k), PrevAi(j - 1, i, k));
    U01(i, k) = select(j == i, SL(j, i, k));

    // Merge UREs
    SL.merge_ures(PrevAi, U01)
        .set_bounds(k, 0, N-K, i, 0, K, j, 0, K);
    SL.space_time_transform(j, i);

    TYPE l00[] = {1,0,0, 2,1,0, 16,2,1};
    TYPE a01[] = {1,2,4, 2,2,1, 16,2,4}; 
    TYPE grd[] = {1,2,4, 0,-2,-7, 0,-26,-46};

    Buffer<TYPE> input0(l00, 3, 3);
    Buffer<TYPE> input1(a01, 3, 3);
    Buffer<TYPE> golden(grd, 3, 3);

    // Set inputs dirty to be copied to the device.
    input0.set_host_dirty();
    input1.set_host_dirty();
    L00.set(input0);
    A01.set(input1);

    Target target = get_host_target();
    target.set_feature(Target::IntelFPGA);
    target.set_feature(Target::Debug);
    Buffer<TYPE> results = U01.realize({K, N-K}, target);

    void check(const Buffer<TYPE> &results, const Buffer<TYPE> &input0, const Buffer<TYPE> &input1);
    check(golden, input0, input1);
    cout << "Success!\n";
    return 0;
}

void check(const Buffer<TYPE> &results, const Buffer<TYPE> &L00, const Buffer<TYPE> &A01) {
    printf("*** Input A01:\n");
    for (int j = 0; j < K; j++) {
      for (int i = 0; i < N-K; i++) {
        printf("%5.2f ", A01(i, j));
      }
      printf("\n");
    }

    printf("*** Input L00:\n");
    for (int j = 0; j < K; j++) {
      for (int i = 0; i < K; i++) {
        printf("%5.2f ", L00(i, j));
      }
      printf("\n");
    }

    // Do exactly the same compute in C style
    Buffer<TYPE> PrevAi(N-K,K,K), SL(N-K,K,K), U01(K, N-K);
    for (int k = 0; k < N-K; k++) {
        for (int i = 0; i < K; i++) {
            for (int j = i; j < K; j++) { 
                if (i == 0) {
                    SL(j,i,k) = A01(k,j); // A01[i,k] => A[i,k+K]
                } else {
                    SL(j,i,k) = SL(j,i-1,k) - PrevAi(j,i-1,k) * L00(i-1,j);
                }
                if (i == j) {
                	PrevAi(j,i,k) = SL(j,i,k);
                    U01(k,i) = SL(j,i,k);
                } else {
                	PrevAi(j,i,k) = PrevAi(j-1,i,k);
                }
            }
        }
    }

    // Check if the results are the same as we get from the C style compute
    printf("*** C style result (URE style):\n");
    bool pass = true;
    for (int j = 0; j < K; j++) {
      for (int i = 0; i < N-K; i++) {
            bool correct = (abs(U01(i, j) - results(i, j)) < 1e-2);
            printf("%5.2f (%5.2f%s)", U01(i, j), results(i, j), correct ? "" : " !!");
            pass = pass && correct;
        }
        printf("\n");
    }

    fflush(stdout);
    assert(pass);
}
