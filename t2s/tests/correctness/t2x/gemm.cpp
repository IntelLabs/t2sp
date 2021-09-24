#include "Halide.h"
#include "util.h"

using namespace Halide;

#ifdef TINY
#define II          4
#define JJ          4
#define KK          4
#define III         4
#define JJJ         4
#define KKK         4
#define O_I         4
#define O_J         4
#define O_K         4
#elif S10
#define II          32
#define JJ          32
#define KK          32
#define III         12
#define JJJ         16
#define KKK         16
#define O_I         32
#define O_J         32
#define O_K         4
#else
#define II          32
#define JJ          32
#define KK          32
#define III         10
#define JJJ         8
#define KKK         16
#define O_I         32
#define O_J         32
#define O_K         4
#endif

int main()
{
    // Dependences
    #define P               kkk,      jjj,  iii,  jj, ii, kk,     k,  j,i
    #define P_kkk_minus_1   kkk-1,    jjj,  iii,  jj, ii, kk,     k,  j,i
    #define P_kk_minus_1    kkk+KKK-1,jjj,  iii,  jj, ii, kk-1,   k,  j,i
    #define P_k_minus_1     kkk+KKK-1,jjj,  iii,  jj, ii, kk+KK-1,k-1,j,i
    #define P_jjj_minus_1   kkk,      jjj-1,iii,  jj, ii, kk,     k,  j,i
    #define P_iii_minus_1   kkk,      jjj,  iii-1,jj, ii, kk,     k,  j,i
    #define P_Out                     jjj,  iii,  jj, ii,             j,i

    // Linearized addresses
    #define total_i         (iii + III * ii + III * II * i)
    #define total_j         (jjj + JJJ * jj + JJJ * JJ * j)
    #define total_k         (kkk + KKK * kk + KKK * KK * k)
    #define I               (A.dim(1).extent() / (III*II))
    #define J               (B.dim(0).extent() / (JJJ*JJ))
    #define K               (A.dim(0).extent() / (KKK*KK))

    // Type of the data to process in C and T2S
    #define CTYPE float
    #define TTYPE Float(32)

    // Inputs
    ImageParam A("A", TTYPE, 2), B("B", TTYPE, 2);

    // UREs
    Var kkk("kkk"), jjj("jjj"), iii("iii"), jj("jj"), ii("ii"), kk("kk"), k("k"), j("j"), i("i");
    URE X("X", TTYPE, {P}), Y("Y", TTYPE, {P}), Z("Z", TTYPE, {P}), Out("Out");
    X(P) = select(jjj == 0, A(total_k, total_i), X(P_jjj_minus_1));
    Y(P) = select(iii == 0, B(total_j, total_k), Y(P_iii_minus_1));
    Z(P) = select(kkk == 0 && kk == 0 && k == 0, 0,
                select(kkk == 0, select(kk == 0, Z(P_k_minus_1), Z(P_kk_minus_1)), Z(P_kkk_minus_1)))
                + X(P) * Y(P);
    Out(P_Out) = select(kkk == KKK-1 && kk == KK-1 && k == K-1, Z(P));

    // Put all the UREs inside the same loop nest of X.
    X.merge_ures(Y, Z, Out);

    // Explicitly set the loop bounds
    X.set_bounds(jjj, 0, JJJ, iii, 0, III, kkk, 0, KKK)
     .set_bounds(jj,  0, JJ,  ii,  0, II,  kk,  0, KK)
     .set_bounds(j,   0, J,   i,   0, I,   k,   0, K);

    // Create a systolic array
    X.space_time_transform(kkk, jjj, iii);

    // GPU can have many threads running in parallel.
    #ifdef GPU
    X.gpu_blocks(j, i).gpu_threads(jj, ii);
    #endif

    // I/O network
    Stensor DA("A_loader", DRAM), SA("A_feeder", SRAM);
    Stensor DB("B_loader", DRAM), SB("B_feeder", SRAM);
    Stensor DC("unloader", DRAM), RC("collector", REG), C("deserializer");
    A >> DA.width(kkk) >> SA.scope(k).banks(iii).width(kkk);
    B >> DB.width(kkk) >> SB.scope(k).banks(jjj).width(kkk);
    Out >> RC.scope(iii).banks(jjj).width(jjj) >> DC >> C;

    Buffer<CTYPE> a = new_data_2d<float, KKK*KK*O_K, III*II*O_I>(RANDOM);
    Buffer<CTYPE> b = new_data_2d<float, JJJ*JJ*O_J, KKK*KK*O_K>(RANDOM);
    Buffer<CTYPE> result(JJJ, III, JJ, II, O_J, O_I);
    A.set(a), B.set(b);
    C.realize(result, IntelFPGA);

    for (int i = 0; i < O_I; i++)
    for (int j = 0; j < O_J; j++)
    for (int ii = 0; ii < II; ii++)
    for (int jj = 0; jj < JJ; jj++)
    for (int iii = 0; iii < III; iii++)
    for (int jjj = 0; jjj < JJJ; jjj++) {
        float golden = 0.0f;
        for (int k = 0; k < O_K*KK*KKK; k++)
            golden += a(k, total_i) * b(total_j, k);
        assert(fabs(golden - result(P_Out)) < 0.005*fabs(golden));
    }
    printf("Success\n");
    return 0;
}
