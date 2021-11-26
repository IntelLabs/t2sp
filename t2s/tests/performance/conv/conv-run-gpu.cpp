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
#include "const-parameters.h"

#include <assert.h>
#include "cm_rt.h"
#include "common/cm_rt_helpers.h"
#include "common/isa_helpers.h"

#define N           4
#define ITER        100
#define SIZE_I_0    TOTAL_CI * N
#define SIZE_I_1    TOTAL_IY * TOTAL_IX
#define SIZE_K_0    TOTAL_CO * KY
#define SIZE_K_1    TOTAL_CI * KX
#define SIZE_O_0    TOTAL_CO * N
#define SIZE_O_1    TOTAL_OY * TOTAL_OX

void check_correctness(float *i, float *k, float *o)
{
    for (int n = 0; n < N; n++)
    for (int x = 0; x < X; x++)
    for (int y = 0; y < Y; y++)
    for (int co = 0; co < CO; co++)
    for (int xx = 0; xx < XX; xx++)
    for (int yy = 0; yy < YY; yy++)
    for (int coo = 0; coo < COO; coo++) {
        float golden = 0.0f;
        size_t total_co = coo + COO * co;
        size_t total_oy = yy + YY * y;
        size_t total_ox = xx + XX * x;
        for (int ci = 0; ci < TOTAL_CI; ci++)
        for (int kx = 0; kx < KX; kx++)
        for (int ky = 0; ky < KY; ky++) {
            size_t total_iy = yy + YY * y + ky;
            size_t total_ix = xx + XX * x + kx;
            size_t i_0 = ci + TOTAL_CI * n;
            size_t i_1 = total_iy + TOTAL_IY * total_ix;
            size_t k_0 = total_co + TOTAL_CO * ky;
            size_t k_1 = ci + TOTAL_CI * kx;
            golden += i[i_0 + SIZE_I_0 * i_1] * k[k_0 + SIZE_K_0 * k_1];
        }
        size_t o_0 = total_co + TOTAL_CO * n;
        size_t o_1 = total_oy + TOTAL_OY * total_ox;
        assert(fabs(golden - o[o_0 + SIZE_O_0 * o_1]) < 0.005*fabs(golden));
    }
    printf("Passed\n");
}

int main(int argc, char *argv[]) {
    // Creates a CmDevice from scratch.
    CmDevice *device = nullptr;
    unsigned int version = 0;
    cm_result_check(::CreateCmDevice(device, version));

    // Creates a CmProgram object consisting of the kernel loaded from the code buffer.
    CmProgram *program = nullptr;
    std::string isa_code = cm::util::isa::loadFile("CONV_genx.isa");
    cm_result_check(device->LoadProgram(const_cast<char*>(isa_code.data()), isa_code.size(), program));

    // Creates the cmNBody kernel.
    CmKernel *kernel = nullptr;
    cm_result_check(device->CreateKernel(program, "kernel_A", kernel));

    // Create a task queue
    CmQueue* cmd_queue = NULL;
    cm_result_check(device->CreateQueue( cmd_queue ));
    srand(time(NULL));

    float *a = (float*)malloc(sizeof(float) * SIZE_I_0 * SIZE_I_1);
    for (int i = 0; i < SIZE_I_0 * SIZE_I_1; ++i) {
        a[i] = rand();
    }
    CmSurface2D *surf_a = nullptr;
    SurfaceIndex *surf_a_idx = nullptr;
    cm_result_check(device->CreateSurface2D(SIZE_I_0, SIZE_I_1, CM_SURFACE_FORMAT_R32F, surf_a));
    cm_result_check(surf_a->WriteSurface((unsigned char*)a, NULL));
    cm_result_check(surf_a->GetIndex(surf_a_idx));

    float *b = (float*)malloc(sizeof(float) * SIZE_K_0 * SIZE_K_1);
    for (int i = 0; i < SIZE_K_0 * SIZE_K_1; ++i) {
        b[i] = rand();
    }
    CmSurface2D *surf_b = nullptr;
    SurfaceIndex *surf_b_idx = nullptr;
    cm_result_check(device->CreateSurface2D(SIZE_K_0, SIZE_K_0, CM_SURFACE_FORMAT_R32F, surf_b));
    cm_result_check(surf_b->WriteSurface((unsigned char*)b, NULL));
    cm_result_check(surf_b->GetIndex(surf_b_idx));

    CmSurface2D *surf_c = nullptr;
    SurfaceIndex *surf_c_idx = nullptr;
    cm_result_check(device->CreateSurface2D(SIZE_O_0, SIZE_O_1, CM_SURFACE_FORMAT_R32F, surf_c));
    cm_result_check(surf_c->GetIndex(surf_c_idx));

    cm_result_check(kernel->SetKernelArg(0, sizeof(SurfaceIndex), surf_a_idx));
    cm_result_check(kernel->SetKernelArg(1, sizeof(SurfaceIndex), surf_b_idx));
    cm_result_check(kernel->SetKernelArg(2, sizeof(SurfaceIndex), surf_c_idx));
    UINT64 kernel_ns = 0;
    UINT64 min_tkern = SIZE_MAX;
    // Creates a CmTask object.
    for (size_t i = 0; i < ITER; i++) {
        CmTask *task = nullptr;
        cm_result_check(device->CreateTask(task));
        cm_result_check(task->AddKernel(kernel));
        CmThreadGroupSpace *thread_group_space = nullptr;
        cm_result_check(device->CreateThreadGroupSpaceEx(Y, X, 1, XX, CO, N, thread_group_space));

        UINT64 tmp_kern_time;
        CmEvent *sync_event = nullptr;
        device->InitPrintBuffer();
        cm_result_check(cmd_queue->EnqueueWithGroup(task, sync_event, thread_group_space));
        cm_result_check(sync_event->WaitForTaskFinished(1000));
        cm_result_check(sync_event->GetExecutionTime( tmp_kern_time ));
        device->FlushPrintBuffer();
        kernel_ns += tmp_kern_time;
        if (tmp_kern_time < min_tkern) {
            min_tkern = tmp_kern_time;
        }
        if (ITER == 1) {
            float *c = (float*)malloc(sizeof(float) * SIZE_O_0 * SIZE_O_1);
            cm_result_check(surf_c->ReadSurface((unsigned char *)c, sync_event));
            check_correctness(a, b, c);
        }
        cm_result_check(device->DestroyTask(task));
    }
    double tkern = kernel_ns / ITER;
    double ops = 2.0 * (long)(N * X * Y) * (long)(CO * XX * YY * COO) * (long)(CI * KX * KY * CII);

    cm_result_check(::DestroyCmDevice(device));
    printf("GFlops: %lf\n", ops / tkern);
    printf("Max GFlops: %lf\n", ops / min_tkern);

    return 0;
}
