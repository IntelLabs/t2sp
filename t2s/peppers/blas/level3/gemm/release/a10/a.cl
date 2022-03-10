/*OpenCL C x86-64-linux-avx-avx2-avx512-avx512_skylake-enable_synthesis-f16c-fma-intel_fpga-jit-opencl-sse41-user_context*/
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
bool __attribute__ ((aligned(16))) s[16];
struct {bool s0,  s1,  s2,  s3,  s4,  s5,  s6,  s7,  s8,  s9,  sa,  sb,  sc,  sd,  se,  sf;};
} bool16;
typedef union {
bool __attribute__ ((aligned(8))) s[8];
struct {bool s0,  s1,  s2,  s3,  s4,  s5,  s6,  s7;};
} bool8;
channel float16 _aLoader_channel[10][8] __attribute__((depth(128))) ;
typedef struct { float16 s[10][8]; } _aFeeder_channel_array_t;
channel _aFeeder_channel_array_t _aFeeder_channel __attribute__((depth(128))) ;
channel float16 _bLoader_channel[10][8] __attribute__((depth(128))) ;
typedef struct { float16 s[10][8]; } _bFeeder_channel_array_t;
channel _bFeeder_channel_array_t _bFeeder_channel __attribute__((depth(128))) ;
typedef struct { float s[10][8]; } _V_channel_array_t;
channel _V_channel_array_t _V_channel __attribute__((depth(0))) ;
channel float8 _cLoader_channel __attribute__((depth(256))) ;
channel float8 _Out_channel __attribute__((depth(1024))) ;
channel float8 _drainer_channel __attribute__((depth(128))) ;
channel float8 _collector_channel __attribute__((depth(128))) ;
// Address spaces for kernel_aLoader
#define __address_space__A_serializer __global
__kernel void kernel_aLoader(
 const int _A_extent_0,
 const int _A_extent_1,
 const int _B_extent_0,
 __address_space__A_serializer const float *restrict _A_serializer)
{
 int _addr_temp;
 _addr_temp = 0;
 int _0 = _A_extent_1 / 320;
 int _1 = _0 * 320;
 int _2 = _A_extent_1 - _1;
 int _3 = _2 >> 31;
 int _4 = 320 >> 31;
 int _5 = _3 & _4;
 int _6 = _0 - _5;
 int _7 = ~_4;
 int _8 = _3 & _7;
 int _9 = _6 + _8;
 for (int _aLoader_s0_i = 0; _aLoader_s0_i < 0 + _9; _aLoader_s0_i++)
 {
  int _10 = _B_extent_0 >> 8;
  for (int _aLoader_s0_j = 0; _aLoader_s0_j < 0 + _10; _aLoader_s0_j++)
  {
   int _11 = _A_extent_0 >> 9;
   for (int _aLoader_s0_k = 0; _aLoader_s0_k < 0 + _11; _aLoader_s0_k++)
   {
    for (int _aLoader_s0_kk_ii_iii = 0; _aLoader_s0_kk_ii_iii < 0 + 10240; _aLoader_s0_kk_ii_iii++)
    {
     int _12 = _addr_temp;
     int _13 = _B_extent_0 >> 8;
     int _14 = _A_extent_0 >> 9;
     int _15 = _13 * _14;
     int _16 = _15 * 10240;
     int _17 = _12 / _16;
     int _18 = _17 * _16;
     int _19 = _12 - _18;
     int _20 = _19 >> 31;
     int _21 = _16 >> 31;
     int _22 = _20 & _21;
     int _23 = _17 - _22;
     int _24 = ~_21;
     int _25 = _20 & _24;
     int _26 = _23 + _25;
     int _27 = _26 * _14;
     int _28 = _27 * 10240;
     int _29 = _14 * 10240;
     int _30 = _12 % _29;
     int _31 = _30 >> 31;
     uint _32 = abs(_29);
     int _33 = (int)(_32);
     int _34 = _31 & _33;
     int _35 = _30 + _34;
     int _36 = _28 + _35;
     int _37 = _36 * 16;
     float16 _38 = vload16(0, (__address_space__A_serializer float*)_A_serializer + _37);
     write_channel_intel(_aLoader_channel[0][0], _38);
     (void)_38;
     int _39 = _12 + 1;
     _addr_temp = _39;
    } // for _aLoader_s0_kk_ii_iii
   } // for _aLoader_s0_k
  } // for _aLoader_s0_j
 } // for _aLoader_s0_i
} // kernel kernel_aLoader
#undef __address_space__A_serializer
// Address spaces for kernel_aFeeder
__kernel void kernel_aFeeder(
 const int _A_extent_0,
 const int _A_extent_1,
 const int _B_extent_0)
{
 _aFeeder_channel_array_t _aFeeder_channel_array;
 float16 _aFeeder_value_shreg[10][8];
 uint _aFeeder_time_stamp_shreg[10][8];
 float16 _aFeeder_in_v_temp[8];
 uint _aFeeder_cycle_temp[8];
 float16 __attribute__((memory, numbanks(16), singlepump, numwriteports(1), numreadports(1))) _aFeeder_DB_0_ibuffer[2][32][32][16];
 #pragma unroll
 for (int _aFeeder_s0_jjj_init = 0; _aFeeder_s0_jjj_init < 0 + 8; _aFeeder_s0_jjj_init++)
 {
  uint _40 = (uint)(ADD_UINT64_T_SUFFIX(22528));
  _aFeeder_cycle_temp[_aFeeder_s0_jjj_init] = _40;
 } // for _aFeeder_s0_jjj_init
 int _41 = _A_extent_0 >> 9;
 int _42 = _A_extent_1 / 320;
 int _43 = _42 * 320;
 int _44 = _A_extent_1 - _43;
 int _45 = _44 >> 31;
 int _46 = 320 >> 31;
 int _47 = _45 & _46;
 int _48 = _42 - _47;
 int _49 = ~_46;
 int _50 = _45 & _49;
 int _51 = _48 + _50;
 int _52 = _B_extent_0 >> 8;
 int _53 = _51 * _52;
 int _54 = _41 * _53;
 int _55 = _54 * 32768;
 int _56 = _55 + 32768;
 for (int _aFeeder_s0_outermost_loop = 0; _aFeeder_s0_outermost_loop < 0 + _56; _aFeeder_s0_outermost_loop++)
 {
  bool _aFeeder_channel_temp;
  _aFeeder_channel_temp = 0;
  uint _57 = (uint)(ADD_UINT64_T_SUFFIX(22528));
  uint _58 = _aFeeder_cycle_temp[0];
  uint _59 = (uint)(ADD_UINT64_T_SUFFIX(32767));
  uint _60 = _58 & _59;
  bool _61 = _57 <= _60;
  uint _62 = (uint)(ADD_UINT64_T_SUFFIX(15));
  uint _63 = _58 >> _62;
  int _64 = (int)(_63);
  int _65 = _A_extent_0 >> 9;
  int _66 = _A_extent_1 / 320;
  int _67 = _66 * 320;
  int _68 = _A_extent_1 - _67;
  int _69 = _68 >> 31;
  int _70 = 320 >> 31;
  int _71 = _69 & _70;
  int _72 = _66 - _71;
  int _73 = ~_70;
  int _74 = _69 & _73;
  int _75 = _72 + _74;
  int _76 = _B_extent_0 >> 8;
  int _77 = _75 * _76;
  int _78 = _65 * _77;
  bool _79 = _64 < _78;
  bool _80 = _61 && _79;
  if (_80)
  {
   float16 __81 = read_channel_intel(_aLoader_channel[0][0]);
   _aFeeder_in_v_temp[0] = __81;
  } // if _80
  #pragma unroll
  for (int _aFeeder_s0_buf = 0; _aFeeder_s0_buf < 0 + 10; _aFeeder_s0_buf++)
  {
   bool _82 = _aFeeder_s0_buf == 0;
   if (_82)
   {
    float16 _83 = _aFeeder_in_v_temp[0];
    _aFeeder_value_shreg[0][0] = _83;
    (void)_83;
    uint _84 = _aFeeder_cycle_temp[0];
    _aFeeder_time_stamp_shreg[0][0] = _84;
    (void)_84;
   } // if _82
   else
   {
    int _85 = _aFeeder_s0_buf + -1;
    float16 _87 = _aFeeder_value_shreg[_85][0];
    float16 _88 = __fpga_reg(__fpga_reg(_87));
    _aFeeder_value_shreg[_aFeeder_s0_buf][0] = _88;
    (void)_88;
    uint _90 = _aFeeder_time_stamp_shreg[_85][0];
    uint _91 = __fpga_reg(__fpga_reg(_90));
    _aFeeder_time_stamp_shreg[_aFeeder_s0_buf][0] = _91;
    (void)_91;
   } // if _82 else
   uint _92 = (uint)(ADD_UINT64_T_SUFFIX(22528));
   uint _94 = _aFeeder_time_stamp_shreg[_aFeeder_s0_buf][0];
   uint _95 = (uint)(ADD_UINT64_T_SUFFIX(32767));
   uint _96 = _94 & _95;
   bool _97 = _92 <= _96;
   if (_97)
   {
    uint _99 = _aFeeder_time_stamp_shreg[_aFeeder_s0_buf][0];
    uint _100 = (uint)(ADD_UINT64_T_SUFFIX(32767));
    uint _101 = _99 & _100;
    uint _102 = (uint)(ADD_UINT64_T_SUFFIX(22528));
    uint _103 = _101 - _102;
    uint _104 = (uint)(ADD_UINT64_T_SUFFIX(10));
    uint _105 = _103 % _104;
    int _106 = (int)(_105);
    bool _107 = _aFeeder_s0_buf == _106;
    if (_107)
    {
     float16 _109 = _aFeeder_value_shreg[_aFeeder_s0_buf][0];
     uint _111 = _aFeeder_time_stamp_shreg[_aFeeder_s0_buf][0];
     uint _112 = (uint)(ADD_UINT64_T_SUFFIX(15));
     uint _113 = _111 >> _112;
     uint _114 = (uint)(ADD_UINT64_T_SUFFIX(1));
     uint _115 = _113 & _114;
     bool _116 = (bool)(_115);
     uint _118 = (uint)(ADD_UINT64_T_SUFFIX(32767));
     uint _119 = _111 & _118;
     uint _120 = (uint)(ADD_UINT64_T_SUFFIX(22528));
     uint _121 = _119 - _120;
     int _122 = (int)(_121);
     int _123 = _122 / 320;
     int _124 = _123 * 320;
     int _125 = _122 - _124;
     int _126 = _125 >> 31;
     int _127 = 320 >> 31;
     int _128 = _126 & _127;
     int _129 = _123 - _128;
     int _130 = ~_127;
     int _131 = _126 & _130;
     int _132 = _129 + _131;
     int _134 = _122 / 10;
     int _135 = _134 * 10;
     int _136 = _122 - _135;
     int _137 = _136 >> 31;
     int _138 = 10 >> 31;
     int _139 = _137 & _138;
     int _140 = _134 - _139;
     int _141 = ~_138;
     int _142 = _137 & _141;
     int _143 = _140 + _142;
     int _144 = _143 & 31;
     _aFeeder_DB_0_ibuffer[_116][_132][_144][_aFeeder_s0_buf] = _109;
    } // if _107
   } // if _97
   uint _146 = _aFeeder_time_stamp_shreg[_aFeeder_s0_buf][0];
   uint _147 = (uint)(ADD_UINT64_T_SUFFIX(15));
   uint _148 = _146 >> _147;
   int _149 = (int)(_148);
   int _150 = _A_extent_0 >> 9;
   int _151 = _A_extent_1 / 320;
   int _152 = _151 * 320;
   int _153 = _A_extent_1 - _152;
   int _154 = _153 >> 31;
   int _155 = 320 >> 31;
   int _156 = _154 & _155;
   int _157 = _151 - _156;
   int _158 = ~_155;
   int _159 = _154 & _158;
   int _160 = _157 + _159;
   int _161 = _B_extent_0 >> 8;
   int _162 = _160 * _161;
   int _163 = _150 * _162;
   bool _164 = _149 <= _163;
   uint _165 = (uint)(ADD_UINT64_T_SUFFIX(0));
   bool _167 = _165 < _148;
   bool _168 = _164 && _167;
   if (_168)
   {
    uint _170 = _aFeeder_time_stamp_shreg[_aFeeder_s0_buf][0];
    uint _171 = (uint)(ADD_UINT64_T_SUFFIX(15));
    uint _172 = _170 >> _171;
    uint _173 = (uint)(ADD_UINT64_T_SUFFIX(1));
    uint _174 = _172 & _173;
    bool _175 = (bool)(_174);
    bool _176 = !(_175);
    uint _178 = (uint)(ADD_UINT64_T_SUFFIX(32767));
    uint _179 = _170 & _178;
    int _180 = (int)(_179);
    int _181 = _180 >> 10;
    int _183 = _180 >> 5;
    int _184 = _183 & 31;
    float16 _185 = _aFeeder_DB_0_ibuffer[_176][_181][_184][_aFeeder_s0_buf];
    _aFeeder_channel_array.s[_aFeeder_s0_buf][0] = _185;
    (void)0;
    _aFeeder_channel_temp = 1;
   } // if _168
  } // for _aFeeder_s0_buf
  uint _186 = _aFeeder_cycle_temp[0];
  uint _187 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _188 = _186 + _187;
  _aFeeder_cycle_temp[0] = _188;
  uint _189 = _aFeeder_cycle_temp[1];
  uint _190 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _191 = _189 + _190;
  _aFeeder_cycle_temp[1] = _191;
  uint _192 = _aFeeder_cycle_temp[2];
  uint _193 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _194 = _192 + _193;
  _aFeeder_cycle_temp[2] = _194;
  uint _195 = _aFeeder_cycle_temp[3];
  uint _196 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _197 = _195 + _196;
  _aFeeder_cycle_temp[3] = _197;
  uint _198 = _aFeeder_cycle_temp[4];
  uint _199 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _200 = _198 + _199;
  _aFeeder_cycle_temp[4] = _200;
  uint _201 = _aFeeder_cycle_temp[5];
  uint _202 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _203 = _201 + _202;
  _aFeeder_cycle_temp[5] = _203;
  uint _204 = _aFeeder_cycle_temp[6];
  uint _205 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _206 = _204 + _205;
  _aFeeder_cycle_temp[6] = _206;
  uint _207 = _aFeeder_cycle_temp[7];
  uint _208 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _209 = _207 + _208;
  _aFeeder_cycle_temp[7] = _209;
  bool _210 = _aFeeder_channel_temp;
  if (_210)
  {
   write_channel_intel(_aFeeder_channel, _aFeeder_channel_array);
   (void)_aFeeder_channel_array;
  } // if _210
 } // for _aFeeder_s0_outermost_loop
} // kernel kernel_aFeeder
// Address spaces for kernel_bLoader
#define __address_space__B_serializer __global
__kernel void kernel_bLoader(
 const int _A_extent_0,
 const int _A_extent_1,
 const int _B_extent_0,
 __address_space__B_serializer const float *restrict _B_serializer)
{
 int _addr_temp;
 _addr_temp = 0;
 int _211 = _A_extent_1 / 320;
 int _212 = _211 * 320;
 int _213 = _A_extent_1 - _212;
 int _214 = _213 >> 31;
 int _215 = 320 >> 31;
 int _216 = _214 & _215;
 int _217 = _211 - _216;
 int _218 = ~_215;
 int _219 = _214 & _218;
 int _220 = _217 + _219;
 for (int _bLoader_s0_i = 0; _bLoader_s0_i < 0 + _220; _bLoader_s0_i++)
 {
  int _221 = _B_extent_0 >> 8;
  for (int _bLoader_s0_j = 0; _bLoader_s0_j < 0 + _221; _bLoader_s0_j++)
  {
   int _222 = _A_extent_0 >> 9;
   for (int _bLoader_s0_k = 0; _bLoader_s0_k < 0 + _222; _bLoader_s0_k++)
   {
    for (int _bLoader_s0_kk_jj_jjj = 0; _bLoader_s0_kk_jj_jjj < 0 + 8192; _bLoader_s0_kk_jj_jjj++)
    {
     int _223 = _B_extent_0 >> 8;
     int _224 = _A_extent_0 >> 9;
     int _225 = _223 * _224;
     int _226 = _addr_temp;
     int _227 = _225 * 8192;
     int _228 = _226 % _227;
     int _229 = _228 >> 31;
     uint _230 = abs(_227);
     int _231 = (int)(_230);
     int _232 = _229 & _231;
     int _233 = _228 + _232;
     int _234 = _233 * 16;
     float16 _235 = vload16(0, (__address_space__B_serializer float*)_B_serializer + _234);
     write_channel_intel(_bLoader_channel[0][0], _235);
     (void)_235;
     int _236 = _226 + 1;
     _addr_temp = _236;
    } // for _bLoader_s0_kk_jj_jjj
   } // for _bLoader_s0_k
  } // for _bLoader_s0_j
 } // for _bLoader_s0_i
} // kernel kernel_bLoader
#undef __address_space__B_serializer
// Address spaces for kernel_bFeeder
__kernel void kernel_bFeeder(
 const int _A_extent_0,
 const int _A_extent_1,
 const int _B_extent_0)
{
 _bFeeder_channel_array_t _bFeeder_channel_array;
 float16 _bFeeder_value_shreg[10][8];
 uint _bFeeder_time_stamp_shreg[10][8];
 float16 _bFeeder_in_v_temp[10];
 uint _bFeeder_cycle_temp[10];
 float16 __attribute__((memory, numbanks(8), singlepump, numwriteports(1), numreadports(1))) _bFeeder_DB_0_ibuffer[2][32][32][8];
 #pragma unroll
 for (int _bFeeder_s0_iii_init = 0; _bFeeder_s0_iii_init < 0 + 10; _bFeeder_s0_iii_init++)
 {
  uint _237 = (uint)(ADD_UINT64_T_SUFFIX(24576));
  _bFeeder_cycle_temp[_bFeeder_s0_iii_init] = _237;
 } // for _bFeeder_s0_iii_init
 int _238 = _A_extent_0 >> 9;
 int _239 = _A_extent_1 / 320;
 int _240 = _239 * 320;
 int _241 = _A_extent_1 - _240;
 int _242 = _241 >> 31;
 int _243 = 320 >> 31;
 int _244 = _242 & _243;
 int _245 = _239 - _244;
 int _246 = ~_243;
 int _247 = _242 & _246;
 int _248 = _245 + _247;
 int _249 = _B_extent_0 >> 8;
 int _250 = _248 * _249;
 int _251 = _238 * _250;
 int _252 = _251 * 32768;
 int _253 = _252 + 32768;
 for (int _bFeeder_s0_outermost_loop = 0; _bFeeder_s0_outermost_loop < 0 + _253; _bFeeder_s0_outermost_loop++)
 {
  bool _bFeeder_channel_temp;
  _bFeeder_channel_temp = 0;
  uint _254 = (uint)(ADD_UINT64_T_SUFFIX(24576));
  uint _255 = _bFeeder_cycle_temp[0];
  uint _256 = (uint)(ADD_UINT64_T_SUFFIX(32767));
  uint _257 = _255 & _256;
  bool _258 = _254 <= _257;
  uint _259 = (uint)(ADD_UINT64_T_SUFFIX(15));
  uint _260 = _255 >> _259;
  int _261 = (int)(_260);
  int _262 = _A_extent_0 >> 9;
  int _263 = _A_extent_1 / 320;
  int _264 = _263 * 320;
  int _265 = _A_extent_1 - _264;
  int _266 = _265 >> 31;
  int _267 = 320 >> 31;
  int _268 = _266 & _267;
  int _269 = _263 - _268;
  int _270 = ~_267;
  int _271 = _266 & _270;
  int _272 = _269 + _271;
  int _273 = _B_extent_0 >> 8;
  int _274 = _272 * _273;
  int _275 = _262 * _274;
  bool _276 = _261 < _275;
  bool _277 = _258 && _276;
  if (_277)
  {
   float16 __278 = read_channel_intel(_bLoader_channel[0][0]);
   _bFeeder_in_v_temp[0] = __278;
  } // if _277
  #pragma unroll
  for (int _bFeeder_s0_buf = 0; _bFeeder_s0_buf < 0 + 8; _bFeeder_s0_buf++)
  {
   bool _279 = _bFeeder_s0_buf == 0;
   if (_279)
   {
    float16 _280 = _bFeeder_in_v_temp[0];
    _bFeeder_value_shreg[0][0] = _280;
    (void)_280;
    uint _281 = _bFeeder_cycle_temp[0];
    _bFeeder_time_stamp_shreg[0][0] = _281;
    (void)_281;
   } // if _279
   else
   {
    int _282 = _bFeeder_s0_buf + -1;
    float16 _284 = _bFeeder_value_shreg[0][_282];
    float16 _285 = __fpga_reg(__fpga_reg(_284));
    _bFeeder_value_shreg[0][_bFeeder_s0_buf] = _285;
    (void)_285;
    uint _287 = _bFeeder_time_stamp_shreg[0][_282];
    uint _288 = __fpga_reg(__fpga_reg(_287));
    _bFeeder_time_stamp_shreg[0][_bFeeder_s0_buf] = _288;
    (void)_288;
   } // if _279 else
   uint _289 = (uint)(ADD_UINT64_T_SUFFIX(24576));
   uint _291 = _bFeeder_time_stamp_shreg[0][_bFeeder_s0_buf];
   uint _292 = (uint)(ADD_UINT64_T_SUFFIX(32767));
   uint _293 = _291 & _292;
   bool _294 = _289 <= _293;
   if (_294)
   {
    uint _296 = _bFeeder_time_stamp_shreg[0][_bFeeder_s0_buf];
    uint _297 = (uint)(ADD_UINT64_T_SUFFIX(32767));
    uint _298 = _296 & _297;
    uint _299 = (uint)(ADD_UINT64_T_SUFFIX(24576));
    uint _300 = _298 - _299;
    uint _301 = (uint)(ADD_UINT64_T_SUFFIX(7));
    uint _302 = _300 & _301;
    int _303 = (int)(_302);
    bool _304 = _bFeeder_s0_buf == _303;
    if (_304)
    {
     float16 _306 = _bFeeder_value_shreg[0][_bFeeder_s0_buf];
     uint _308 = _bFeeder_time_stamp_shreg[0][_bFeeder_s0_buf];
     uint _309 = (uint)(ADD_UINT64_T_SUFFIX(15));
     uint _310 = _308 >> _309;
     uint _311 = (uint)(ADD_UINT64_T_SUFFIX(1));
     uint _312 = _310 & _311;
     bool _313 = (bool)(_312);
     uint _315 = (uint)(ADD_UINT64_T_SUFFIX(32767));
     uint _316 = _308 & _315;
     uint _317 = (uint)(ADD_UINT64_T_SUFFIX(24576));
     uint _318 = _316 - _317;
     int _319 = (int)(_318);
     int _320 = _319 >> 8;
     int _322 = _319 >> 3;
     int _323 = _322 & 31;
     _bFeeder_DB_0_ibuffer[_313][_320][_323][_bFeeder_s0_buf] = _306;
    } // if _304
   } // if _294
   uint _325 = _bFeeder_time_stamp_shreg[0][_bFeeder_s0_buf];
   uint _326 = (uint)(ADD_UINT64_T_SUFFIX(15));
   uint _327 = _325 >> _326;
   int _328 = (int)(_327);
   int _329 = _A_extent_0 >> 9;
   int _330 = _A_extent_1 / 320;
   int _331 = _330 * 320;
   int _332 = _A_extent_1 - _331;
   int _333 = _332 >> 31;
   int _334 = 320 >> 31;
   int _335 = _333 & _334;
   int _336 = _330 - _335;
   int _337 = ~_334;
   int _338 = _333 & _337;
   int _339 = _336 + _338;
   int _340 = _B_extent_0 >> 8;
   int _341 = _339 * _340;
   int _342 = _329 * _341;
   bool _343 = _328 <= _342;
   uint _344 = (uint)(ADD_UINT64_T_SUFFIX(0));
   bool _346 = _344 < _327;
   bool _347 = _343 && _346;
   if (_347)
   {
    uint _349 = _bFeeder_time_stamp_shreg[0][_bFeeder_s0_buf];
    uint _350 = (uint)(ADD_UINT64_T_SUFFIX(15));
    uint _351 = _349 >> _350;
    uint _352 = (uint)(ADD_UINT64_T_SUFFIX(1));
    uint _353 = _351 & _352;
    bool _354 = (bool)(_353);
    bool _355 = !(_354);
    uint _357 = (uint)(ADD_UINT64_T_SUFFIX(32767));
    uint _358 = _349 & _357;
    int _359 = (int)(_358);
    int _360 = _359 >> 10;
    int _362 = _359 & 31;
    float16 _363 = _bFeeder_DB_0_ibuffer[_355][_360][_362][_bFeeder_s0_buf];
    _bFeeder_channel_array.s[0][_bFeeder_s0_buf] = _363;
    (void)_bFeeder_s0_buf;
    _bFeeder_channel_temp = 1;
   } // if _347
  } // for _bFeeder_s0_buf
  uint _364 = _bFeeder_cycle_temp[0];
  uint _365 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _366 = _364 + _365;
  _bFeeder_cycle_temp[0] = _366;
  uint _367 = _bFeeder_cycle_temp[1];
  uint _368 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _369 = _367 + _368;
  _bFeeder_cycle_temp[1] = _369;
  uint _370 = _bFeeder_cycle_temp[2];
  uint _371 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _372 = _370 + _371;
  _bFeeder_cycle_temp[2] = _372;
  uint _373 = _bFeeder_cycle_temp[3];
  uint _374 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _375 = _373 + _374;
  _bFeeder_cycle_temp[3] = _375;
  uint _376 = _bFeeder_cycle_temp[4];
  uint _377 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _378 = _376 + _377;
  _bFeeder_cycle_temp[4] = _378;
  uint _379 = _bFeeder_cycle_temp[5];
  uint _380 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _381 = _379 + _380;
  _bFeeder_cycle_temp[5] = _381;
  uint _382 = _bFeeder_cycle_temp[6];
  uint _383 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _384 = _382 + _383;
  _bFeeder_cycle_temp[6] = _384;
  uint _385 = _bFeeder_cycle_temp[7];
  uint _386 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _387 = _385 + _386;
  _bFeeder_cycle_temp[7] = _387;
  uint _388 = _bFeeder_cycle_temp[8];
  uint _389 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _390 = _388 + _389;
  _bFeeder_cycle_temp[8] = _390;
  uint _391 = _bFeeder_cycle_temp[9];
  uint _392 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _393 = _391 + _392;
  _bFeeder_cycle_temp[9] = _393;
  bool _394 = _bFeeder_channel_temp;
  if (_394)
  {
   write_channel_intel(_bFeeder_channel, _bFeeder_channel_array);
   (void)_bFeeder_channel_array;
  } // if _394
 } // for _bFeeder_s0_outermost_loop
} // kernel kernel_bFeeder
// Address spaces for kernel_V
__kernel void kernel_V(
 const int _A_extent_0,
 const int _A_extent_1,
 const int _B_extent_0)
{
 _bFeeder_channel_array_t _bFeeder_channel_array;
 _aFeeder_channel_array_t _aFeeder_channel_array;
 _V_channel_array_t _V_channel_array;
 // produce Z
 float _Z_shreg[1024][8][10];
 // produce Y
 float16 _Y_shreg[8][10];
 float _Z_temp[8][10];
 // produce X
 float16 _X_shreg[8][10];
 float _Z_shreg_temp;
 int _395 = _A_extent_1 / 320;
 int _396 = _395 * 320;
 int _397 = _A_extent_1 - _396;
 int _398 = _397 >> 31;
 int _399 = 320 >> 31;
 int _400 = _398 & _399;
 int _401 = _395 - _400;
 int _402 = ~_399;
 int _403 = _398 & _402;
 int _404 = _401 + _403;
 for (int _X_s0_i = 0; _X_s0_i < 0 + _404; _X_s0_i++)
 {
  int _405 = _B_extent_0 >> 8;
  for (int _X_s0_j = 0; _X_s0_j < 0 + _405; _X_s0_j++)
  {
   int _406 = _A_extent_0 >> 9;
   for (int _X_s0_k = 0; _X_s0_k < 0 + _406; _X_s0_k++)
   {
    for (int _X_s0_kk_ii_jj = 0; _X_s0_kk_ii_jj < 0 + 32768; _X_s0_kk_ii_jj++)
    {
     bool _V_channel_temp;
     _V_channel_temp = 0;
     _bFeeder_channel_array_t __407 = read_channel_intel(_bFeeder_channel);
     _bFeeder_channel_array = __407;
     (void)__407;
     _aFeeder_channel_array_t __408 = read_channel_intel(_aFeeder_channel);
     _aFeeder_channel_array = __408;
     (void)__408;
     #pragma unroll
     for (int _dummy__1_s0_iii = 0; _dummy__1_s0_iii < 0 + 10; _dummy__1_s0_iii++)
     {
      #pragma unroll
      for (int _dummy_s0_jjj = 0; _dummy_s0_jjj < 0 + 8; _dummy_s0_jjj++)
      {
       float _410 = _Z_shreg[1023][_dummy_s0_jjj][_dummy__1_s0_iii];
       _Z_temp[_dummy_s0_jjj][_dummy__1_s0_iii] = _410;
       #pragma unroll
       for (int _dummy__2_s0_l1 = 0; _dummy__2_s0_l1 < 0 + 1023; _dummy__2_s0_l1++)
       {
        int _411 = 1023 - _dummy__2_s0_l1;
        int _412 = 1022 - _dummy__2_s0_l1;
        float _414 = _Z_shreg[_412][_dummy_s0_jjj][_dummy__1_s0_iii];
        _Z_shreg[_411][_dummy_s0_jjj][_dummy__1_s0_iii] = _414;
        (void)_414;
       } // for _dummy__2_s0_l1
       float _415 = _Z_temp[_dummy_s0_jjj][_dummy__1_s0_iii];
       _Z_shreg[0][_dummy_s0_jjj][_dummy__1_s0_iii] = _415;
       (void)_415;
      } // for _dummy_s0_jjj
     } // for _dummy__1_s0_iii
     #pragma unroll
     for (int _X_s0_iii = 0; _X_s0_iii < 0 + 10; _X_s0_iii++)
     {
      #pragma unroll
      for (int _X_s0_jjj = 0; _X_s0_jjj < 0 + 8; _X_s0_jjj++)
      {
       float16 _416;
       bool _417 = _X_s0_jjj == 0;
       if (_417)
       {
        float16 __418 = _aFeeder_channel_array.s[_X_s0_iii][_X_s0_jjj];
        _416 = __418;
       } // if _417
       else
       {
        int _419 = _X_s0_jjj + -1;
        float16 _421 = _X_shreg[_419][_X_s0_iii];
        float16 _422 = __fpga_reg(__fpga_reg(_421));
        _416 = _422;
       } // if _417 else
       float16 _423 = _416;
       _X_shreg[_X_s0_jjj][_X_s0_iii] = _423;
       (void)_423;
       float16 _424;
       bool _425 = _X_s0_iii == 0;
       if (_425)
       {
        float16 __426 = _bFeeder_channel_array.s[_X_s0_iii][_X_s0_jjj];
        _424 = __426;
       } // if _425
       else
       {
        int _427 = _X_s0_iii + -1;
        float16 _429 = _Y_shreg[_X_s0_jjj][_427];
        float16 _430 = __fpga_reg(__fpga_reg(_429));
        _424 = _430;
       } // if _425 else
       float16 _431 = _424;
       _Y_shreg[_X_s0_jjj][_X_s0_iii] = _431;
       (void)_431;
       float _432;
       bool _433 = _X_s0_k == 0;
       int _434 = _X_s0_kk_ii_jj >> 10;
       bool _435 = _434 == 0;
       bool _436 = _433 && _435;
       if (_436)
       {
        float _437 = float_from_bits(0 /* 0 */);
        _432 = _437;
       } // if _436
       else
       {
        float _439 = _Z_shreg[0][_X_s0_jjj][_X_s0_iii];
        _432 = _439;
       } // if _436 else
       float _440 = _432;
       _Z_shreg_temp = _440;
       #pragma unroll
       for (int _X_s0_kkk = 0; _X_s0_kkk < 0 + 16; _X_s0_kkk++)
       {
        float _441 = _Z_shreg_temp;
        float _443 = _X_shreg[_X_s0_jjj][_X_s0_iii][_X_s0_kkk];
        float _445 = _Y_shreg[_X_s0_jjj][_X_s0_iii][_X_s0_kkk];
        float _446 = _443 * _445;
        float _447 = _441 + _446;
        _Z_shreg_temp = _447;
       } // for _X_s0_kkk
       float _448 = _Z_shreg_temp;
       _Z_shreg[0][_X_s0_jjj][_X_s0_iii] = _448;
       (void)_448;
       #pragma unroll
       for (int _X_s0_kkk = 0; _X_s0_kkk < 0 + 16; _X_s0_kkk++)
       {
        bool _449 = _X_s0_kkk == 15;
        int _450 = _X_s0_kk_ii_jj >> 10;
        bool _451 = _450 == 31;
        bool _452 = _449 && _451;
        int _453 = _A_extent_0 >> 9;
        int _454 = _453 + -1;
        bool _455 = _X_s0_k == _454;
        bool _456 = _452 && _455;
        if (_456)
        {
         float _458 = _Z_shreg[0][_X_s0_jjj][_X_s0_iii];
         _V_channel_array.s[_X_s0_iii][_X_s0_jjj] = _458;
         (void)_X_s0_jjj;
         _V_channel_temp = 1;
        } // if _456
       } // for _X_s0_kkk
      } // for _X_s0_jjj
     } // for _X_s0_iii
     bool _459 = _V_channel_temp;
     if (_459)
     {
      write_channel_intel(_V_channel, _V_channel_array);
      (void)_V_channel_array;
     } // if _459
    } // for _X_s0_kk_ii_jj
   } // for _X_s0_k
  } // for _X_s0_j
 } // for _X_s0_i
} // kernel kernel_V
// Address spaces for kernel_cLoader
#define __address_space__cSerializer __global
__kernel void kernel_cLoader(
 const int _A_extent_1,
 const int _B_extent_0,
 __address_space__cSerializer const float *restrict _cSerializer)
{
 int _addr_temp;
 _addr_temp = 0;
 int _460 = _A_extent_1 / 320;
 int _461 = _460 * 320;
 int _462 = _A_extent_1 - _461;
 int _463 = _462 >> 31;
 int _464 = 320 >> 31;
 int _465 = _463 & _464;
 int _466 = _460 - _465;
 int _467 = ~_464;
 int _468 = _463 & _467;
 int _469 = _466 + _468;
 for (int _cLoader_s0_i = 0; _cLoader_s0_i < 0 + _469; _cLoader_s0_i++)
 {
  int _470 = _B_extent_0 >> 8;
  for (int _cLoader_s0_j = 0; _cLoader_s0_j < 0 + _470; _cLoader_s0_j++)
  {
   for (int _cLoader_s0_ii_jj_iii = 0; _cLoader_s0_ii_jj_iii < 0 + 10240; _cLoader_s0_ii_jj_iii++)
   {
    int _471 = _addr_temp;
    int _472 = _471 * 8;
    float8 _473 = vload8(0, (__address_space__cSerializer float*)_cSerializer + _472);
    write_channel_intel(_cLoader_channel, _473);
    (void)_473;
    int _474 = _471 + 1;
    _addr_temp = _474;
   } // for _cLoader_s0_ii_jj_iii
  } // for _cLoader_s0_j
 } // for _cLoader_s0_i
} // kernel kernel_cLoader
#undef __address_space__cSerializer
// Address spaces for kernel_Out
__kernel void kernel_Out(
 const int _A_extent_1,
 const int _B_extent_0,
 const float _Alpha,
 const float _Beta)
{
 _V_channel_array_t _V_channel_array;
 int _475 = _A_extent_1 / 320;
 int _476 = _475 * 320;
 int _477 = _A_extent_1 - _476;
 int _478 = _477 >> 31;
 int _479 = 320 >> 31;
 int _480 = _478 & _479;
 int _481 = _475 - _480;
 int _482 = ~_479;
 int _483 = _478 & _482;
 int _484 = _481 + _483;
 for (int _Out_s0_i = 0; _Out_s0_i < 0 + _484; _Out_s0_i++)
 {
  int _485 = _B_extent_0 >> 8;
  for (int _Out_s0_j = 0; _Out_s0_j < 0 + _485; _Out_s0_j++)
  {
   for (int _Out_s0_ii_jj = 0; _Out_s0_ii_jj < 0 + 1024; _Out_s0_ii_jj++)
   {
    _V_channel_array_t __486 = read_channel_intel(_V_channel);
    _V_channel_array = __486;
    (void)__486;
    for (int _Out_s0_iii = 0; _Out_s0_iii < 0 + 10; _Out_s0_iii++)
    {
     int _487 = 0 >> 0;
     int _488 = _487 + 0;
     int _489 = (int)(_488);
     float __490 = _V_channel_array.s[_Out_s0_iii][_489];
     int _491 = _487 + 1;
     int _492 = (int)(_491);
     float __493 = _V_channel_array.s[_Out_s0_iii][_492];
     int _494 = _487 + 2;
     int _495 = (int)(_494);
     float __496 = _V_channel_array.s[_Out_s0_iii][_495];
     int _497 = _487 + 3;
     int _498 = (int)(_497);
     float __499 = _V_channel_array.s[_Out_s0_iii][_498];
     int _500 = _487 + 4;
     int _501 = (int)(_500);
     float __502 = _V_channel_array.s[_Out_s0_iii][_501];
     int _503 = _487 + 5;
     int _504 = (int)(_503);
     float __505 = _V_channel_array.s[_Out_s0_iii][_504];
     int _506 = _487 + 6;
     int _507 = (int)(_506);
     float __508 = _V_channel_array.s[_Out_s0_iii][_507];
     int _509 = _487 + 7;
     int _510 = (int)(_509);
     float __511 = _V_channel_array.s[_Out_s0_iii][_510];
     float8 _512 = (float8)(__490, __493, __496, __499, __502, __505, __508, __511);
     float8 _513 = _Alpha;
     float8 _514 = _512 * _513;
     float8 __515 = read_channel_intel(_cLoader_channel);
     float8 _516 = _Beta;
     float8 _517 = __515 * _516;
     float8 _518 = _514 + _517;
     write_channel_intel(_Out_channel, _518);
     (void)_518;
    } // for _Out_s0_iii
   } // for _Out_s0_ii_jj
  } // for _Out_s0_j
 } // for _Out_s0_i
} // kernel kernel_Out
// Address spaces for kernel_drainer
__kernel void kernel_drainer(
 const int _A_extent_1,
 const int _B_extent_0)
{
 int _519 = _A_extent_1 / 320;
 int _520 = _519 * 320;
 int _521 = _A_extent_1 - _520;
 int _522 = _521 >> 31;
 int _523 = 320 >> 31;
 int _524 = _522 & _523;
 int _525 = _519 - _524;
 int _526 = ~_523;
 int _527 = _522 & _526;
 int _528 = _525 + _527;
 for (int _drainer_s0_i = 0; _drainer_s0_i < 0 + _528; _drainer_s0_i++)
 {
  int _529 = _B_extent_0 >> 8;
  for (int _drainer_s0_j = 0; _drainer_s0_j < 0 + _529; _drainer_s0_j++)
  {
   for (int _drainer_s0_ii_jj_iii = 0; _drainer_s0_ii_jj_iii < 0 + 10240; _drainer_s0_ii_jj_iii++)
   {
    float8 __530 = read_channel_intel(_Out_channel);
    write_channel_intel(_drainer_channel, __530);
    (void)__530;
   } // for _drainer_s0_ii_jj_iii
  } // for _drainer_s0_j
 } // for _drainer_s0_i
} // kernel kernel_drainer
// Address spaces for kernel_collector
__kernel void kernel_collector(
 const int _A_extent_1,
 const int _B_extent_0)
{
 int _531 = _A_extent_1 / 320;
 int _532 = _531 * 320;
 int _533 = _A_extent_1 - _532;
 int _534 = _533 >> 31;
 int _535 = 320 >> 31;
 int _536 = _534 & _535;
 int _537 = _531 - _536;
 int _538 = ~_535;
 int _539 = _534 & _538;
 int _540 = _537 + _539;
 for (int _collector_s0_i = 0; _collector_s0_i < 0 + _540; _collector_s0_i++)
 {
  int _541 = _B_extent_0 >> 8;
  for (int _collector_s0_j = 0; _collector_s0_j < 0 + _541; _collector_s0_j++)
  {
   for (int _collector_s0_ii_jj_iii = 0; _collector_s0_ii_jj_iii < 0 + 10240; _collector_s0_ii_jj_iii++)
   {
    float8 __542 = read_channel_intel(_drainer_channel);
    write_channel_intel(_collector_channel, __542);
    (void)__542;
   } // for _collector_s0_ii_jj_iii
  } // for _collector_s0_j
 } // for _collector_s0_i
} // kernel kernel_collector
// Address spaces for kernel_unloader
#define __address_space__unloader __global
__kernel void kernel_unloader(
 const int _A_extent_1,
 const int _B_extent_0,
 __address_space__unloader float *restrict _unloader)
{
 int _addr_temp;
 _addr_temp = 0;
 int _543 = _A_extent_1 / 320;
 int _544 = _543 * 320;
 int _545 = _A_extent_1 - _544;
 int _546 = _545 >> 31;
 int _547 = 320 >> 31;
 int _548 = _546 & _547;
 int _549 = _543 - _548;
 int _550 = ~_547;
 int _551 = _546 & _550;
 int _552 = _549 + _551;
 for (int _unloader_s0_i = 0; _unloader_s0_i < 0 + _552; _unloader_s0_i++)
 {
  int _553 = _B_extent_0 >> 8;
  for (int _unloader_s0_j = 0; _unloader_s0_j < 0 + _553; _unloader_s0_j++)
  {
   for (int _unloader_s0_ii_jj_iii = 0; _unloader_s0_ii_jj_iii < 0 + 10240; _unloader_s0_ii_jj_iii++)
   {
    float8 __554 = read_channel_intel(_collector_channel);
    int _555 = _addr_temp;
    int _556 = _555 * 8;
    vstore8(__554, 0, (__address_space__unloader float*)_unloader + _556);
    int _557 = _addr_temp;
    int _558 = _557 + 1;
    _addr_temp = _558;
   } // for _unloader_s0_ii_jj_iii
  } // for _unloader_s0_j
 } // for _unloader_s0_i
} // kernel kernel_unloader
#undef __address_space__unloader

