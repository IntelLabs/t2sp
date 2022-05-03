#include "Halide.h"
#include "util.h"

// Constant parameters (inner loop bounds) of the design
#include "const-parameters.h"

using namespace Halide;

int main(void)
{
    // Macros: for convenient use.
    #define A                       jj,      ii,      hh,  rr,  j,   i,   h,  r
    #define A_ii_minus_1            jj,      ii-1,    hh,  rr,  j,   i,   h,  r
    #define A_jj_minus_1            jj-1,    ii,      hh,  rr,  j,   i,   h,  r
    #define A_ii_minus_1_jj_minus_1 jj-1,    ii-1,    hh,  rr,  j,   i,   h,  r
    #define A_last_ii               jj,      ii+II-1, hh,  rr,  j,   i-1, h,  r
    #define A_last_ii_jj_minus_1    jj-1,    ii+II-1, hh,  rr,  j,   i-1, h,  r
    #define A_last_jj               jj+JJ-1, ii,      hh,  rr,  j-1, i,   h,  r
    #define A_ii_minus_1_last_jj    jj+JJ-1, ii-1,    hh,  rr,  j-1, i,   h,  r
    #define A_last_ii_last_jj       jj+JJ-1, ii+II-1, hh,  rr,  j-1, i-1, h,  r

    // Linearized addresses
    #define total_i                 (i * II + ii)
    #define total_j                 (j * JJ + jj)
    #define total_r                 (r * RR + rr)
    #define total_h                 (h * HH + hh)
    #define FLOAT_URE_DECL          Float(32), {A}
    #define CHAR_URE_DECL           UInt(8), {A}

    #define OH  (H.dim(0).extent() / HH)
    #define OR  (R.dim(1).extent() / RR)

    // Hap data
    ImageParam H(UInt(8), 2);

    // Read data
    ImageParam R(UInt(8), 2);
    ImageParam delta(Float(32), 2);
    ImageParam zeta(Float(32), 2);
    ImageParam eta(Float(32), 2);
    ImageParam alpha_match(Float(32), 2);
    ImageParam alpha_gap(Float(32), 2);
    ImageParam beta_match(Float(32), 2);
    ImageParam beta_gap(Float(32), 2);

    // UREs for propogating inputs
    Var  A;
    URE Hap("Hap", CHAR_URE_DECL), Read("Read", CHAR_URE_DECL);
    URE Delta("Delta", FLOAT_URE_DECL), Zeta("Zeta", FLOAT_URE_DECL), Eta("Eta", FLOAT_URE_DECL);
    URE AlphaMatch("AlphaMatch", FLOAT_URE_DECL), AlphaGap("AlphaGap", FLOAT_URE_DECL);
    URE BetaMatch("BetaMatch", FLOAT_URE_DECL), BetaGap("BetaGap", FLOAT_URE_DECL);

    Hap(A)        = select(ii == 0, H(total_h, total_j), Hap(A_ii_minus_1));
    Read(A)       = select(jj == 0, R(total_i, total_r), Read(A_jj_minus_1));
    Delta(A)      = select(jj == 0, delta(total_i, total_r), Delta(A_jj_minus_1));
    Zeta(A)       = select(jj == 0, zeta(total_i, total_r), Zeta(A_jj_minus_1));
    Eta(A)        = select(jj == 0, eta(total_i, total_r), Eta(A_jj_minus_1));
    AlphaMatch(A) = select(jj == 0, alpha_match(total_i, total_r), AlphaMatch(A_jj_minus_1));
    AlphaGap(A)   = select(jj == 0, alpha_gap(total_i, total_r), AlphaGap(A_jj_minus_1));
    BetaMatch(A)  = select(jj == 0, beta_match(total_i, total_r), BetaMatch(A_jj_minus_1));
    BetaGap(A)    = select(jj == 0, beta_gap(total_i, total_r), BetaGap(A_jj_minus_1));

    // UREs for computations
    #define M_expr(x)    Alpha(A) * M(x) + Beta(A) * (I(x) + D(x))
    #define I_expr(x)    Delta(A) * M(x) + Eta(A) * I(x)
    #define D_expr(x)    Zeta(A)  * M(x) + Eta(A) * D(x)

    Expr i_is_0 = (ii == 0 && i == 0);
    Expr j_is_0 = (jj == 0 && j == 0);
    Expr i_is_last = (ii == II - 1 && i == OI - 1);
    Expr j_is_last = (jj == JJ - 1 && j == OJ - 1);

    URE Alpha("Alpha", FLOAT_URE_DECL), Beta("Beta", FLOAT_URE_DECL), Out("Out");
    URE M("M", FLOAT_URE_DECL), I("I", FLOAT_URE_DECL), D("D", FLOAT_URE_DECL), Sum("Sum", FLOAT_URE_DECL); 
    Alpha(A) = select(Read(A) == Hap(A), AlphaMatch(A), AlphaGap(A));
    Beta(A)  = select(Read(A) == Hap(A), BetaMatch(A), BetaGap(A));
    M(A)  = select(i_is_0 || j_is_0, 0.0f,
                select(ii == 0, select(jj == 0, M_expr(A_last_ii_last_jj),
                                                M_expr(A_last_ii_jj_minus_1)),
                                select(jj == 0, M_expr(A_ii_minus_1_last_jj),
                                                M_expr(A_ii_minus_1_jj_minus_1))));
    I(A)  = select(i_is_0 || j_is_0, 0.0f,
                select(ii == 0, I_expr(A_last_ii),
                                I_expr(A_ii_minus_1)));
    D(A)  = select(i_is_0, 1.0f / (HAP_LEN - 1),
                select(j_is_0, 0.0f, 
                    select(jj == 0, D_expr(A_last_jj),
                                    D_expr(A_jj_minus_1))));
    Sum(A) = select(i_is_last,
                select(j_is_0, 0.0f,
                    select(jj == 0, Sum(A_last_jj), Sum(A_jj_minus_1))) + M(A) + I(A), 0.0f);
    Out(hh, rr, h, r) = select(i_is_last && j_is_last, Sum(A));

    // Put all the UREs inside the same loop nest of Hap.
    Hap.merge_ures(Read, Delta, Zeta, Eta, AlphaMatch, AlphaGap, BetaMatch, BetaGap, Alpha, Beta, M, I, D, Sum, Out);

    // Explicitly set the loop bounds
    Hap.set_bounds(ii,  0, II,  jj, 0, JJ)
       .set_bounds(i,   0, OI,  j,  0, OJ)
       .set_bounds(rr,  0, RR,  hh, 0, HH)
       .set_bounds(r,   0, OR,  h,  0, OH);

    // Create a systolic array
#ifdef GPU
    Hap.space_time_transform(hh);
    Hap.gpu_blocks(h, r).gpu_threads(rr);
#else
    Hap.space_time_transform({ii, jj}, {0, 0});
#endif
    // I/O network
    Stensor DH("hLoader", DRAM), SH("hFeeder", SRAM), DR("rLoader", DRAM), SR("rFeeder", SRAM);
    Stensor DO("unloader", DRAM), O("deserializer");
    H >> DH >> FIFO(128)
      >> SH.scope(h).out(jj) >> FIFO(128);
    vector<ImageParam>{ R, delta, zeta, eta, alpha_match, alpha_gap, beta_match, beta_gap }
      >> DR >> FIFO(16)
      >> SR.scope(h).out(ii) >> FIFO(16);
    Out >> FIFO(128) >> DO >> O(total_h, total_r);

    // Compile the kernel to an FPGA bitstream, and expose a C interface for the host to invoke
#ifdef GPU
    O.compile_to_host("pairhmm-interface", { H, R, delta, zeta, eta, alpha_match, alpha_gap, beta_match, beta_gap }, "pairhmm", IntelGPU);
#else
    O.compile_to_host("pairhmm-interface", { H, R, delta, zeta, eta, alpha_match, alpha_gap, beta_match, beta_gap }, "pairhmm", IntelFPGA);
#endif
    cout << "Success!\n";
    return 0;
}
