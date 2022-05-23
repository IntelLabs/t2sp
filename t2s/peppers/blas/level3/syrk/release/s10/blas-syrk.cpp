#include "syrk-interface.h"
#include "blas-syrk.h"
#include "const-parameters.h"
#include "HalideBuffer.h"

#define MATRIX_OP_IDENTITY 0
#define MATRIX_OP_TRANSPOSE 1

using namespace std;

// current version ignores n, k, lda, ldc
// assumes n = 768, k = 1024, lda = (trans == 'N' || trans == 'n') ? 768 : 1024, ldc = 768
void ssyrk(char uplo, char trans, int n, int k, float alpha, float *a, int lda, float beta, float *c, int ldc) {
    int opa = (trans == 'N' || trans == 'n') ? MATRIX_OP_IDENTITY : MATRIX_OP_TRANSPOSE;

    Halide::Runtime::Buffer<float> bufferA(k, n), bufferC(n, n);
    for (size_t iterI = 0; iterI < n; iterI++)
        for (size_t iterK = 0; iterK < k; iterK++)
            bufferA(iterK, iterI) = a[iterI * k + iterK];
    
    if (uplo == 'U' || uplo == 'u') {
        for (size_t iterI = 0; iterI < n; iterI++)
            for (size_t iterJ = 0; iterJ < n; iterJ++) {
                if (iterI <= iterJ) {
                    bufferC(iterJ, iterI) = c[iterI * n + iterJ];
                } else {
                    bufferC(iterJ, iterI) = c[iterJ * n + iterI];
                }
            }
    } else {
        for (size_t iterI = 0; iterI < n; iterI++)
            for (size_t iterJ = 0; iterJ < n; iterJ++) {
                if (iterI >= iterJ) {
                    bufferC(iterJ, iterI) = c[iterI * n + iterJ];
                } else {
                    bufferC(iterJ, iterI) = c[iterJ * n + iterI];
                }
            }
    }
    
    Halide::Runtime::Buffer<float> bufferO(III, III, II, II, I+1, I);
    syrk(opa, alpha, beta, bufferA, bufferC, bufferO);
    
    for (int i = 0; i < I; i++)
    for (int j = 0; j < I; j++) {
        for (int ii = 0; ii < II; ii++)
        for (int jj = 0; jj < II; jj++)
            for (int iii = 0; iii < III; iii++)
            for (int jjj = 0; jjj < III; jjj++) {
                size_t total_i = iii + III * ii + III * II * i;
                size_t total_j = jjj + III * jj + III * II * j;

                if (total_i > total_j) {
                    continue;
                }

                if (i * 2 < I) {
                    c[total_i * n + total_j] = bufferO(jjj, iii, jj, ii, j+1, i);
                } else {
                    c[total_i * n + total_j] = bufferO(III - jjj - 1, III - iii - 1, II - jj - 1, II - ii - 1, I - j - 1, I - i - 1);
                }
            }
    }
    
    for (int i = 0; i < I; i++)
    for (int j = 0; j < I; j++)
        for (int ii = 0; ii < II; ii++)
        for (int jj = 0; jj < II; jj++)
            for (int iii = 0; iii < III; iii++)
            for (int jjj = 0; jjj < III; jjj++) {
                size_t total_i = iii + III * ii + III * II * i;
                size_t total_j = jjj + III * jj + III * II * j;
                if (total_i <= total_j) {
                    continue;
                }

                c[total_i * n + total_j] = c[total_j * n + total_i];
            }
}
