/*OpenCL C x86-64-linux-avx-avx2-avx512-avx512_skylake-enable_synthesis-f16c-fma-intel_fpga-opencl-sse41*/
#pragma OPENCL FP_CONTRACT ON
#define float_from_bits(x) as_float(x)
inline float nan_f32() { return NAN; }
inline float neg_inf_f32() { return -INFINITY; }
inline float inf_f32() { return INFINITY; }
inline bool is_nan_f32(float x) {return isnan(x); }
inline bool is_inf_f32(float x) {return isinf(x); }
inline bool is_finite_f32(float x) {return isfinite(x); }
#define sqrt_f32 sqrt 
#define sin_f32 sin 
#define cos_f32 cos 
#define exp_f32 exp 
#define log_f32 log 
#define abs_f32 fabs 
#define floor_f32 floor 
#define ceil_f32 ceil 
#define round_f32 round 
#define trunc_f32 trunc 
#define pow_f32 pow
#define asin_f32 asin 
#define acos_f32 acos 
#define tan_f32 tan 
#define atan_f32 atan 
#define atan2_f32 atan2
#define sinh_f32 sinh 
#define asinh_f32 asinh 
#define cosh_f32 cosh 
#define acosh_f32 acosh 
#define tanh_f32 tanh 
#define atanh_f32 atanh 
#define fast_inverse_f32 native_recip 
#define fast_inverse_sqrt_f32 native_rsqrt 
#define __address_space___shared __local


// ll suffix in OpenCL is reserved for 128-bit integers.
#if defined __OPENCL_VERSION__
#define ADD_INT64_T_SUFFIX(x) x##l
#define ADD_UINT64_T_SUFFIX(x) x##ul
// HLSL doesn't have any suffixes.
#elif defined HLSL_VERSION
#define ADD_INT64_T_SUFFIX(x) x
#define ADD_UINT64_T_SUFFIX(x) x
#else
#define ADD_INT64_T_SUFFIX(x) x##ll
#define ADD_UINT64_T_SUFFIX(x) x##ull
#endif
#pragma OPENCL EXTENSION cl_intel_channels : enable
typedef union {
float __attribute__ ((aligned(128))) s[32];
struct {float s0,  s1,  s2,  s3,  s4,  s5,  s6,  s7,  s8,  s9,  sa,  sb,  sc,  sd,  se,  sf,  s16,  s17,  s18,  s19,  s20,  s21,  s22,  s23,  s24,  s25,  s26,  s27,  s28,  s29,  s30,  s31;};
} float32;
typedef union {
bool __attribute__ ((aligned(32))) s[32];
struct {bool s0,  s1,  s2,  s3,  s4,  s5,  s6,  s7,  s8,  s9,  sa,  sb,  sc,  sd,  se,  sf,  s16,  s17,  s18,  s19,  s20,  s21,  s22,  s23,  s24,  s25,  s26,  s27,  s28,  s29,  s30,  s31;};
} bool32;
typedef union {
int __attribute__ ((aligned(128))) s[32];
struct {int s0,  s1,  s2,  s3,  s4,  s5,  s6,  s7,  s8,  s9,  sa,  sb,  sc,  sd,  se,  sf,  s16,  s17,  s18,  s19,  s20,  s21,  s22,  s23,  s24,  s25,  s26,  s27,  s28,  s29,  s30,  s31;};
} int32;
typedef union { float16 v[2]; float s[32]; } _aLoader_channel_array_t;
channel _aLoader_channel_array_t _aLoader_channel __attribute__((depth(1024))) ;
typedef union { float16 v[2]; float s[32]; } _xLoader_channel_array_t;
channel _xLoader_channel_array_t _xLoader_channel __attribute__((depth(1024))) ;
typedef struct { float s[1]; } _V_channel_array_t;
channel _V_channel_array_t _V_channel __attribute__((depth(1024))) ;
channel float _yLoader_channel[1] __attribute__((depth(1024))) ;
channel float _Out_channel[1] __attribute__((depth(1024))) ;
// Address spaces for kernel_aLoader
#define __address_space__aSerializer_mem_channel __global
__kernel void kernel_aLoader(
 const int _A_extent_0,
 const int _A_extent_1,
 __address_space__aSerializer_mem_channel const float *restrict _aSerializer_mem_channel_1,
 __address_space__aSerializer_mem_channel const float *restrict _aSerializer_mem_channel_2)
{
 int _addr_temp;
 _addr_temp = 0;
 int _0 = _A_extent_1 >> 4;
 for (int _aLoader_s0_i = 0; _aLoader_s0_i < 0 + _0; _aLoader_s0_i++)
 {
  int _1 = _A_extent_0 + 511;
  int _2 = _1 >> 9;
  for (int _aLoader_s0_k = 0; _aLoader_s0_k < 0 + _2; _aLoader_s0_k++)
  {
   for (int _aLoader_s0_kk_ii = 0; _aLoader_s0_kk_ii < 0 + 256; _aLoader_s0_kk_ii++)
   {
    int _3 = _addr_temp;
    int _4 = _3 * 32;
    _aLoader_channel_array_t _temp;
    _temp.v[0] = vload16(0, (__address_space__aSerializer_mem_channel float*)(_aSerializer_mem_channel_1 + _4));
    _temp.v[1] = vload16(0, (__address_space__aSerializer_mem_channel float*)(_aSerializer_mem_channel_1 + _4));
    write_channel_intel(_aLoader_channel, _temp);
    (void)_temp;
    int _6 = _3 + 1;
    _addr_temp = _6;
   } // for _aLoader_s0_kk_ii
  } // for _aLoader_s0_k
 } // for _aLoader_s0_i
} // kernel kernel_aLoader
#undef __address_space__aSerializer_mem_channel
// Address spaces for kernel_xLoader
#define __address_space__xSerializer_mem_channel __global
__kernel void kernel_xLoader(
 const int _A_extent_0,
 const int _A_extent_1,
 __address_space__xSerializer_mem_channel const float *restrict _xSerializer_mem_channel_1,
 __address_space__xSerializer_mem_channel const float *restrict _xSerializer_mem_channel_2)
{
 int _addr_temp;
 _addr_temp = 0;
 int _7 = _A_extent_1 >> 4;
 for (int _xLoader_s0_i = 0; _xLoader_s0_i < 0 + _7; _xLoader_s0_i++)
 {
  int _8 = _A_extent_0 + 511;
  int _9 = _8 >> 9;
  for (int _xLoader_s0_k = 0; _xLoader_s0_k < 0 + _9; _xLoader_s0_k++)
  {
   for (int _xLoader_s0_kk_ii = 0; _xLoader_s0_kk_ii < 0 + 256; _xLoader_s0_kk_ii++)
   {
    int _10 = _addr_temp;
    int _11 = _10 * 32;
    _xLoader_channel_array_t _temp;
    _temp.v[0] = vload16(0, (__address_space__xSerializer_mem_channel float*)(_xSerializer_mem_channel_1 + _11));
    _temp.v[1] = vload16(0, (__address_space__xSerializer_mem_channel float*)(_xSerializer_mem_channel_2 + _11));
    write_channel_intel(_xLoader_channel, _temp);
    (void)_temp;
    int _13 = _10 + 1;
    _addr_temp = _13;
   } // for _xLoader_s0_kk_ii
  } // for _xLoader_s0_k
 } // for _xLoader_s0_i
} // kernel kernel_xLoader
#undef __address_space__xSerializer_mem_channel
// Address spaces for kernel_V
__kernel void kernel_V(
 const int _A_extent_0,
 const int _A_extent_1)
{
 _V_channel_array_t _V_channel_array;
 // produce uZ
 float _uZ_shreg[16];
 // produce uX
 float32 _uX_shreg;
 float _uZ_temp;
 // produce uA
 float32 _uA_shreg;
 int _14 = _A_extent_1 >> 4;
 for (int _uA_s0_i = 0; _uA_s0_i < 0 + _14; _uA_s0_i++)
 {
  int _15 = _A_extent_0 + 511;
  int _16 = _15 >> 9;
  for (int _uA_s0_k = 0; _uA_s0_k < 0 + _16; _uA_s0_k++)
  {
   for (int _uA_s0_kk_ii = 0; _uA_s0_kk_ii < 0 + 256; _uA_s0_kk_ii++)
   {
    float _18 = _uZ_shreg[15];
    _uZ_temp = _18;
    #pragma unroll
    for (int _dummy_s0_l1 = 0; _dummy_s0_l1 < 0 + 15; _dummy_s0_l1++)
    {
     int _19 = 15 - _dummy_s0_l1;
     int _20 = 14 - _dummy_s0_l1;
     float _22 = _uZ_shreg[_20];
     _uZ_shreg[_19] = _22;
     (void)_22;
    } // for _dummy_s0_l1
    float _23 = _uZ_temp;
    _uZ_shreg[0] = _23;
    (void)_23;
    _aLoader_channel_array_t __24 = read_channel_intel(_aLoader_channel);
    _uA_shreg.s0 = __24.s[0];
    _uA_shreg.s1 = __24.s[1];
    _uA_shreg.s2 = __24.s[2];
    _uA_shreg.s3 = __24.s[3];
    _uA_shreg.s4 = __24.s[4];
    _uA_shreg.s5 = __24.s[5];
    _uA_shreg.s6 = __24.s[6];
    _uA_shreg.s7 = __24.s[7];
    _uA_shreg.s8 = __24.s[8];
    _uA_shreg.s9 = __24.s[9];
    _uA_shreg.sa = __24.s[10];
    _uA_shreg.sb = __24.s[11];
    _uA_shreg.sc = __24.s[12];
    _uA_shreg.sd = __24.s[13];
    _uA_shreg.se = __24.s[14];
    _uA_shreg.sf = __24.s[15];
    _uA_shreg.s16 = __24.s[16];
    _uA_shreg.s17 = __24.s[17];
    _uA_shreg.s18 = __24.s[18];
    _uA_shreg.s19 = __24.s[19];
    _uA_shreg.s20 = __24.s[20];
    _uA_shreg.s21 = __24.s[21];
    _uA_shreg.s22 = __24.s[22];
    _uA_shreg.s23 = __24.s[23];
    _uA_shreg.s24 = __24.s[24];
    _uA_shreg.s25 = __24.s[25];
    _uA_shreg.s26 = __24.s[26];
    _uA_shreg.s27 = __24.s[27];
    _uA_shreg.s28 = __24.s[28];
    _uA_shreg.s29 = __24.s[29];
    _uA_shreg.s30 = __24.s[30];
    _uA_shreg.s31 = __24.s[31];
    (void)__24;
    _xLoader_channel_array_t __25 = read_channel_intel(_xLoader_channel);
    _uX_shreg.s0 = __25.s[0];
    _uX_shreg.s1 = __25.s[1];
    _uX_shreg.s2 = __25.s[2];
    _uX_shreg.s3 = __25.s[3];
    _uX_shreg.s4 = __25.s[4];
    _uX_shreg.s5 = __25.s[5];
    _uX_shreg.s6 = __25.s[6];
    _uX_shreg.s7 = __25.s[7];
    _uX_shreg.s8 = __25.s[8];
    _uX_shreg.s9 = __25.s[9];
    _uX_shreg.sa = __25.s[10];
    _uX_shreg.sb = __25.s[11];
    _uX_shreg.sc = __25.s[12];
    _uX_shreg.sd = __25.s[13];
    _uX_shreg.se = __25.s[14];
    _uX_shreg.sf = __25.s[15];
    _uX_shreg.s16 = __25.s[16];
    _uX_shreg.s17 = __25.s[17];
    _uX_shreg.s18 = __25.s[18];
    _uX_shreg.s19 = __25.s[19];
    _uX_shreg.s20 = __25.s[20];
    _uX_shreg.s21 = __25.s[21];
    _uX_shreg.s22 = __25.s[22];
    _uX_shreg.s23 = __25.s[23];
    _uX_shreg.s24 = __25.s[24];
    _uX_shreg.s25 = __25.s[25];
    _uX_shreg.s26 = __25.s[26];
    _uX_shreg.s27 = __25.s[27];
    _uX_shreg.s28 = __25.s[28];
    _uX_shreg.s29 = __25.s[29];
    _uX_shreg.s30 = __25.s[30];
    _uX_shreg.s31 = __25.s[31];
    (void)__25;
    bool _V_channel_temp;
    _V_channel_temp = 0;
    #pragma unroll
    for (int _uA_s0_kkk = 0; _uA_s0_kkk < 0 + 32; _uA_s0_kkk++)
    {
     float _26;
     int _27 = _uA_s0_k * 512;
     int _28 = _uA_s0_kk_ii >> 4;
     int _29 = _28 * 32;
     int _30 = _29 + _uA_s0_kkk;
     int _31 = _27 + _30;
     bool _32 = _31 == 0;
     if (_32)
     {
      float _33 = float_from_bits(0 /* 0 */);
      _26 = _33;
     } // if _32
     else
     {
      float _35 = _uZ_shreg[0];
      _26 = _35;
     } // if _32 else
     float _36 = _26;
     float _38 = _uA_shreg.s[_uA_s0_kkk];
     float _40 = _uX_shreg.s[_uA_s0_kkk];
     float _41 = _38 * _40;
     float _42 = _36 + _41;
     _uZ_shreg[0] = _42;
     (void)_42;
    } // for _uA_s0_kkk
    bool _43 = _V_channel_temp;
    if (_43)
    {
     write_channel_intel(_V_channel, _V_channel_array);
     (void)_V_channel_array;
    } // if _43
    _V_channel_temp = 0;
    #pragma unroll
    for (int _uA_s0_kkk = 0; _uA_s0_kkk < 0 + 32; _uA_s0_kkk++)
    {
     bool _44 = _uA_s0_kkk == 31;
     int _45 = _uA_s0_kk_ii >> 4;
     bool _46 = _45 == 15;
     bool _47 = _44 && _46;
     int _48 = _A_extent_0 + -1;
     int _49 = _48 >> 9;
     bool _50 = _uA_s0_k == _49;
     bool _51 = _47 && _50;
     if (_51)
     {
      float _53 = _uZ_shreg[0];
      _V_channel_array.s[0] = _53;
      (void)0;
      _V_channel_temp = 1;
     } // if _51
    } // for _uA_s0_kkk
    bool _54 = _V_channel_temp;
    if (_54)
    {
     write_channel_intel(_V_channel, _V_channel_array);
     (void)_V_channel_array;
    } // if _54
   } // for _uA_s0_kk_ii
  } // for _uA_s0_k
 } // for _uA_s0_i
} // kernel kernel_V
// Address spaces for kernel_yLoader
#define __address_space__ySerializer_mem_channel __global
__kernel void kernel_yLoader(
 const int _A_extent_1,
 __address_space__ySerializer_mem_channel const float *restrict _ySerializer_mem_channel)
{
 int _addr_temp;
 _addr_temp = 0;
 int _55 = _A_extent_1 >> 4;
 for (int _yLoader_s0_i = 0; _yLoader_s0_i < 0 + _55; _yLoader_s0_i++)
 {
  for (int _yLoader_s0_ii = 0; _yLoader_s0_ii < 0 + 16; _yLoader_s0_ii++)
  {
   int _56 = _addr_temp;
   float _57 = _ySerializer_mem_channel[_56];
   write_channel_intel(_yLoader_channel[0], _57);
   (void)_57;
   int _58 = _56 + 1;
   _addr_temp = _58;
  } // for _yLoader_s0_ii
 } // for _yLoader_s0_i
} // kernel kernel_yLoader
#undef __address_space__ySerializer_mem_channel
// Address spaces for kernel_Out
__kernel void kernel_Out(
 const int _A_extent_1,
 const float _Alpha,
 const float _Beta)
{
 _V_channel_array_t _V_channel_array;
 int _59 = _A_extent_1 >> 4;
 for (int _Out_s0_i = 0; _Out_s0_i < 0 + _59; _Out_s0_i++)
 {
  for (int _Out_s0_ii = 0; _Out_s0_ii < 0 + 16; _Out_s0_ii++)
  {
   _V_channel_array_t __60 = read_channel_intel(_V_channel);
   _V_channel_array = __60;
   (void)__60;
   float __61 = _V_channel_array.s[0];
   float _62 = __61 * _Alpha;
   float __63 = read_channel_intel(_yLoader_channel[0]);
   float _64 = __63 * _Beta;
   float _65 = _62 + _64;
   write_channel_intel(_Out_channel[0], _65);
   (void)_65;
  } // for _Out_s0_ii
 } // for _Out_s0_i
} // kernel kernel_Out
// Address spaces for kernel_unloader
#define __address_space__unloader_mem_channel __global
__kernel void kernel_unloader(
 const int _A_extent_1,
 __address_space__unloader_mem_channel float *restrict _unloader_mem_channel)
{
 int _addr_temp;
 _addr_temp = 0;
 int _66 = _A_extent_1 >> 4;
 for (int _unloader_s0_i = 0; _unloader_s0_i < 0 + _66; _unloader_s0_i++)
 {
  for (int _unloader_s0_ii = 0; _unloader_s0_ii < 0 + 16; _unloader_s0_ii++)
  {
   float __67 = read_channel_intel(_Out_channel[0]);
   int _68 = _addr_temp;
   _unloader_mem_channel[_68] = __67;
   int _69 = _addr_temp;
   int _70 = _69 + 1;
   _addr_temp = _70;
  } // for _unloader_s0_ii
 } // for _unloader_s0_i
} // kernel kernel_unloader
#undef __address_space__unloader_mem_channel

