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

#define N           16
#define TOTAL_N     N * NN
#define SIZE_P_0    TOTAL_CI * MK * MX * NN
#define SIZE_P_1    TOTAL_IY * TOTAL_IX * N
#define SIZE_W_0    TOTAL_CO * MY
#define SIZE_W_1    TOTAL_CI * KY * KX * MK
#define SIZE_V_0    TOTAL_CO * MY * MX * NN
#define SIZE_V_1    OY * OX * N

using namespace std;

void check_correctness(float *P, float *W, float *V)
{
    for (int n = 0; n < N; n++)
    for (int nn = 0; nn < NN; nn++)
    for (int x = 0; x < OX; x++)
    for (int y = 0; y < OY; y++)
    for (int mx = 0; mx < MX; mx++)
    for (int my = 0; my < MY; my++)
    for (int co = 0; co < TOTAL_CO; co++) {
        float golden = 0.0f;
        for (int kx = 0; kx < KX; kx++)
        for (int ky = 0; ky < KY; ky++)
        for (int mk = 0; mk < MK; mk++)
        for (int ci = 0; ci < CI; ci++)
        for (int cii = 0; cii < CII; cii++) {
            size_t total_ix = x*2 + kx;
            size_t total_iy = y*2 + ky;
            size_t total_ci = cii + CII*ci;
            size_t p_0 = total_ci + (TOTAL_CI)*mk + (TOTAL_CI*MK)*mx + (TOTAL_CI*MK*MX)*nn;
            size_t p_1 = total_iy + (TOTAL_IY)*total_ix + (TOTAL_IY*TOTAL_IX)*n;
            size_t w_0 = co + (TOTAL_CO)*my;
            size_t w_1 = cii + (CII)*ky + (CII*KY)*kx + (CII*KY*KX)*ci + (TOTAL_CI*KY*KX)*mk;
            golden += P[p_0 + SIZE_P_0 * p_1] * W[w_0 + SIZE_W_0 * w_1];
        }
        size_t v_0 = co + TOTAL_CO*my + TOTAL_CO*MY*mx + TOTAL_CO*MY*MX*nn;
        size_t v_1 = y + OY*x + OY*OX*n;
        assert(fabs(golden - V[v_0 + SIZE_V_0 * v_1]) < 0.005*fabs(golden));
    }
}

int main(int argc, char *argv[]) {
    // Find a driver instance with a GPU device
    auto [hDriver, hDevice, hContext] = findDriverAndDevice();
    auto hCommandList = createImmCommandList(hContext, hDevice);

    float *a = (float*)malloc(sizeof(float) * SIZE_P_0 * SIZE_P_1);
    for (int i = 0; i < SIZE_P_0 * SIZE_P_1; ++i) {
        a[i] = rand();
    }
    float *b = (float*)malloc(sizeof(float) * SIZE_W_0 * SIZE_W_1);
    for (int i = 0; i < SIZE_W_0 * SIZE_W_1; ++i) {
        b[i] = rand();
    }
    ze_image_format_t fmt = {ZE_IMAGE_FORMAT_LAYOUT_32, ZE_IMAGE_FORMAT_TYPE_FLOAT};
    auto hAImage = createImage2D(hContext, hDevice, hCommandList, fmt, SIZE_P_0, SIZE_P_1, a);
    auto hBImage = createImage2D(hContext, hDevice, hCommandList, fmt, SIZE_W_0, SIZE_W_1, b);
    auto hCImage = createImage2D(hContext, hDevice, hCommandList, fmt, SIZE_V_0, SIZE_V_1);

    auto hKernel = createKernel(hContext, hDevice, "capsule_genx.bin", "kernel_A");
    setKernelArgs(hKernel, &hCImage, &hAImage, &hBImage);
    L0_SAFE_CALL(zeKernelSetGroupSize(hKernel, MY, MX, 1));

    double min_thost = SIZE_MAX *1.0f;
    double thost = 0;
    // Creates a CmTask object.
    for (size_t i = 0; i < ITER; i++) {
        ze_event_handle_t hEvent = createEvent(hContext, hDevice);
        ze_group_count_t launchArgs = {CO, NN, N};
        double host_start = getTimeStamp();
        appendLaunchKernel(hCommandList, hKernel, &launchArgs, hEvent);
        zeEventHostSynchronize(hEvent, std::numeric_limits<uint32_t>::max());
        double host_end = getTimeStamp();

        thost += (host_end - host_start);
        if (host_end - host_start < min_thost) {
            min_thost = host_end - host_start;
        }
        if (ITER == 1) {
            float *c = (float*)malloc(sizeof(float) * SIZE_V_0 * SIZE_V_1);
            copyToMemory(hCommandList, c, hCImage, hEvent);
            zeEventHostSynchronize(hEvent, std::numeric_limits<uint32_t>::max());
            check_correctness(a, b, c);
        }
    }
    thost = thost / ITER;
    double ops = 2 * (long)(TOTAL_N * TOTAL_CO) * (long)(MY * MX * OY * OX) * (long)(TOTAL_CI * MK * KY * KX) / (1.0f*1000*1000*1000);

    destroy(hCommandList);
    destroy(hContext);

    if (ITER == 1) {
        printf("Pass!\n");
    } else {
        cout << "Size of tensor P: " << TOTAL_N << ", " << TOTAL_CI << ", " << TOTAL_IX << ", " << TOTAL_IY
                                     << ", " << MX << ", " << MK << "\n";
        cout << "Size of tensor W: " << TOTAL_CI << ", " << TOTAL_CO << ", " << KX << ", " << KY
                                     << ", " << MK << ", " << MY << "\n";
        printf("Average GFlops: %lf\n", ops / thost);
        printf("Max GFlops: %lf\n", ops / min_thost);
    }
    return 0;
}
