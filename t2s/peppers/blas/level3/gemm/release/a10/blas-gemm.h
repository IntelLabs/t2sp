#ifndef blas_gemm_h
#define blas_gemm_h

// use template
void sgemm(char transa, char transb, int m, int n, int k, float alpha, float *a, int lda,  float *b, int ldb, float beta, float *c, int ldc);

#endif
