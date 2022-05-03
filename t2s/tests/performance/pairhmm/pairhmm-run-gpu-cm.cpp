#define GPU     1

// Constant parameters (inner loop bounds) of the design
#include "const-parameters.h"
#define OR          8
#define OH          8

#define NUM_READS   (OR*RR)
#define NUM_HAPS    (OH*HH)

#include <assert.h>
#include "cm_rt.h"
#include "common/cm_rt_helpers.h"
#include "common/isa_helpers.h"
#include "context.h"

// For printing output
#include <stdio.h>
#include <iostream>

float (*delta)[READ_LEN], (*eta)[READ_LEN], (*zeta)[READ_LEN];
float (*alpha_match)[READ_LEN], (*alpha_gap)[READ_LEN], (*beta_match)[READ_LEN], (*beta_gap)[READ_LEN];
char (*H)[NUM_HAPS], (*R)[READ_LEN];

template<size_t N1, size_t N2>
void new_characters(char (*b)[N2]) {
    for (size_t i = 0; i < N1; i++) {
        for (size_t j = 0; j < N2; j++) {
            b[i][j] = 'A' + (rand() % 26);
        }
    }
}

template<size_t N1, size_t N2>
void new_probability(float (*b)[N2]) {
    for (size_t i = 0; i < N1; i++) {
        for (size_t j = 0; j < N2; j++) {
            b[i][j] = (float)(rand() * 1.0) / (float)(RAND_MAX * 1.0);
        }
    }
}

void set_real_input(CmDevice *device, CmKernel *kernel)
{
    // input buffer
    delta       = new float[NUM_READS][READ_LEN];
    eta         = new float[NUM_READS][READ_LEN];
    zeta        = new float[NUM_READS][READ_LEN];
    alpha_match = new float[NUM_READS][READ_LEN];
    alpha_gap   = new float[NUM_READS][READ_LEN];
    beta_match  = new float[NUM_READS][READ_LEN];
    beta_gap    = new float[NUM_READS][READ_LEN];
    R           = new char[NUM_READS][READ_LEN];
    H           = new char[HAP_LEN][NUM_HAPS];

    // temporary buffer
    char R_c[NUM_READS][READ_LEN];
    char R_i[NUM_READS][READ_LEN];
    char R_d[NUM_READS][READ_LEN];
    char R_q[NUM_READS][READ_LEN];
    srand(time(0));
    new_characters<HAP_LEN, NUM_HAPS>(H);
    new_characters<NUM_READS, READ_LEN>(R);
    new_characters<NUM_READS, READ_LEN>(R_c);
    new_characters<NUM_READS, READ_LEN>(R_i);
    new_characters<NUM_READS, READ_LEN>(R_d);
    new_characters<NUM_READS, READ_LEN>(R_q);

    Context<float> ctx;
    for (size_t i = 0; i < NUM_READS; i++) {
        for (size_t j = 0; j < READ_LEN; j++) {
            float alpha = ctx.set_mm_prob(R_i[i][j], R_d[i][j]);
            float beta  = 1.0f - ctx.ph2pr[R_c[i][j]];
            delta[i][j] = ctx.ph2pr[R_i[i][j]];
            zeta[i][j]  = ctx.ph2pr[R_d[i][j]];
            eta[i][j]   = ctx.ph2pr[R_c[i][j]];
            alpha_match[i][j] = alpha * (1.0f - ctx.ph2pr[R_q[i][j]]);
            alpha_gap[i][j]   = alpha * ctx.ph2pr[R_q[i][j]] / 3.0f;
            beta_match[i][j]  = beta * (1.0f - ctx.ph2pr[R_q[i][j]]);
            beta_gap[i][j]    = beta * ctx.ph2pr[R_q[i][j]] / 3.0f;
        }
    }

    CmSurface2D *delta_surf, *eta_surf, *zeta_surf, *H_surf, *R_surf;
    CmSurface2D *alpha_match_surf, *alpha_gap_surf, *beta_match_surf, *beta_gap_surf;
    cm_result_check(device->CreateSurface2D(NUM_HAPS, HAP_LEN, CM_SURFACE_FORMAT_R8_UINT, H_surf));
    cm_result_check(device->CreateSurface2D(READ_LEN, NUM_READS, CM_SURFACE_FORMAT_R8_UINT, R_surf));
    cm_result_check(device->CreateSurface2D(READ_LEN, NUM_READS, CM_SURFACE_FORMAT_R32F, delta_surf));
    cm_result_check(device->CreateSurface2D(READ_LEN, NUM_READS, CM_SURFACE_FORMAT_R32F, zeta_surf));
    cm_result_check(device->CreateSurface2D(READ_LEN, NUM_READS, CM_SURFACE_FORMAT_R32F, eta_surf));
    cm_result_check(device->CreateSurface2D(READ_LEN, NUM_READS, CM_SURFACE_FORMAT_R32F, alpha_match_surf));
    cm_result_check(device->CreateSurface2D(READ_LEN, NUM_READS, CM_SURFACE_FORMAT_R32F, alpha_gap_surf));
    cm_result_check(device->CreateSurface2D(READ_LEN, NUM_READS, CM_SURFACE_FORMAT_R32F, beta_match_surf));
    cm_result_check(device->CreateSurface2D(READ_LEN, NUM_READS, CM_SURFACE_FORMAT_R32F, beta_gap_surf));
    cm_result_check(H_surf->WriteSurface((unsigned char*)H, NULL));
    cm_result_check(R_surf->WriteSurface((unsigned char*)R, NULL));
    cm_result_check(delta_surf->WriteSurface((unsigned char*)delta, NULL));
    cm_result_check(zeta_surf->WriteSurface((unsigned char*)zeta, NULL));
    cm_result_check(eta_surf->WriteSurface((unsigned char*)eta, NULL));
    cm_result_check(alpha_match_surf->WriteSurface((unsigned char*)alpha_match, NULL));
    cm_result_check(alpha_gap_surf->WriteSurface((unsigned char*)alpha_gap, NULL));
    cm_result_check(beta_match_surf->WriteSurface((unsigned char*)beta_match, NULL));
    cm_result_check(beta_gap_surf->WriteSurface((unsigned char*)beta_gap, NULL));

    SurfaceIndex *delta_surf_idx, *eta_surf_idx, *zeta_surf_idx, *H_surf_idx, *R_surf_idx;
    SurfaceIndex *alpha_match_surf_idx, *alpha_gap_surf_idx, *beta_match_surf_idx, *beta_gap_surf_idx;
    cm_result_check(H_surf->GetIndex(H_surf_idx));
    cm_result_check(R_surf->GetIndex(R_surf_idx));
    cm_result_check(delta_surf->GetIndex(delta_surf_idx));
    cm_result_check(zeta_surf->GetIndex(zeta_surf_idx));
    cm_result_check(eta_surf->GetIndex(eta_surf_idx));
    cm_result_check(alpha_match_surf->GetIndex(alpha_match_surf_idx));
    cm_result_check(alpha_gap_surf->GetIndex(alpha_gap_surf_idx));
    cm_result_check(beta_match_surf->GetIndex(beta_match_surf_idx));
    cm_result_check(beta_gap_surf->GetIndex(beta_gap_surf_idx));
    cm_result_check(kernel->SetKernelArg(0, sizeof(SurfaceIndex), H_surf_idx));
    cm_result_check(kernel->SetKernelArg(1, sizeof(SurfaceIndex), R_surf_idx));
    cm_result_check(kernel->SetKernelArg(3, sizeof(SurfaceIndex), alpha_gap_surf_idx));
    cm_result_check(kernel->SetKernelArg(4, sizeof(SurfaceIndex), alpha_match_surf_idx));
    cm_result_check(kernel->SetKernelArg(5, sizeof(SurfaceIndex), beta_gap_surf_idx));
    cm_result_check(kernel->SetKernelArg(6, sizeof(SurfaceIndex), beta_match_surf_idx));
    cm_result_check(kernel->SetKernelArg(7, sizeof(SurfaceIndex), delta_surf_idx));
    cm_result_check(kernel->SetKernelArg(8, sizeof(SurfaceIndex), eta_surf_idx));
    cm_result_check(kernel->SetKernelArg(9, sizeof(SurfaceIndex), zeta_surf_idx));
}

void set_pseudo_input(CmDevice *device, CmKernel *kernel)
{
    // input buffer
    delta       = new float[NUM_READS][READ_LEN];
    eta         = new float[NUM_READS][READ_LEN];
    zeta        = new float[NUM_READS][READ_LEN];
    alpha_match = new float[NUM_READS][READ_LEN];
    alpha_gap   = new float[NUM_READS][READ_LEN];
    beta_match  = new float[NUM_READS][READ_LEN];
    beta_gap    = new float[NUM_READS][READ_LEN];
    R           = new char[NUM_READS][READ_LEN];
    H           = new char[HAP_LEN][NUM_HAPS];

    new_characters<HAP_LEN, NUM_HAPS>(H);
    new_characters<NUM_READS, READ_LEN>(R);
    new_probability<NUM_READS, READ_LEN>(delta);
    new_probability<NUM_READS, READ_LEN>(eta);
    new_probability<NUM_READS, READ_LEN>(zeta);
    srand(time(0));
    for (size_t i = 0; i < NUM_READS; i++) {
        for (size_t j = 0; j < READ_LEN; j++) {
            float Q = (float)(rand() * 1.0) / (float)(RAND_MAX * 1.0);
            float alpha = (float)(rand() * 1.0) / (float)(RAND_MAX * 1.0);
            float beta = (float)(rand() * 1.0) / (float)(RAND_MAX * 1.0);
            alpha_match[i][j] = (1.0f - Q) * alpha;
            alpha_gap[i][j] = (Q / 3) * alpha;
            beta_match[i][j] = (1.0f - Q) * beta;
            beta_gap[i][j] = (Q / 3) * beta;
        }
    }

    CmSurface2D *delta_surf, *eta_surf, *zeta_surf, *H_surf, *R_surf;
    CmSurface2D *alpha_match_surf, *alpha_gap_surf, *beta_match_surf, *beta_gap_surf;
    cm_result_check(device->CreateSurface2D(NUM_HAPS, HAP_LEN, CM_SURFACE_FORMAT_R8_UINT, H_surf));
    cm_result_check(device->CreateSurface2D(READ_LEN, NUM_READS, CM_SURFACE_FORMAT_R8_UINT, R_surf));
    cm_result_check(device->CreateSurface2D(READ_LEN, NUM_READS, CM_SURFACE_FORMAT_R32F, delta_surf));
    cm_result_check(device->CreateSurface2D(READ_LEN, NUM_READS, CM_SURFACE_FORMAT_R32F, zeta_surf));
    cm_result_check(device->CreateSurface2D(READ_LEN, NUM_READS, CM_SURFACE_FORMAT_R32F, eta_surf));
    cm_result_check(device->CreateSurface2D(READ_LEN, NUM_READS, CM_SURFACE_FORMAT_R32F, alpha_match_surf));
    cm_result_check(device->CreateSurface2D(READ_LEN, NUM_READS, CM_SURFACE_FORMAT_R32F, alpha_gap_surf));
    cm_result_check(device->CreateSurface2D(READ_LEN, NUM_READS, CM_SURFACE_FORMAT_R32F, beta_match_surf));
    cm_result_check(device->CreateSurface2D(READ_LEN, NUM_READS, CM_SURFACE_FORMAT_R32F, beta_gap_surf));
    cm_result_check(H_surf->WriteSurface((unsigned char*)H, NULL));
    cm_result_check(R_surf->WriteSurface((unsigned char*)R, NULL));
    cm_result_check(delta_surf->WriteSurface((unsigned char*)delta, NULL));
    cm_result_check(zeta_surf->WriteSurface((unsigned char*)zeta, NULL));
    cm_result_check(eta_surf->WriteSurface((unsigned char*)eta, NULL));
    cm_result_check(alpha_match_surf->WriteSurface((unsigned char*)alpha_match, NULL));
    cm_result_check(alpha_gap_surf->WriteSurface((unsigned char*)alpha_gap, NULL));
    cm_result_check(beta_match_surf->WriteSurface((unsigned char*)beta_match, NULL));
    cm_result_check(beta_gap_surf->WriteSurface((unsigned char*)beta_gap, NULL));

    SurfaceIndex *delta_surf_idx, *eta_surf_idx, *zeta_surf_idx, *H_surf_idx, *R_surf_idx;
    SurfaceIndex *alpha_match_surf_idx, *alpha_gap_surf_idx, *beta_match_surf_idx, *beta_gap_surf_idx;
    cm_result_check(H_surf->GetIndex(H_surf_idx));
    cm_result_check(R_surf->GetIndex(R_surf_idx));
    cm_result_check(delta_surf->GetIndex(delta_surf_idx));
    cm_result_check(zeta_surf->GetIndex(zeta_surf_idx));
    cm_result_check(eta_surf->GetIndex(eta_surf_idx));
    cm_result_check(alpha_match_surf->GetIndex(alpha_match_surf_idx));
    cm_result_check(alpha_gap_surf->GetIndex(alpha_gap_surf_idx));
    cm_result_check(beta_match_surf->GetIndex(beta_match_surf_idx));
    cm_result_check(beta_gap_surf->GetIndex(beta_gap_surf_idx));
    cm_result_check(kernel->SetKernelArg(0, sizeof(SurfaceIndex), H_surf_idx));
    cm_result_check(kernel->SetKernelArg(1, sizeof(SurfaceIndex), R_surf_idx));
    cm_result_check(kernel->SetKernelArg(3, sizeof(SurfaceIndex), alpha_gap_surf_idx));
    cm_result_check(kernel->SetKernelArg(4, sizeof(SurfaceIndex), alpha_match_surf_idx));
    cm_result_check(kernel->SetKernelArg(5, sizeof(SurfaceIndex), beta_gap_surf_idx));
    cm_result_check(kernel->SetKernelArg(6, sizeof(SurfaceIndex), beta_match_surf_idx));
    cm_result_check(kernel->SetKernelArg(7, sizeof(SurfaceIndex), delta_surf_idx));
    cm_result_check(kernel->SetKernelArg(8, sizeof(SurfaceIndex), eta_surf_idx));
    cm_result_check(kernel->SetKernelArg(9, sizeof(SurfaceIndex), zeta_surf_idx));
}

void check_correctness(float *out)
{
    float M[READ_LEN][HAP_LEN], I[READ_LEN][HAP_LEN], D[READ_LEN][HAP_LEN];
    // Validate the results
    for (int h = 0; h < OH; h++)
    for (int r = 0; r < OR; r++)
    for (int hh = 0; hh < HH; hh++)
    for (int rr = 0; rr < RR; rr++) {
        int total_r = r * RR + rr;
        int total_h = h * HH + hh;
        float golden = 0.0;
        for (int i = 0; i < READ_LEN; i++) {
            for (int j = 0; j < HAP_LEN; j++) {
                if (j == 0) {
                    M[i][0] = 0.0;
                    I[i][0] = 0.0;
                    D[i][0] = 0.0;
                }
                if (i == 0) {
                    M[0][j] = 0.0;
                    I[0][j] = 0.0;
                    D[0][j] = 1.0 / (HAP_LEN - 1);
                }
                if (j != 0 && i != 0) {
                    float alpha = (R[total_r][i] == H[j][total_h]) ? alpha_match[total_r][i] : alpha_gap[total_r][i];
                    float beta  = (R[total_r][i] == H[j][total_h]) ? beta_match[total_r][i] : beta_gap[total_r][i];
                    M[i][j] = alpha * M[i-1][j-1] +  beta * (I[i-1][j-1] + D[i-1][j-1]);
                    I[i][j] = delta[total_r][i] * M[i-1][j] + eta[total_r][i] * I[i-1][j];
                    D[i][j] = zeta[total_r][i] * M[i][j-1] + eta[total_r][i] * D[i][j-1];
                }
                if (i == READ_LEN - 1 && j > 0) {
                    golden += M[READ_LEN-1][j] + I[READ_LEN-1][j];
                }
            }
        }
        assert(abs(golden - out[total_h + NUM_HAPS*total_r]) < 1e-6);
    }
}

int main(int argc, char *argv[])
{
    // Creates a CmDevice from scratch.
    CmDevice *device = nullptr;
    unsigned int version = 0;
    cm_result_check(::CreateCmDevice(device, version));

    // Creates a CmProgram object consisting of the kernel loaded from the code buffer.
    CmProgram *program = nullptr;
    std::string isa_code = cm::util::isa::loadFile("pairhmm_genx.isa");
    cm_result_check(device->LoadProgram(const_cast<char*>(isa_code.data()), isa_code.size(), program));

    // Creates the cmNBody kernel.
    CmKernel *kernel = nullptr;
    cm_result_check(device->CreateKernel(program, "kernel_Hap", kernel));

    // Creates a task queue.
    CmQueue *cmd_queue = nullptr;
    cm_result_check(device->CreateQueue(cmd_queue));

    set_pseudo_input(device, kernel);
    CmSurface2D *out_surf = nullptr;
    SurfaceIndex *out_surf_idx = nullptr;
    cm_result_check(device->CreateSurface2D(NUM_HAPS, NUM_READS, CM_SURFACE_FORMAT_R32F, out_surf));
    cm_result_check(out_surf->GetIndex(out_surf_idx));
    cm_result_check(kernel->SetKernelArg(2, sizeof(SurfaceIndex), out_surf_idx));

    UINT64 kernel_ns = 0;
    for (size_t i = 0; i < ITER; i++) {
        // Creates a CmTask object.
        CmTask *task = nullptr;
        cm_result_check(device->CreateTask(task));
        cm_result_check(task->AddKernel(kernel));
        CmThreadGroupSpace *thread_group_space = nullptr;
        cm_result_check(device->CreateThreadGroupSpace(RR, 1, OH, OR, thread_group_space));

        UINT64 tmp_kern_time;
        CmEvent *sync_event = nullptr;
        device->InitPrintBuffer();
        cm_result_check(cmd_queue->EnqueueWithGroup(task, sync_event, thread_group_space));
        cm_result_check(sync_event->WaitForTaskFinished());
        cm_result_check(sync_event->GetExecutionTime( tmp_kern_time ));
        device->FlushPrintBuffer();
        kernel_ns += tmp_kern_time;
        if (ITER == 1) {
            float *out = (float*)malloc(sizeof(float) * NUM_HAPS * NUM_READS);
            cm_result_check(out_surf->ReadSurface((unsigned char *)out, sync_event));
            check_correctness(out);
            free(out);
        }
        cm_result_check(device->DestroyThreadGroupSpace(thread_group_space));
        cm_result_check(device->DestroyTask(task));
    }

    cm_result_check(::DestroyCmDevice(device));

    if (ITER == 1) {
        cout << "Pass!\n";
    } else {
        double number_ops = (double)NUM_READS * READ_LEN * NUM_HAPS * HAP_LEN;
        cout << "Length of read strings: " << NUM_READS << "*" << READ_LEN << "\n";
        cout << "Length of hap strings: " << NUM_HAPS << "*" << HAP_LEN << "\n";
        cout << "GCups: " << number_ops / (kernel_ns / ITER) << "\n";
    }

    return 0;
}
