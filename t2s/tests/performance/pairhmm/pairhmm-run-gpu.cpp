#define GPU     1

// Constant parameters (inner loop bounds) of the design
#include "const-parameters.h"
#define OR          16
#define OH          16

#define NUM_READS   (OR*RR)
#define NUM_HAPS    (OH*HH)

#include <math.h>
#include <assert.h>
#include "l0_rt_helpers.h"
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

void set_real_input(void)
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
}

void set_pseudo_input(void)
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

    srand(time(0));
    new_characters<HAP_LEN, NUM_HAPS>(H);
    new_characters<NUM_READS, READ_LEN>(R);
    new_probability<NUM_READS, READ_LEN>(delta);
    new_probability<NUM_READS, READ_LEN>(eta);
    new_probability<NUM_READS, READ_LEN>(zeta);
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
    // Find a driver instance with a GPU device
    auto [hDriver, hDevice, hContext] = findDriverAndDevice();
    auto hCommandList = createImmCommandList(hContext, hDevice);

    set_pseudo_input();
    ze_image_format_t fmt_uint = {ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_TYPE_UINT};
    auto H_surf           = createImage2D(hContext, hDevice, hCommandList, fmt_uint, NUM_HAPS, HAP_LEN, H);
    auto R_surf           = createImage2D(hContext, hDevice, hCommandList, fmt_uint, READ_LEN, NUM_READS, R);
    ze_image_format_t fmt_float = {ZE_IMAGE_FORMAT_LAYOUT_32, ZE_IMAGE_FORMAT_TYPE_FLOAT};
    auto delta_surf       = createImage2D(hContext, hDevice, hCommandList, fmt_float, READ_LEN, NUM_READS, delta);
    auto zeta_surf        = createImage2D(hContext, hDevice, hCommandList, fmt_float, READ_LEN, NUM_READS, zeta);
    auto eta_surf         = createImage2D(hContext, hDevice, hCommandList, fmt_float, READ_LEN, NUM_READS, eta);
    auto alpha_match_surf = createImage2D(hContext, hDevice, hCommandList, fmt_float, READ_LEN, NUM_READS, alpha_match);
    auto alpha_gap_surf   = createImage2D(hContext, hDevice, hCommandList, fmt_float, READ_LEN, NUM_READS, alpha_gap);
    auto beta_match_surf  = createImage2D(hContext, hDevice, hCommandList, fmt_float, READ_LEN, NUM_READS, beta_match);
    auto beta_gap_surf    = createImage2D(hContext, hDevice, hCommandList, fmt_float, READ_LEN, NUM_READS, beta_gap);
    auto out_surf         = createImage2D(hContext, hDevice, hCommandList, fmt_float, NUM_HAPS, NUM_READS);

    auto hKernel = createKernel(hContext, hDevice, "pairhmm_genx.bin", "kernel_Hap");
    setKernelArgs(hKernel, &H_surf, &R_surf, &out_surf,
                           &alpha_gap_surf, &alpha_match_surf, &beta_gap_surf, &beta_match_surf,
                           &delta_surf, &eta_surf, &zeta_surf);
    L0_SAFE_CALL(zeKernelSetGroupSize(hKernel, RR, 1, 1));

    double thost = 0;
    int iter = ITER == 1 ? 1 : ITER*100;
    for (size_t i = 0; i < iter; i++) {
        ze_event_handle_t hEvent = createEvent(hContext, hDevice);
        ze_group_count_t launchArgs = {OH, OR, 1};
        double host_start = getTimeStamp();
        appendLaunchKernel(hCommandList, hKernel, &launchArgs, hEvent);
        zeEventHostSynchronize(hEvent, std::numeric_limits<uint32_t>::max());
        double host_end = getTimeStamp();

        thost += (host_end - host_start);
        if (iter == 1) {
            float *out = (float*)malloc(sizeof(float) * NUM_HAPS * NUM_READS);
            copyToMemory(hCommandList, out, out_surf, hEvent);
            zeEventHostSynchronize(hEvent, std::numeric_limits<uint32_t>::max());
            check_correctness(out);
            free(out);
        }
    }

    destroy(hCommandList);
    destroy(hContext);

    if (iter == 1) {
        cout << "Pass!\n";
    } else {
        double ops = (double)NUM_READS * READ_LEN * NUM_HAPS * HAP_LEN / (1.0f*1000*1000*1000);
        cout << "Length of read strings: " << NUM_READS << "*" << READ_LEN << "\n";
        cout << "Length of hap strings: " << NUM_HAPS << "*" << HAP_LEN << "\n";
        cout << "GCups: " << ops / (thost / iter) << "\n";
    }

    return 0;
}
