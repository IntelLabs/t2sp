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
#define GPU     1

// Constant parameters (inner loop bounds) of the design
#include "const-parameters.h"

#include <math.h>
#include <assert.h>
#include "l0_rt_helpers.h"

// For printing output
#include <stdio.h>
#include <iostream>

#define N           4
#define SIZE_I_0    TOTAL_CI * N
#define SIZE_I_1    TOTAL_IY * TOTAL_IX
#define SIZE_K_0    TOTAL_CO * KX
#define SIZE_K_1    TOTAL_CI * KY
#define SIZE_O_0    TOTAL_CO * N
#define SIZE_O_1    TOTAL_OY * TOTAL_OX

using namespace std;

void check_correctness(float *i, float *k, float *o)
{
    for (int n = 0; n < N; n++)
    for (int x = 0; x < TOTAL_OX; x++)
    for (int y = 0; y < TOTAL_OY; y++)
    for (int co = 0; co < TOTAL_CO; co++) {
        float golden = 0.0f;
        for (int ci = 0; ci < TOTAL_CI; ci++)
        for (int kx = 0; kx < KX; kx++)
        for (int ky = 0; ky < KY; ky++) {
            size_t iy = y + ky;
            size_t ix = x + kx;
            size_t i_0 = ci + TOTAL_CI * n;
            size_t i_1 = iy + TOTAL_IY * ix;
            size_t k_0 = co + TOTAL_CO * kx;
            size_t k_1 = ci + TOTAL_CI * ky;
            golden += i[i_0 + SIZE_I_0 * i_1] * k[k_0 + SIZE_K_0 * k_1];
        }
        size_t o_0 = co + TOTAL_CO * n;
        size_t o_1 = y + TOTAL_OY * x;
        assert(fabs(golden - o[o_0 + SIZE_O_0 * o_1]) < 0.005*fabs(golden));
    }
}

int main(int argc, char *argv[]) {
    // Find a driver instance with a GPU device
    auto [hDriver, hDevice, hContext] = findDriverAndDevice();
    auto hCommandList = createImmCommandList(hContext, hDevice);

    float *a = (float*)malloc(sizeof(float) * SIZE_I_0 * SIZE_I_1);
    for (int i = 0; i < SIZE_I_0 * SIZE_I_1; ++i) {
        a[i] = rand();
    }
    float *b = (float*)malloc(sizeof(float) * SIZE_K_0 * SIZE_K_1);
    for (int i = 0; i < SIZE_K_0 * SIZE_K_1; ++i) {
        b[i] = rand();
    }
    ze_image_format_t fmt = {ZE_IMAGE_FORMAT_LAYOUT_32, ZE_IMAGE_FORMAT_TYPE_FLOAT};
    auto hAImage = createImage2D(hContext, hDevice, hCommandList, fmt, SIZE_I_0, SIZE_I_1, a);
    auto hBImage = createImage2D(hContext, hDevice, hCommandList, fmt, SIZE_K_0, SIZE_K_1, b);
    auto hCImage = createImage2D(hContext, hDevice, hCommandList, fmt, SIZE_O_0, SIZE_O_1);

    auto hKernel = createKernel(hContext, hDevice, "conv_genx.bin", "kernel_A");
    setKernelArgs(hKernel, &hAImage, &hBImage, &hCImage);
    L0_SAFE_CALL(zeKernelSetGroupSize(hKernel, YY, XX, 1));

    double min_thost = SIZE_MAX *1.0f;
    double thost = 0;
    // Creates a CmTask object.
    for (size_t i = 0; i < ITER; i++) {
        ze_event_handle_t hEvent = createEvent(hContext, hDevice);
        ze_group_count_t launchArgs = {X, CO, N};
        double host_start = getTimeStamp();
        appendLaunchKernel(hCommandList, hKernel, &launchArgs, hEvent);
        zeEventHostSynchronize(hEvent, std::numeric_limits<uint32_t>::max());
        double host_end = getTimeStamp();

        thost += (host_end - host_start);
        if (host_end - host_start < min_thost) {
            min_thost = host_end - host_start;
        }
        if (ITER == 1) {
            float *c = (float*)malloc(sizeof(float) * SIZE_O_0 * SIZE_O_1);
            copyToMemory(hCommandList, c, hCImage, hEvent);
            zeEventHostSynchronize(hEvent, std::numeric_limits<uint32_t>::max());
            check_correctness(a, b, c);
            free(c);
        }
    }
    thost = thost / ITER;
    double ops = 2.0 * (long)(N * TOTAL_OX * TOTAL_OY * TOTAL_CO) * (long)(TOTAL_CI * KX * KY) / (1.0f*1000*1000*1000);

    destroy(hCommandList);
    destroy(hContext);

    if (ITER == 1) {
        printf("Pass!\n");
    } else {
        cout << "Size of tensor I: " << N << ", " << TOTAL_CI << ", " << TOTAL_IX << ", " << TOTAL_IY << "\n";
        cout << "Size of tensor K: " << TOTAL_CI << ", " << TOTAL_CO << ", " << KX << ", " << KY << "\n";
        printf("Average GFlops: %lf\n", ops / thost);
        printf("Max GFlops: %lf\n", ops / min_thost);
    }
    return 0;
}
