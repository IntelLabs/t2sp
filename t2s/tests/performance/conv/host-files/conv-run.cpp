#define GPU     1
#include "sizes.h"

#include <assert.h>
#include "cm_rt.h"
#include "common/cm_rt_helpers.h"
#include "common/isa_helpers.h"

#define ITER        1
#define SIZE_I_0    TOTAL_CI * B
#define SIZE_I_1    TOTAL_IW * TOTAL_IH
#define SIZE_K_0    TOTAL_CI * KW
#define SIZE_K_1    TOTAL_CO * KH
#define SIZE_O_0    TOTAL_CO * B
#define SIZE_O_1    TOTAL_OW * TOTAL_OH

void check_correctness(float *i, float *k, float *o)
{
    for (int b = 0; b < B; b++)
    for (int h = 0; h < H; h++)
    for (int w = 0; w < W; w++) {
        for (int co = 0; co < CO; co++)
        for (int hh = 0; hh < HH; hh++)
        for (int ww = 0; ww < WW; ww++)
        for (int coo = 0; coo < COO; coo++) {
            float golden = 0.0f;
            size_t total_co = coo + COO * co;
            size_t total_ow = ww + WW * w;
            size_t total_oh = hh + HH * h;
            for (int r_ci = 0; r_ci < TOTAL_CI; r_ci++)
            for (int r_kh = 0; r_kh < KH; r_kh++)
            for (int r_kw = 0; r_kw < KW; r_kw++) {
                size_t total_iw = ww + WW * w + r_kw;
                size_t total_ih = hh + HH * h + r_kh;
                size_t i_0 = r_ci + TOTAL_CI * b;
                size_t i_1 = total_iw + TOTAL_IW * b;
                size_t k_0 = r_ci + TOTAL_CI * r_kw;
                size_t k_1 = total_co + TOTAL_CO * r_kh;
                golden += i[i_0 + SIZE_I_0 * i_1] * k[k_0 + SIZE_K_0 * k_1];
            }
            size_t o_0 = total_co + TOTAL_CO * b;
            size_t o_1 = total_ow + TOTAL_OW * total_oh;
            printf("( %lf, %lf )\n", golden, o[o_0 + SIZE_O_0 * o_1]);
            assert(fabs(golden - o[o_0 + SIZE_O_0 * o_1]) < 0.005*fabs(golden));
        }
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
    cm_result_check(device->CreateKernel(program, "kernel_X", kernel));

    // Create a task queue
    CmQueue* cmd_queue = NULL;
    cm_result_check(device->CreateQueue( cmd_queue ));
    srand(time(NULL));

    float *a = (float*)malloc(sizeof(float) * SIZE_I_0 * SIZE_I_1);
    for (int i = 0; i < SIZE_I_0 * SIZE_I_1; ++i) {
        a[i] = 1;
    }
    CmSurface2D *surf_a = nullptr;
    SurfaceIndex *surf_a_idx = nullptr;
    cm_result_check(device->CreateSurface2D(SIZE_I_1, SIZE_I_0, CM_SURFACE_FORMAT_R32F, surf_a));
    cm_result_check(surf_a->WriteSurface((unsigned char*)a, NULL));
    cm_result_check(surf_a->GetIndex(surf_a_idx));

    float *b = (float*)malloc(sizeof(float) * SIZE_K_0 * SIZE_K_1);
    for (int i = 0; i < SIZE_K_0 * SIZE_K_1; ++i) {
        b[i] = 1;
    }
    CmSurface2D *surf_b = nullptr;
    SurfaceIndex *surf_b_idx = nullptr;
    cm_result_check(device->CreateSurface2D(SIZE_K_1, SIZE_K_0, CM_SURFACE_FORMAT_R32F, surf_b));
    cm_result_check(surf_b->WriteSurface((unsigned char*)b, NULL));
    cm_result_check(surf_b->GetIndex(surf_b_idx));

    CmSurface2D *surf_c = nullptr;
    SurfaceIndex *surf_c_idx = nullptr;
    cm_result_check(device->CreateSurface2D(SIZE_O_1, SIZE_O_0, CM_SURFACE_FORMAT_R32F, surf_c));
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
        cm_result_check(device->CreateThreadGroupSpaceEx(W, H, 1, HH, CO, B, thread_group_space));

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
    double ops = 2.0 * (long)(B * H * W) * (long)(CO * HH * WW * COO) * (long)(CI * KH * KW * CII);

    cm_result_check(::DestroyCmDevice(device));
    printf("GFlops: %lf\n", ops / tkern);
    printf("Max GFlops: %lf\n", ops / min_tkern);

    return 0;
}
