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

#define K           64
#define J           128
#define I           32

#include <assert.h>
#include "cm_rt.h"
#include "common/cm_rt_helpers.h"
#include "common/isa_helpers.h"

// For printing output
#include <stdio.h>
#include <iostream>

#define TOTAL_I     III*II*I
#define TOTAL_J     JJJ*JJ*J
#define TOTAL_K     KKK*KK*K
#define SIZE_A      TOTAL_I*TOTAL_K
#define SIZE_B      TOTAL_J*TOTAL_K
#define SIZE_C      TOTAL_I*TOTAL_J

using namespace std;

void check_correctness(float *a, float *b, float *c)
{
    for (int i = 0; i < I; i++)
    for (int j = 0; j < J; j++) {
        for (int ii = 0; ii < II; ii++)
        for (int jj = 0; jj < JJ; jj++) {
            for (int iii = 0; iii < III; iii++)
            for (int jjj = 0; jjj < JJJ; jjj++) {
                size_t total_i = iii + III * ii + III * II * i;
                size_t total_j = jjj + JJJ * jj + JJJ * JJ * j;
                float golden = 0.0f;
                for (int k = 0; k < TOTAL_K; k++)
                    golden += a[k + total_i*TOTAL_K] * b[total_j + k*TOTAL_J];
                assert(fabs(golden - c[total_j + total_i*TOTAL_J]) < 0.005*fabs(golden));
            }
        }
    }
}

int main(int argc, char *argv[]) {
    // Creates a CmDevice from scratch.
    CmDevice *device = nullptr;
    unsigned int version = 0;
    cm_result_check(::CreateCmDevice(device, version));

    // Creates a CmProgram object consisting of the kernel loaded from the code buffer.
    CmProgram *program = nullptr;
    std::string isa_code = cm::util::isa::loadFile("gemm_genx.isa");
    cm_result_check(device->LoadProgram(const_cast<char*>(isa_code.data()), isa_code.size(), program));

    // Creates the cmNBody kernel.
    CmKernel *kernel = nullptr;
    cm_result_check(device->CreateKernel(program, "kernel_X", kernel));

    // Create a task queue
    CmQueue* cmd_queue = NULL;
    cm_result_check(device->CreateQueue( cmd_queue ));
    srand(time(NULL));

    float *a = (float*)malloc(sizeof(float) * SIZE_A);
    for (int i = 0; i < SIZE_A; ++i) {
        a[i] = rand();
    }
    CmSurface2D *surf_a = nullptr;
    SurfaceIndex *surf_a_idx = nullptr;
    cm_result_check(device->CreateSurface2D(TOTAL_K, TOTAL_I, CM_SURFACE_FORMAT_R32F, surf_a));
    cm_result_check(surf_a->WriteSurface((unsigned char*)a, NULL));
    cm_result_check(surf_a->GetIndex(surf_a_idx));

    float *b = (float*)malloc(sizeof(float) * SIZE_B);
    for (int i = 0; i < SIZE_B; ++i) {
        b[i] = rand();
    }
    CmSurface2D *surf_b = nullptr;
    SurfaceIndex *surf_b_idx = nullptr;
    cm_result_check(device->CreateSurface2D(TOTAL_J, TOTAL_K, CM_SURFACE_FORMAT_R32F, surf_b));
    cm_result_check(surf_b->WriteSurface((unsigned char*)b, NULL));
    cm_result_check(surf_b->GetIndex(surf_b_idx));

    CmSurface2D *surf_c = nullptr;
    SurfaceIndex *surf_c_idx = nullptr;
    cm_result_check(device->CreateSurface2D(TOTAL_J, TOTAL_I, CM_SURFACE_FORMAT_R32F, surf_c));
    cm_result_check(surf_c->GetIndex(surf_c_idx));

    int _A_extent_0 = TOTAL_K;
    cm_result_check(kernel->SetKernelArg(0, sizeof(int), &_A_extent_0));
    cm_result_check(kernel->SetKernelArg(1, sizeof(SurfaceIndex), surf_a_idx));
    cm_result_check(kernel->SetKernelArg(2, sizeof(SurfaceIndex), surf_b_idx));
    cm_result_check(kernel->SetKernelArg(3, sizeof(SurfaceIndex), surf_c_idx));
    UINT64 kernel_ns = 0;
    UINT64 min_tkern = SIZE_MAX;
    // Creates a CmTask object.
    for (size_t i = 0; i < ITER; i++) {
        CmTask *task = nullptr;
        cm_result_check(device->CreateTask(task));
        cm_result_check(task->AddKernel(kernel));
        CmThreadGroupSpace *thread_group_space = nullptr;
        cm_result_check(device->CreateThreadGroupSpace(JJ, II, J, I, thread_group_space));

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
            float *c = (float*)malloc(sizeof(float) * SIZE_C);
            cm_result_check(surf_c->ReadSurface((unsigned char *)c, sync_event));
            check_correctness(a, b, c);
        }
        cm_result_check(device->DestroyTask(task));
    }
    double tkern = kernel_ns / ITER;
    double ops = (long)TOTAL_I*(long)TOTAL_J*(long)TOTAL_K*2.0;

    cm_result_check(::DestroyCmDevice(device));

    if (ITER == 1) {
        printf("Pass!\n");
    } else {
        cout << "Size of matrix A: " << TOTAL_I << " * " << TOTAL_K << "\n";
        cout << "Size of matrix B: " << TOTAL_K << " * " << TOTAL_J << "\n";
        printf("Average GFlops: %lf\n", ops / tkern);
        printf("Max GFlops: %lf\n", ops / min_tkern);
    }

    return 0;
}
