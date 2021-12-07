#define GPU     1
#include "const-parameters.h"

#include <assert.h>
#include "cm_rt.h"
#include "common/cm_rt_helpers.h"
#include "common/isa_helpers.h"

#define N           64
#define SIZE_I_0    TOTAL_CI * MK * MX
#define SIZE_I_1    TOTAL_IY * TOTAL_IX * N
#define SIZE_K_0    TOTAL_CO * MY
#define SIZE_K_1    TOTAL_CI * KY * KX * MK
#define SIZE_O_0    TOTAL_CO * MY * MX
#define SIZE_O_1    OY * OX * N

void check_correctness(float *i, float *k, float *o)
{
    for (int n = 0; n < N; n++)
    for (int x = 0; x < OX; x++)
    for (int y = 0; y < OY; y++)
    for (int mx = 0; mx < MX; mx++)
    for (int my = 0; my < MY; my++)
    for (int co = 0; co < TOTAL_CO; co++) {
        float golden = 0.0f;
        for (int kx = 0; kx < KX; kx++)
        for (int ky = 0; ky < KY; ky++)
        for (int mk = 0; mk < MK; mk++)
        for (int ci = 0; ci < TOTAL_CI; ci++) {
            size_t total_ix = x*2 + kx;
            size_t total_iy = y*2 + ky;
            size_t i_0 = ci + (TOTAL_CI)*mk + (TOTAL_CI*MK)*mx;
            size_t i_1 = total_iy + (TOTAL_IY)*total_ix + (TOTAL_IY*TOTAL_IX)*n;
            size_t k_0 = co + (TOTAL_CO)*my;
            size_t k_1 = (ci % CII) + (CII)*ky + (CII*KY)*kx + (CII*KY*KX)*(ci/CII) + (CII*KY*KX*CI)*mk;
            golden += i[i_0 + SIZE_I_0 * i_1] * k[k_0 + SIZE_K_0 * k_1];
        }
        size_t o_0 = co + TOTAL_CO*my + TOTAL_CO*MY*mx;
        size_t o_1 = y + OY*x + OY*OX*n;
        assert(fabs(golden - o[o_0 + SIZE_O_0 * o_1]) < 0.005*fabs(golden));
    }
}

int main(int argc, char *argv[]) {
    // Creates a CmDevice from scratch.
    CmDevice *device = nullptr;
    unsigned int version = 0;
    cm_result_check(::CreateCmDevice(device, version));

    // Creates a CmProgram object consisting of the kernel loaded from the code buffer.
    CmProgram *program = nullptr;
    std::string isa_code = cm::util::isa::loadFile("capsule_genx.isa");
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
    cm_result_check(device->CreateSurface2D(SIZE_K_0, SIZE_K_1, CM_SURFACE_FORMAT_R32F, surf_b));
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
        cm_result_check(device->CreateThreadGroupSpace(MY, MX, CO, N, thread_group_space));

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
    double ops = 2 * (long)(N * TOTAL_CO) * (long)(MY * MX * OY * OX) * (long)(TOTAL_CI * MK * KY * KX);

    cm_result_check(::DestroyCmDevice(device));

    if (ITER == 1) {
        printf("Pass!\n");
    } else {
        printf("Average GFlops: %lf\n", ops / tkern);
        printf("Max GFlops: %lf\n", ops / min_tkern);
    }
    return 0;
}