// auto-regressive-filter-testing-ures-in-c.c
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int main() {
    const int T = 10;
    const int I = 3;
    const int c = 2;
    
    // Inputs.
    int phi[1][I+1], epsilon[1][T+1];
    for (int i = 1; i <= I; i++) { phi[0][i] = rand(); }
    for (int t = 1; t <= T; t++) { epsilon[0][t] = rand(); }
    
    // Arrays storing the values of UREs.
    int Phi[I+1][T+1];
    int Chi[I+1][T+1];
    int X[I+1][T+1];
    
    // Run the UREs.
    for (int t= 1; t <= T; t++) {
      for (int i = 1; i <= I; i++) {
        Phi[i][t] = (t-1 == 0) ? phi[0][i] : Phi[i][t-1];
        Chi[i][t] = (t-1 == 0) ? 0 : (i-1 == 0 ? X[i-(1-I)][t-1] : Chi[i-1][t-1]);
        X[i][t]   = (i-1 == 0 ? c+epsilon[0][t] : X[i-1][t]) + Phi[i][t]*Chi[i][t];
      }
    }
    
    // Validate correctness
    int golden[T+1];
    for (int t= 1; t <= T; t++) {
      golden[t] = c + epsilon[0][t];
      for (int i = 1; i <= I; i++) {
        if (t - i > 0) {
           golden[t] += phi[0][i] * golden[t - i];
        }
      }
      assert(golden[t] == X[I][t]);
    }
    printf("Success!\n");
}

