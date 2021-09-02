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
#include "host.h"

// The only header file needed for including T2S.
#include "HalideBuffer.h"

#include <math.h>
// For printing output
#include <stdio.h>
#include <iostream>

// For validation of results.
#include <assert.h>

// using namespace Halide;
using namespace std;

#define SIZE 8

int main() {
    Halide::Runtime::Buffer<float> input(SIZE, SIZE);
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            input(i, j) = (i + 1)*(j + 1) + log(i * j + 1);
        }
    }

    Halide::Runtime::Buffer<float> result(SIZE, SIZE);
    LU(input, result);

    // Step 3: Validate the results
    printf("*** Input:\n");
    for (int j = 0; j < SIZE; j++) {
      for (int i = 0; i < SIZE; i++) {
        printf("%5.2f ", input(i, j));
      }
      printf("\n");
    }

    // Do exactly the same compute in C style
    Halide::Runtime::Buffer<float> PrevV(SIZE, SIZE, SIZE), V(SIZE, SIZE, SIZE), L(SIZE, SIZE, SIZE), U(SIZE, SIZE, SIZE), O(SIZE, SIZE);
    for (int k = 0; k < SIZE; k++) {
      for (int j = k; j < SIZE; j++) {
        for (int i = k; i < SIZE; i++) {
          if (k == 0) {
              PrevV(i, j, k) = input(i, j);
          } else {
              PrevV(i, j, k) = V(i, j, k - 1);
          }

          if (j == k) {
              U(i, j, k) = PrevV(i, j, k);
          } else {
              // operation f: No change to row k, because we do not do pivoting
              U(i, j, k) = U(i, j - 1, k);

              // operation g
              if (i == k) {
                  L(i, j, k) = PrevV(i, j, k) / U(i, j - 1, k);
              } else {
                  L(i, j, k) = L(i - 1, j, k);
              }
              V(i, j, k) = PrevV(i, j, k) - L(i, j, k) * U(i, j - 1, k);
          }

           // Final results in the decomposition include
           if (j == k) {
               O(i, j) =  U(i, j, k);
           } else {
               if (i == k) {
                   O(i, j) =  L(i, j, k);
               }
           }
        }
      }
    }

    // Check if the results are the same as we get from the C style compute
    printf("*** C style result (URE style):\n");
    bool pass = true;
    for (int j = 0; j < SIZE; j++) {
      for (int i = 0; i < SIZE; i++) {
            bool correct = (abs(O(i, j) - result(i, j)) < 1e-2);
            printf("%5.2f (%5.2f%s)", O(i, j), result(i, j), correct ? "" : " !!");
            pass = pass && correct;
        }
        printf("\n");
    }

    // Check if the results can reproduce the input, i.e. L*U = A
    printf("*** L * U in C style (Input):\n");
    for (int j = 0; j < SIZE; j++) {
      for (int i = 0; i < SIZE; i++) {
        float sum = 0;
        // j'th row of O times i'th column of O
        for (int k = 0; k < SIZE; k++) {
            float l, u;
            l = (j > k) ? O(k, j) : (j == k) ? 1 : 0;
            u = (k > i) ? 0 : O(i, k);
            sum += l * u;
        }
        bool correct = (abs(sum - input(i, j)) < 1e-2);
        printf("%5.2f (%5.2f%s)", sum, input(i, j), correct ? "" : " !!");
        pass = pass && correct;
      }
      printf("\n");
    }

    fflush(stdout);
    assert(pass);
    cout << "Success!\n";
    return 0;
}



