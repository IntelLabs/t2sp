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
#include "Math.h"
#include "Error.h"

namespace Matrix {

void get_cofactor(const matrix_t &in,
                  size_t r, size_t c,
                  matrix_t &out) {
    size_t n = in.size();
    size_t pos_x = 0, pos_y = 0;

    for (size_t i = 0; i < n; ++i)
        for (size_t j = 0; j < n; ++j) {
            if (i != r && j != c) {
                out[pos_x][pos_y++] = in[i][j];
                if (pos_y == n-1) {
                    pos_y = 0;
                    pos_x++;
                }
            }
        }
}

int get_determinant(const matrix_t &in) {
    size_t n = in.size();
    int res = 0;
    if (n == 1)
        return in[0][0];

    int sign = 1;
    matrix_t cof(n-1, row_t(n-1));

    for (size_t c = 0; c < n; ++c) {
        get_cofactor(in, 0, c, cof);
        res += sign * in[0][c] * get_determinant(cof);
        sign = -sign;
    }
    return res;
}

void get_adjoint(const matrix_t &in, matrix_t &out) {
    int n = in.size();
    if (n == 1) {
        out[0][0] = 1;
        return;
    }
    matrix_t cof(n-1, row_t(n-1));

    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j) {
            get_cofactor(in, i, j, cof);
            int sign = ((i+j)%2 == 0) ? 1 : -1;
            out[j][i] = sign * get_determinant(cof);
        }
}

bool get_inverse(const matrix_t &in, matrix_t &out) {
    internal_assert(in.size() == out.size());

    size_t n = in.size();
    if (in[0].size() != n) {
        user_warning << "Only square matrix can get its inverse";
        return false;
    }
    int det = get_determinant(in);
    if (det == 0) {
        user_warning << "Singular matrix can not get its inverse";
        return false;
    }

    matrix_t adj(n, row_t(n));
    get_adjoint(in, adj);

    for (size_t i = 0; i < n; ++i)
        for (size_t j = 0; j < n; ++j) {
            if (adj[i][j] % det != 0) {
                user_warning << "Only support integer matrix";
                return false;
            }
            out[i][j] = adj[i][j] / det;
        }
    return true;
}

}  // namespace Matrix