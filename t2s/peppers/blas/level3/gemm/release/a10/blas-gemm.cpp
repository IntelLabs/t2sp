#include "gemm-interface.h"
#include "blas-gemm.h"
#include "const-parameters.h"
#include "HalideBuffer.h"

#define MATRIX_OP_IDENTITY 0
#define MATRIX_OP_TRANSPOSE 1

void sgemm(char transa, char transb, int m, int n, int k, float alpha, float *a, int lda,  float *b, int ldb, float beta, float *c, int ldc) {
    const int I = (m + III * II - 1) / (III * II);
    const int J = (n + JJJ * JJ - 1) / (JJJ * JJ);
    const int K = (k + KKK * KK - 1) / (KKK * KK);

    int opa = (transa == 'N' || transa == 'n') ? MATRIX_OP_IDENTITY : MATRIX_OP_TRANSPOSE;
    int opb = (transb == 'N' || transb == 'n') ? MATRIX_OP_IDENTITY : MATRIX_OP_TRANSPOSE;

    Halide::Runtime::Buffer<float> bufferA(k, m), bufferB(n, k), bufferC(n, m);
    for (size_t iterI = 0; iterI < m; iterI++)
        for (size_t iterK = 0; iterK < k; iterK++)
            bufferA(iterK, iterI) = a[iterI * k + iterK];
    for (size_t iterK = 0; iterK < k; iterK++)
        for (size_t iterJ = 0; iterJ < n; iterJ++)
            bufferB(iterJ, iterK) = b[iterK * n + iterJ];
    for (size_t iterI = 0; iterI < m; iterI++)
        for (size_t iterJ = 0; iterJ < n; iterJ++)
            bufferC(iterJ, iterI) = c[iterI * n + iterJ];
    
    Halide::Runtime::Buffer<float> bufferO(JJJ, III, JJ, II, J, I);
    gemm(opa, opb, alpha, beta, bufferA, bufferB, bufferC, bufferO);
    
    for (int i = 0; i < I; i++)
    for (int j = 0; j < J; j++)
        for (int ii = 0; ii < II; ii++)
        for (int jj = 0; jj < JJ; jj++)
            for (int iii = 0; iii < III; iii++)
            for (int jjj = 0; jjj < JJJ; jjj++) {
                size_t total_i = iii + III * ii + III * II * i;
                size_t total_j = jjj + JJJ * jj + JJJ * JJ * j;
                c[total_i * n + total_j] = bufferO(jjj, iii, jj, ii, j, i);
            }
}
