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
channel float _yLoader_channel[4] __attribute__((depth(0))) ;
channel float _aLoader_channel[4] __attribute__((depth(0))) ;
channel float _xLoader2_channel[4] __attribute__((depth(0))) ;
channel float _xLoader1_channel[4] __attribute__((depth(0))) ;
channel float _Out_channel __attribute__((depth(0))) ;
// Address spaces for kernel_yLoader
#define __address_space__ySerializer_mem_channel __global
__kernel void kernel_yLoader(
 const int _A_extent_0,
 __address_space__ySerializer_mem_channel const float *restrict _ySerializer_mem_channel)
{
 int _addr_temp;
 _addr_temp = 0;
 int _0 = _A_extent_0 >> 3;
 for (int _yLoader_s0_k = 0; _yLoader_s0_k < 0 + _0; _yLoader_s0_k++)
 {
  for (int _yLoader_s0_kk_kkk = 0; _yLoader_s0_kk_kkk < 0 + 8; _yLoader_s0_kk_kkk++)
  {
   int _1 = _addr_temp;
   float _2 = _ySerializer_mem_channel[_1];
   write_channel_intel(_yLoader_channel[3], _2);
   (void)_2;
   int _3 = _1 + 1;
   _addr_temp = _3;
  } // for _yLoader_s0_kk_kkk
 } // for _yLoader_s0_k
} // kernel kernel_yLoader
#undef __address_space__ySerializer_mem_channel
// Address spaces for kernel_aLoader
#define __address_space__aSerializer __global
__kernel void kernel_aLoader(
 const int _A_extent_0,
 __address_space__aSerializer const float *restrict _aSerializer)
{
 int _4 = _A_extent_0 >> 3;
 for (int _aLoader_s0_k = 0; _aLoader_s0_k < 0 + _4; _aLoader_s0_k++)
 {
  for (int _aLoader_s0_kk_ii_kkk = 0; _aLoader_s0_kk_ii_kkk < 0 + 32; _aLoader_s0_kk_ii_kkk++)
  {
   int _5 = _aLoader_s0_k * 32;
   int _6 = _5 + _aLoader_s0_kk_ii_kkk;
   float _7 = _aSerializer[_6];
   write_channel_intel(_aLoader_channel[0], _7);
   (void)_7;
  } // for _aLoader_s0_kk_ii_kkk
 } // for _aLoader_s0_k
 int _8 = _A_extent_0 >> 3;
 for (int _aLoader_s0_k = 0; _aLoader_s0_k < 0 + _8; _aLoader_s0_k++)
 {
  for (int _aLoader_s0_kk_ii_kkk = 0; _aLoader_s0_kk_ii_kkk < 0 + 32; _aLoader_s0_kk_ii_kkk++)
  {
   int _9 = _A_extent_0 >> 3;
   int _10 = _9 * 32;
   int _11 = _aLoader_s0_k * 32;
   int _12 = _11 + _aLoader_s0_kk_ii_kkk;
   int _13 = _10 + _12;
   float _14 = _aSerializer[_13];
   write_channel_intel(_aLoader_channel[1], _14);
   (void)_14;
  } // for _aLoader_s0_kk_ii_kkk
 } // for _aLoader_s0_k
 int _15 = _A_extent_0 >> 3;
 for (int _aLoader_s0_k = 0; _aLoader_s0_k < 0 + _15; _aLoader_s0_k++)
 {
  for (int _aLoader_s0_kk_ii_kkk = 0; _aLoader_s0_kk_ii_kkk < 0 + 32; _aLoader_s0_kk_ii_kkk++)
  {
   int _16 = _A_extent_0 >> 3;
   int _17 = _16 * 64;
   int _18 = _aLoader_s0_k * 32;
   int _19 = _18 + _aLoader_s0_kk_ii_kkk;
   int _20 = _17 + _19;
   float _21 = _aSerializer[_20];
   write_channel_intel(_aLoader_channel[2], _21);
   (void)_21;
  } // for _aLoader_s0_kk_ii_kkk
 } // for _aLoader_s0_k
 int _22 = _A_extent_0 >> 3;
 for (int _aLoader_s0_k = 0; _aLoader_s0_k < 0 + _22; _aLoader_s0_k++)
 {
  for (int _aLoader_s0_kk_ii_kkk = 0; _aLoader_s0_kk_ii_kkk < 0 + 32; _aLoader_s0_kk_ii_kkk++)
  {
   int _23 = _A_extent_0 >> 3;
   int _24 = _23 * 96;
   int _25 = _aLoader_s0_k * 32;
   int _26 = _25 + _aLoader_s0_kk_ii_kkk;
   int _27 = _24 + _26;
   float _28 = _aSerializer[_27];
   write_channel_intel(_aLoader_channel[3], _28);
   (void)_28;
  } // for _aLoader_s0_kk_ii_kkk
 } // for _aLoader_s0_k
} // kernel kernel_aLoader
#undef __address_space__aSerializer
// Address spaces for kernel_xLoader2
#define __address_space__xSerializer2 __global
__kernel void kernel_xLoader2(
 const int _A_extent_0,
 __address_space__xSerializer2 const float *restrict _xSerializer2)
{
 int _29 = _A_extent_0 >> 3;
 for (int _xLoader2_s0_k = 0; _xLoader2_s0_k < 0 + _29; _xLoader2_s0_k++)
 {
  for (int _xLoader2_s0_kk_kkk = 0; _xLoader2_s0_kk_kkk < 0 + 8; _xLoader2_s0_kk_kkk++)
  {
   int _30 = _xLoader2_s0_k * 32;
   int _31 = _xLoader2_s0_kk_kkk >> 2;
   int _32 = _31 * 16;
   int _33 = _xLoader2_s0_kk_kkk & 3;
   int _34 = _32 + _33;
   int _35 = _30 + _34;
   float _36 = _xSerializer2[_35];
   write_channel_intel(_xLoader2_channel[0], _36);
   (void)_36;
  } // for _xLoader2_s0_kk_kkk
 } // for _xLoader2_s0_k
 int _37 = _A_extent_0 >> 3;
 for (int _xLoader2_s0_k = 0; _xLoader2_s0_k < 0 + _37; _xLoader2_s0_k++)
 {
  for (int _xLoader2_s0_kk_kkk = 0; _xLoader2_s0_kk_kkk < 0 + 8; _xLoader2_s0_kk_kkk++)
  {
   int _38 = _A_extent_0 >> 3;
   int _39 = _38 * 32;
   int _40 = _xLoader2_s0_k * 32;
   int _41 = _xLoader2_s0_kk_kkk >> 2;
   int _42 = _41 * 16;
   int _43 = _xLoader2_s0_kk_kkk & 3;
   int _44 = _42 + _43;
   int _45 = _40 + _44;
   int _46 = _39 + _45;
   float _47 = _xSerializer2[_46];
   write_channel_intel(_xLoader2_channel[1], _47);
   (void)_47;
  } // for _xLoader2_s0_kk_kkk
 } // for _xLoader2_s0_k
 int _48 = _A_extent_0 >> 3;
 for (int _xLoader2_s0_k = 0; _xLoader2_s0_k < 0 + _48; _xLoader2_s0_k++)
 {
  for (int _xLoader2_s0_kk_kkk = 0; _xLoader2_s0_kk_kkk < 0 + 8; _xLoader2_s0_kk_kkk++)
  {
   int _49 = _A_extent_0 >> 3;
   int _50 = _49 * 64;
   int _51 = _xLoader2_s0_k * 32;
   int _52 = _xLoader2_s0_kk_kkk >> 2;
   int _53 = _52 * 16;
   int _54 = _xLoader2_s0_kk_kkk & 3;
   int _55 = _53 + _54;
   int _56 = _51 + _55;
   int _57 = _50 + _56;
   float _58 = _xSerializer2[_57];
   write_channel_intel(_xLoader2_channel[2], _58);
   (void)_58;
  } // for _xLoader2_s0_kk_kkk
 } // for _xLoader2_s0_k
 int _59 = _A_extent_0 >> 3;
 for (int _xLoader2_s0_k = 0; _xLoader2_s0_k < 0 + _59; _xLoader2_s0_k++)
 {
  for (int _xLoader2_s0_kk_kkk = 0; _xLoader2_s0_kk_kkk < 0 + 8; _xLoader2_s0_kk_kkk++)
  {
   int _60 = _A_extent_0 >> 3;
   int _61 = _60 * 96;
   int _62 = _xLoader2_s0_k * 32;
   int _63 = _xLoader2_s0_kk_kkk >> 2;
   int _64 = _63 * 16;
   int _65 = _xLoader2_s0_kk_kkk & 3;
   int _66 = _64 + _65;
   int _67 = _62 + _66;
   int _68 = _61 + _67;
   float _69 = _xSerializer2[_68];
   write_channel_intel(_xLoader2_channel[3], _69);
   (void)_69;
  } // for _xLoader2_s0_kk_kkk
 } // for _xLoader2_s0_k
} // kernel kernel_xLoader2
#undef __address_space__xSerializer2
// Address spaces for kernel_xLoader1
#define __address_space__xSerializer1 __global
__kernel void kernel_xLoader1(
 const int _A_extent_0,
 __address_space__xSerializer1 const float *restrict _xSerializer1)
{
 int _70 = _A_extent_0 >> 3;
 for (int _xLoader1_s0_k = 0; _xLoader1_s0_k < 0 + _70; _xLoader1_s0_k++)
 {
  for (int _xLoader1_s0_kk_ii = 0; _xLoader1_s0_kk_ii < 0 + 8; _xLoader1_s0_kk_ii++)
  {
   int _71 = _xLoader1_s0_k * 8;
   int _72 = _71 + _xLoader1_s0_kk_ii;
   int _73 = _72 * 4;
   float _74 = _xSerializer1[_73];
   write_channel_intel(_xLoader1_channel[0], _74);
   (void)_74;
  } // for _xLoader1_s0_kk_ii
 } // for _xLoader1_s0_k
 int _75 = _A_extent_0 >> 3;
 for (int _xLoader1_s0_k = 0; _xLoader1_s0_k < 0 + _75; _xLoader1_s0_k++)
 {
  for (int _xLoader1_s0_kk_ii = 0; _xLoader1_s0_kk_ii < 0 + 8; _xLoader1_s0_kk_ii++)
  {
   int _76 = _A_extent_0 >> 3;
   int _77 = _76 * 8;
   int _78 = _xLoader1_s0_k * 8;
   int _79 = _78 + _xLoader1_s0_kk_ii;
   int _80 = _77 + _79;
   int _81 = _80 * 4;
   float _82 = _xSerializer1[_81];
   write_channel_intel(_xLoader1_channel[1], _82);
   (void)_82;
  } // for _xLoader1_s0_kk_ii
 } // for _xLoader1_s0_k
 int _83 = _A_extent_0 >> 3;
 for (int _xLoader1_s0_k = 0; _xLoader1_s0_k < 0 + _83; _xLoader1_s0_k++)
 {
  for (int _xLoader1_s0_kk_ii = 0; _xLoader1_s0_kk_ii < 0 + 8; _xLoader1_s0_kk_ii++)
  {
   int _84 = _A_extent_0 >> 3;
   int _85 = _84 * 16;
   int _86 = _xLoader1_s0_k * 8;
   int _87 = _86 + _xLoader1_s0_kk_ii;
   int _88 = _85 + _87;
   int _89 = _88 * 4;
   float _90 = _xSerializer1[_89];
   write_channel_intel(_xLoader1_channel[2], _90);
   (void)_90;
  } // for _xLoader1_s0_kk_ii
 } // for _xLoader1_s0_k
 int _91 = _A_extent_0 >> 3;
 for (int _xLoader1_s0_k = 0; _xLoader1_s0_k < 0 + _91; _xLoader1_s0_k++)
 {
  for (int _xLoader1_s0_kk_ii = 0; _xLoader1_s0_kk_ii < 0 + 8; _xLoader1_s0_kk_ii++)
  {
   int _92 = _A_extent_0 >> 3;
   int _93 = _92 * 24;
   int _94 = _xLoader1_s0_k * 8;
   int _95 = _94 + _xLoader1_s0_kk_ii;
   int _96 = _93 + _95;
   int _97 = _96 * 4;
   float _98 = _xSerializer1[_97];
   write_channel_intel(_xLoader1_channel[3], _98);
   (void)_98;
  } // for _xLoader1_s0_kk_ii
 } // for _xLoader1_s0_k
} // kernel kernel_xLoader1
#undef __address_space__xSerializer1
// Address spaces for kernel_Out
__kernel void kernel_Out(
 const int _A_extent_0,
 const float _Alpha,
 const float _Beta)
{
 // produce uZTop2
 float _uZTop2_shreg[16];
 // produce uZTop
 float _uZTop_shreg[4][4];
 // produce uZBot
 float _uZBot_shreg[4][4];
 // produce uXTop
 float _uXTop_shreg[4];
 // produce uXBot
 float _uXBot_shreg[4][4];
 // produce uA
 float _uA_shreg[4];
 int _99 = _A_extent_0 >> 3;
 for (int _uA_s0_k = 0; _uA_s0_k < 0 + _99; _uA_s0_k++)
 {
  for (int _uA_s0_kk_ii_kkk = 0; _uA_s0_kk_ii_kkk < 0 + 32; _uA_s0_kk_ii_kkk++)
  {
   float __100 = read_channel_intel(_aLoader_channel[0]);
   _uA_shreg[0] = __100;
   (void)__100;
   float _101;
   int _102 = _uA_s0_kk_ii_kkk & 15;
   int _103 = _102 >> 2;
   bool _104 = _103 == 0;
   if (_104)
   {
    float __105 = read_channel_intel(_xLoader2_channel[0]);
    _101 = __105;
   } // if _104
   else
   {
    float _107 = _uXBot_shreg[_uA_s0_kk_ii_kkk & 3][0];
    _101 = _107;
   } // if _104 else
   float _108 = _101;
   _uXBot_shreg[_uA_s0_kk_ii_kkk & 3][0] = _108;
   (void)_108;
   float _109;
   int _110 = _uA_s0_kk_ii_kkk & 3;
   bool _111 = _110 == 0;
   if (_111)
   {
    float __112 = read_channel_intel(_xLoader1_channel[0]);
    _109 = __112;
   } // if _111
   else
   {
    float _114 = _uXTop_shreg[0];
    _109 = _114;
   } // if _111 else
   float _115 = _109;
   _uXTop_shreg[0] = _115;
   (void)_115;
   float _116;
   int _117 = _uA_s0_kk_ii_kkk & 15;
   int _118 = _117 >> 2;
   bool _119 = _118 == 0;
   if (_119)
   {
    float _120 = float_from_bits(0 /* 0 */);
    _116 = _120;
   } // if _119
   else
   {
    float _121;
    int _122 = _uA_s0_kk_ii_kkk & 3;
    bool _123 = _122 == 0;
    float _127 = _uZBot_shreg[(_uA_s0_kk_ii_kkk & 12) / 4][0];
    _121 = _127;
    float _128 = _121;
    _116 = _128;
   } // if _119 else
   float _129 = _116;
   float _130;
   int _131 = _uA_s0_kk_ii_kkk & 15;
   int _132 = _131 >> 2;
   int _133 = _uA_s0_k * 8;
   int _134 = _uA_s0_kk_ii_kkk >> 4;
   int _135 = _134 * 4;
   int _136 = _uA_s0_kk_ii_kkk & 3;
   int _137 = _135 + _136;
   int _138 = _133 + _137;
   bool _139 = _132 < _138;
   float _142 = _uA_shreg[0];
   float _144 = _uXBot_shreg[_uA_s0_kk_ii_kkk & 3][0];
   if (_139)
   {
    float _140 = float_from_bits(0 /* 0 */);
    _130 = _140;
   } // if _139
   else
   {
    float _145 = _142 * _144;
    _130 = _145;
   } // if _139 else
   float _146 = _130;
   float _147 = _129 + _146;
   //printf("i=%d k=%d a=%f x=%f z=%f->%f\n", 0 * 4 + (_uA_s0_kk_ii_kkk & 12) / 4, _uA_s0_k * 8 + (_uA_s0_kk_ii_kkk >> 4) * 4 + (_uA_s0_kk_ii_kkk & 3), _142, _144, _129, _147);
   _uZBot_shreg[(_uA_s0_kk_ii_kkk & 12) / 4][0] = _147;
   (void)_147;
   float _148;
   int _149 = _uA_s0_kk_ii_kkk & 15;
   int _150 = _149 >> 2;
   int _151 = _uA_s0_k * 8;
   int _152 = _uA_s0_kk_ii_kkk >> 4;
   int _153 = _152 * 4;
   int _154 = _uA_s0_kk_ii_kkk & 3;
   int _155 = _153 + _154;
   int _156 = _151 + _155;
   bool _157 = _150 == _156;
   if (_157)
   {
    float _159 = _uZBot_shreg[(_uA_s0_kk_ii_kkk & 12) / 4][0];
    _148 = _159;
   } // if _157
   else
   {
    float _160;
    int _161 = _uA_s0_kk_ii_kkk & 15;
    int _162 = _161 >> 2;
    bool _163 = _162 == 0;
    if (_163)
    {
     float _164 = float_from_bits(0 /* 0 */);
     _160 = _164;
    } // if _163
    else
    {
     float _166 = _uZTop_shreg[_uA_s0_kk_ii_kkk & 3][0];
     _160 = _166;
    } // if _163 else
    float _167 = _160;
    float _169 = _uA_shreg[0];
    float _171 = _uXTop_shreg[0];
    float _172 = _169 * _171;
    float _173 = _167 + _172;
    //printf("i=%d k=%d a=%f x=%f z=%f->%f\n", 0 * 4 + (_uA_s0_kk_ii_kkk & 12) / 4, _uA_s0_k * 8 + (_uA_s0_kk_ii_kkk >> 4) * 4 + (_uA_s0_kk_ii_kkk & 3), _169, _171, _167, _173);
    _148 = _173;
   } // if _157 else
   float _174 = _148;
   _uZTop_shreg[_uA_s0_kk_ii_kkk & 3][0] = _174;
   (void)_174;
   float _175;
   int _176 = _uA_s0_kk_ii_kkk & 15;
   int _177 = _176 >> 2;
   bool _178 = _177 == 3;
   if (_178)
   {
    float _180 = _uZTop_shreg[_uA_s0_kk_ii_kkk & 3][0];
    _175 = _180;
   } // if _178
   else
   {
    float _181 = float_from_bits(0 /* 0 */);
    _175 = _181;
   } // if _178 else
   float _182 = _175;
   if (0 * 4 + (_uA_s0_kk_ii_kkk & 12) / 4 >= _uA_s0_k * 8 + (_uA_s0_kk_ii_kkk >> 4) * 4 + (_uA_s0_kk_ii_kkk & 3)) {
    _uZTop2_shreg[_uA_s0_k * 8 + (_uA_s0_kk_ii_kkk >> 4) * 4 + (_uA_s0_kk_ii_kkk & 3)] = _182;
    printf("i=%d k=%d z=%f\n", 0 * 4 + (_uA_s0_kk_ii_kkk & 12) / 4, _uA_s0_k * 8 + (_uA_s0_kk_ii_kkk >> 4) * 4 + (_uA_s0_kk_ii_kkk & 3), _182);
   }
   (void)_182;
  } // for _uA_s0_kk_ii_kkk
 } // for _uA_s0_k
 int _183 = _A_extent_0 >> 3;
 for (int _uA_s0_k = 0; _uA_s0_k < 0 + _183; _uA_s0_k++)
 {
  for (int _uA_s0_kk_ii_kkk = 0; _uA_s0_kk_ii_kkk < 0 + 32; _uA_s0_kk_ii_kkk++)
  {
   float __184 = read_channel_intel(_aLoader_channel[1]);
   _uA_shreg[1] = __184;
   (void)__184;
   float _185;
   int _186 = _uA_s0_kk_ii_kkk & 15;
   int _187 = _186 >> 2;
   bool _188 = _187 == 0;
   if (_188)
   {
    float __189 = read_channel_intel(_xLoader2_channel[1]);
    _185 = __189;
   } // if _188
   else
   {
    float _191 = _uXBot_shreg[_uA_s0_kk_ii_kkk & 3][1];
    _185 = _191;
   } // if _188 else
   float _192 = _185;
   _uXBot_shreg[_uA_s0_kk_ii_kkk & 3][1] = _192;
   (void)_192;
   float _193;
   int _194 = _uA_s0_kk_ii_kkk & 3;
   bool _195 = _194 == 0;
   if (_195)
   {
    float __196 = read_channel_intel(_xLoader1_channel[1]);
    _193 = __196;
   } // if _195
   else
   {
    float _198 = _uXTop_shreg[1];
    _193 = _198;
   } // if _195 else
   float _199 = _193;
   _uXTop_shreg[1] = _199;
   (void)_199;
   float _200;
   int _201 = _uA_s0_kk_ii_kkk & 3;
   bool _202 = _201 == 0;
   float _206 = _uZBot_shreg[(_uA_s0_kk_ii_kkk & 12) / 4][1];
   _200 = _206;
   float _207 = _200;
   float _208;
   int _209 = _uA_s0_kk_ii_kkk & 15;
   int _210 = _209 >> 2;
   int _211 = _uA_s0_k * 8;
   int _212 = _uA_s0_kk_ii_kkk >> 4;
   int _213 = _212 * 4;
   int _214 = _uA_s0_kk_ii_kkk & 3;
   int _215 = _213 + _214;
   int _216 = _211 + _215;
   int _217 = _216 + -4;
   bool _218 = _210 < _217;
   float _221 = _uA_shreg[1];
   float _223 = _uXBot_shreg[_uA_s0_kk_ii_kkk & 3][1];
   if (_218)
   {
    float _219 = float_from_bits(0 /* 0 */);
    _208 = _219;
   } // if _218
   else
   {
    float _224 = _221 * _223;
    _208 = _224;
   } // if _218 else
   float _225 = _208;
   float _226 = _207 + _225;
   //printf("i=%d k=%d a=%f x=%f z=%f->%f\n", 1 * 4 + (_uA_s0_kk_ii_kkk & 12) / 4, _uA_s0_k * 8 + (_uA_s0_kk_ii_kkk >> 4) * 4 + (_uA_s0_kk_ii_kkk & 3), _221, _223, _207, _226);
   _uZBot_shreg[(_uA_s0_kk_ii_kkk & 12) / 4][1] = _226;
   (void)_226;
   float _227;
   int _228 = _uA_s0_kk_ii_kkk & 15;
   int _229 = _228 >> 2;
   int _230 = _uA_s0_k * 8;
   int _231 = _uA_s0_kk_ii_kkk >> 4;
   int _232 = _231 * 4;
   int _233 = _uA_s0_kk_ii_kkk & 3;
   int _234 = _232 + _233;
   int _235 = _230 + _234;
   int _236 = _235 + -4;
   bool _237 = _229 == _236;
   if (_237)
   {
    float _239 = _uZBot_shreg[(_uA_s0_kk_ii_kkk & 12) / 4][1];
    _227 = _239;
   } // if _237
   else
   {
    float _240;
    int _241 = _uA_s0_kk_ii_kkk & 15;
    int _242 = _241 >> 2;
    bool _243 = _242 == 0;
    if (_243)
    {
     float _244 = float_from_bits(0 /* 0 */);
     _240 = _244;
    } // if _243
    else
    {
     float _246 = _uZTop_shreg[_uA_s0_kk_ii_kkk & 3][1];
     _240 = _246;
    } // if _243 else
    float _247 = _240;
    float _249 = _uA_shreg[1];
    float _251 = _uXTop_shreg[1];
    float _252 = _249 * _251;
    float _253 = _247 + _252;
    //printf("i=%d k=%d a=%f x=%f z=%f->%f\n", 1 * 4 + (_uA_s0_kk_ii_kkk & 12) / 4, _uA_s0_k * 8 + (_uA_s0_kk_ii_kkk >> 4) * 4 + (_uA_s0_kk_ii_kkk & 3), _249, _251, _247, _253);
    _227 = _253;
   } // if _237 else
   float _254 = _227;
   _uZTop_shreg[_uA_s0_kk_ii_kkk & 3][1] = _254;
   (void)_254;
   float _255;
   int _256 = _uA_s0_kk_ii_kkk & 15;
   int _257 = _256 >> 2;
   bool _258 = _257 == 3;
   if (_258)
   {
    float _260 = _uZTop_shreg[_uA_s0_kk_ii_kkk & 3][1];
    float _262 = _uZTop2_shreg[_uA_s0_k * 8 + (_uA_s0_kk_ii_kkk >> 4) * 4 + (_uA_s0_kk_ii_kkk & 3)];
    float _263 = _260 + _262;
    printf("i=%d k=%d z=%f->%f\n", 1 * 4 + (_uA_s0_kk_ii_kkk & 12) / 4, _uA_s0_k * 8 + (_uA_s0_kk_ii_kkk >> 4) * 4 + (_uA_s0_kk_ii_kkk & 3), _262, _263);
    _255 = _263;
   } // if _258
   else
   {
    float _264 = float_from_bits(0 /* 0 */);
    _255 = _264;
   } // if _258 else
   float _265 = _255;
   if (_258 && (1 * 4 + (_uA_s0_kk_ii_kkk & 12) / 4 >=  _uA_s0_k * 8 + (_uA_s0_kk_ii_kkk >> 4) * 4 + (_uA_s0_kk_ii_kkk & 3))) {
    _uZTop2_shreg[_uA_s0_k * 8 + (_uA_s0_kk_ii_kkk >> 4) * 4 + (_uA_s0_kk_ii_kkk & 3)] = _265;
   }
   (void)_265;
  } // for _uA_s0_kk_ii_kkk
 } // for _uA_s0_k
 int _266 = _A_extent_0 >> 3;
 for (int _uA_s0_k = 0; _uA_s0_k < 0 + _266; _uA_s0_k++)
 {
  for (int _uA_s0_kk_ii_kkk = 0; _uA_s0_kk_ii_kkk < 0 + 32; _uA_s0_kk_ii_kkk++)
  {
   float __267 = read_channel_intel(_aLoader_channel[2]);
   _uA_shreg[2] = __267;
   (void)__267;
   float _268;
   int _269 = _uA_s0_kk_ii_kkk & 15;
   int _270 = _269 >> 2;
   bool _271 = _270 == 0;
   if (_271)
   {
    float __272 = read_channel_intel(_xLoader2_channel[2]);
    _268 = __272;
   } // if _271
   else
   {
    float _274 = _uXBot_shreg[_uA_s0_kk_ii_kkk & 3][2];
    _268 = _274;
   } // if _271 else
   float _275 = _268;
   _uXBot_shreg[_uA_s0_kk_ii_kkk & 3][2] = _275;
   (void)_275;
   float _276;
   int _277 = _uA_s0_kk_ii_kkk & 3;
   bool _278 = _277 == 0;
   if (_278)
   {
    float __279 = read_channel_intel(_xLoader1_channel[2]);
    _276 = __279;
   } // if _278
   else
   {
    float _281 = _uXTop_shreg[2];
    _276 = _281;
   } // if _278 else
   float _282 = _276;
   _uXTop_shreg[2] = _282;
   (void)_282;
   float _283;
   int _284 = _uA_s0_kk_ii_kkk & 3;
   bool _285 = _284 == 0;
   float _287 = _uZBot_shreg[(_uA_s0_kk_ii_kkk & 12) / 4][2];
   _283 = _287;
   float _290 = _283;
   float _291;
   int _292 = _uA_s0_kk_ii_kkk & 15;
   int _293 = _292 >> 2;
   int _294 = _uA_s0_k * 8;
   int _295 = _uA_s0_kk_ii_kkk >> 4;
   int _296 = _295 * 4;
   int _297 = _uA_s0_kk_ii_kkk & 3;
   int _298 = _296 + _297;
   int _299 = _294 + _298;
   int _300 = _299 + -8;
   bool _301 = _293 < _300;
   float _304 = _uA_shreg[2];
   float _306 = _uXBot_shreg[_uA_s0_kk_ii_kkk & 3][2];
   if (_301)
   {
    float _302 = float_from_bits(0 /* 0 */);
    _291 = _302;
   } // if _301
   else
   {
    float _307 = _304 * _306;
    _291 = _307;
   } // if _301 else
   float _308 = _291;
   float _309 = _290 + _308;
   //printf("i=%d k=%d a=%f x=%f z=%f->%f\n", 2 * 4 + (_uA_s0_kk_ii_kkk & 12) / 4, _uA_s0_k * 8 + (_uA_s0_kk_ii_kkk >> 4) * 4 + (_uA_s0_kk_ii_kkk & 3), _304, _306, _290, _309);
   _uZBot_shreg[(_uA_s0_kk_ii_kkk & 12) / 4][2] = _309;
   (void)_309;
   float _310;
   int _311 = _uA_s0_kk_ii_kkk & 15;
   int _312 = _311 >> 2;
   int _313 = _uA_s0_k * 8;
   int _314 = _uA_s0_kk_ii_kkk >> 4;
   int _315 = _314 * 4;
   int _316 = _uA_s0_kk_ii_kkk & 3;
   int _317 = _315 + _316;
   int _318 = _313 + _317;
   int _319 = _318 + -8;
   bool _320 = _312 == _319;
   if (_320)
   {
    float _322 = _uZBot_shreg[(_uA_s0_kk_ii_kkk & 12) / 4][2];
    _310 = _322;
   } // if _320
   else
   {
    float _323;
    int _324 = _uA_s0_kk_ii_kkk & 15;
    int _325 = _324 >> 2;
    bool _326 = _325 == 0;
    if (_326)
    {
     float _327 = float_from_bits(0 /* 0 */);
     _323 = _327;
    } // if _326
    else
    {
     float _329 = _uZTop_shreg[_uA_s0_kk_ii_kkk & 3][2];
     _323 = _329;
    } // if _326 else
    float _330 = _323;
    float _332 = _uA_shreg[2];
    float _334 = _uXTop_shreg[2];
    float _335 = _332 * _334;
    float _336 = _330 + _335;
    //printf("i=%d k=%d a=%f x=%f z=%f->%f\n", 2 * 4 + (_uA_s0_kk_ii_kkk & 12) / 4, _uA_s0_k * 8 + (_uA_s0_kk_ii_kkk >> 4) * 4 + (_uA_s0_kk_ii_kkk & 3), _332, _334, _330, _336);
    _310 = _336;
   } // if _320 else
   float _337 = _310;
   _uZTop_shreg[_uA_s0_kk_ii_kkk & 3][2] = _337;
   (void)_337;
   float _338;
   int _339 = _uA_s0_kk_ii_kkk & 15;
   int _340 = _339 >> 2;
   bool _341 = _340 == 3;
   if (_341)
   {
    float _343 = _uZTop_shreg[_uA_s0_kk_ii_kkk & 3][2];
    float _345 = _uZTop2_shreg[_uA_s0_k * 8 + (_uA_s0_kk_ii_kkk >> 4) * 4 + (_uA_s0_kk_ii_kkk & 3)];
    float _346 = _343 + _345;
    //printf("i=%d k=%d z=%f->%f\n", 2 * 4 + (_uA_s0_kk_ii_kkk & 12) / 4, _uA_s0_k * 8 + (_uA_s0_kk_ii_kkk >> 4) * 4 + (_uA_s0_kk_ii_kkk & 3), _345, _346);
    _338 = _346;
   } // if _341
   else
   {
    float _347 = float_from_bits(0 /* 0 */);
    _338 = _347;
   } // if _341 else
   float _348 = _338;
   if (_341 && (2 * 4 + (_uA_s0_kk_ii_kkk & 12) / 4 >=  _uA_s0_k * 8 + (_uA_s0_kk_ii_kkk >> 4) * 4 + (_uA_s0_kk_ii_kkk & 3))) {
    _uZTop2_shreg[_uA_s0_k * 8 + (_uA_s0_kk_ii_kkk >> 4) * 4 + (_uA_s0_kk_ii_kkk & 3)] = _348;
   }
   (void)_348;
  } // for _uA_s0_kk_ii_kkk
 } // for _uA_s0_k
 int _349 = _A_extent_0 >> 3;
 for (int _uA_s0_k = 0; _uA_s0_k < 0 + _349; _uA_s0_k++)
 {
  for (int _uA_s0_kk_ii_kkk = 0; _uA_s0_kk_ii_kkk < 0 + 32; _uA_s0_kk_ii_kkk++)
  {
   float __350 = read_channel_intel(_aLoader_channel[3]);
   _uA_shreg[3] = __350;
   (void)__350;
   float _351;
   int _352 = _uA_s0_kk_ii_kkk & 15;
   int _353 = _352 >> 2;
   bool _354 = _353 == 0;
   if (_354)
   {
    float __355 = read_channel_intel(_xLoader2_channel[3]);
    _351 = __355;
   } // if _354
   else
   {
    float _357 = _uXBot_shreg[_uA_s0_kk_ii_kkk & 3][3];
    _351 = _357;
   } // if _354 else
   float _358 = _351;
   _uXBot_shreg[_uA_s0_kk_ii_kkk & 3][3] = _358;
   (void)_358;
   float _359;
   int _360 = _uA_s0_kk_ii_kkk & 3;
   bool _361 = _360 == 0;
   if (_361)
   {
    float __362 = read_channel_intel(_xLoader1_channel[3]);
    _359 = __362;
   } // if _361
   else
   {
    float _364 = _uXTop_shreg[3];
    _359 = _364;
   } // if _361 else
   float _365 = _359;
   _uXTop_shreg[3] = _365;
   (void)_365;
   float _366;
   int _367 = _uA_s0_kk_ii_kkk & 3;
   bool _368 = _367 == 0;
   float _372 = _uZBot_shreg[(_uA_s0_kk_ii_kkk & 12) / 4][3];
   _366 = _372;
   float _373 = _366;
   float _374;
   int _375 = _uA_s0_kk_ii_kkk & 15;
   int _376 = _375 >> 2;
   int _377 = _uA_s0_k * 8;
   int _378 = _uA_s0_kk_ii_kkk >> 4;
   int _379 = _378 * 4;
   int _380 = _uA_s0_kk_ii_kkk & 3;
   int _381 = _379 + _380;
   int _382 = _377 + _381;
   int _383 = _382 + -12;
   bool _384 = _376 < _383;
   float _387 = _uA_shreg[3];
   float _389 = _uXBot_shreg[_uA_s0_kk_ii_kkk & 3][3];
   if (_384)
   {
    float _385 = float_from_bits(0 /* 0 */);
    _374 = _385;
   } // if _384
   else
   {
    float _390 = _387 * _389;
    _374 = _390;
   } // if _384 else
   float _391 = _374;
   float _392 = _373 + _391;
   //printf("i=%d k=%d a=%f x=%f z=%f->%f\n", 3 * 4 + (_uA_s0_kk_ii_kkk & 12) / 4, _uA_s0_k * 8 + (_uA_s0_kk_ii_kkk >> 4) * 4 + (_uA_s0_kk_ii_kkk & 3), _387, _389, _373, _392);
   _uZBot_shreg[(_uA_s0_kk_ii_kkk & 12) / 4][3] = _392;
   (void)_392;
   float _393;
   int _394 = _uA_s0_kk_ii_kkk & 15;
   int _395 = _394 >> 2;
   int _396 = _uA_s0_k * 8;
   int _397 = _uA_s0_kk_ii_kkk >> 4;
   int _398 = _397 * 4;
   int _399 = _uA_s0_kk_ii_kkk & 3;
   int _400 = _398 + _399;
   int _401 = _396 + _400;
   int _402 = _401 + -12;
   bool _403 = _395 == _402;
   if (_403)
   {
    float _405 = _uZBot_shreg[(_uA_s0_kk_ii_kkk & 12) / 4][3];
    _393 = _405;
   } // if _403
   else
   {
    float _406;
    int _407 = _uA_s0_kk_ii_kkk & 15;
    int _408 = _407 >> 2;
    bool _409 = _408 == 0;
    if (_409)
    {
     float _410 = float_from_bits(0 /* 0 */);
     _406 = _410;
    } // if _409
    else
    {
     float _412 = _uZTop_shreg[_uA_s0_kk_ii_kkk & 3][3];
     _406 = _412;
    } // if _409 else
    float _413 = _406;
    float _415 = _uA_shreg[3];
    float _417 = _uXTop_shreg[3];
    float _418 = _415 * _417;
    float _419 = _413 + _418;
    //printf("i=%d k=%d a=%f x=%f z=%f->%f\n", 3 * 4 + (_uA_s0_kk_ii_kkk & 12) / 4, _uA_s0_k * 8 + (_uA_s0_kk_ii_kkk >> 4) * 4 + (_uA_s0_kk_ii_kkk & 3), _415, _417, _413, _419);
    _393 = _419;
   } // if _403 else
   float _420 = _393;
   _uZTop_shreg[_uA_s0_kk_ii_kkk & 3][3] = _420;
   (void)_420;
   float _421;
   int _422 = _uA_s0_kk_ii_kkk & 15;
   int _423 = _422 >> 2;
   bool _424 = _423 == 3;
   if (_424)
   {
    float _426 = _uZTop_shreg[_uA_s0_kk_ii_kkk & 3][3];
    float _428 = _uZTop2_shreg[_uA_s0_k * 8 + (_uA_s0_kk_ii_kkk >> 4) * 4 + (_uA_s0_kk_ii_kkk & 3)];
    float _429 = _426 + _428;
    printf("i=%d k=%d z=%f->%f\n", 3 * 4 + (_uA_s0_kk_ii_kkk & 12) / 4, _uA_s0_k * 8 + (_uA_s0_kk_ii_kkk >> 4) * 4 + (_uA_s0_kk_ii_kkk & 3), _428, _429);
    _421 = _429;
   } // if _424
   else
   {
    float _430 = float_from_bits(0 /* 0 */);
    _421 = _430;
   } // if _424 else
   float _431 = _421;
   if (_424) {
    _uZTop2_shreg[_uA_s0_k * 8 + (_uA_s0_kk_ii_kkk >> 4) * 4 + (_uA_s0_kk_ii_kkk & 3)] = _431;
   }
   (void)_431;
   int _432 = _uA_s0_kk_ii_kkk & 15;
   int _433 = _432 >> 2;
   bool _434 = _433 == 3;
   if (_434)
   {
    float _436 = _uZTop2_shreg[_uA_s0_k * 8 + (_uA_s0_kk_ii_kkk >> 4) * 4 + (_uA_s0_kk_ii_kkk & 3)];
    //printf("i=%d k=%d z=%f\n", 3 * 4 + (_uA_s0_kk_ii_kkk & 12) / 4, _uA_s0_k * 8 + (_uA_s0_kk_ii_kkk >> 4) * 4 + (_uA_s0_kk_ii_kkk & 3), _436);
    float _437 = _436 * _Alpha;
    float __438 = read_channel_intel(_yLoader_channel[3]);
    float _439 = __438 * _Beta;
    float _440 = _437 + _439;
    write_channel_intel(_Out_channel, _440);
    (void)_440;
   } // if _434
  } // for _uA_s0_kk_ii_kkk
 } // for _uA_s0_k
} // kernel kernel_Out
// Address spaces for kernel_unloader
#define __address_space__unloader_mem_channel __global
__kernel void kernel_unloader(
 const int _A_extent_0,
 __address_space__unloader_mem_channel float *restrict _unloader_mem_channel)
{
 int _addr_temp;
 _addr_temp = 0;
 int _441 = _A_extent_0 >> 3;
 for (int _unloader_s0_k = 0; _unloader_s0_k < 0 + _441; _unloader_s0_k++)
 {
  for (int _unloader_s0_kk_kkk = 0; _unloader_s0_kk_kkk < 0 + 8; _unloader_s0_kk_kkk++)
  {
   float __442 = read_channel_intel(_Out_channel);
   int _443 = _addr_temp;
   _unloader_mem_channel[_443] = __442;
   int _444 = _addr_temp;
   int _445 = _444 + 1;
   _addr_temp = _445;
  } // for _unloader_s0_kk_kkk
 } // for _unloader_s0_k
} // kernel kernel_unloader
#undef __address_space__unloader_mem_channel

