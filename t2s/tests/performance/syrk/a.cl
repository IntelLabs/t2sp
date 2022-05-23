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
bool __attribute__ ((aligned(16))) s[12];
struct {bool s0,  s1,  s2,  s3,  s4,  s5,  s6,  s7,  s8,  s9,  sa,  sb;};
} bool12;
typedef union {
float __attribute__ ((aligned(64))) s[12];
struct {float s0,  s1,  s2,  s3,  s4,  s5,  s6,  s7,  s8,  s9,  sa,  sb;};
} float12;
typedef union {
int __attribute__ ((aligned(64))) s[12];
struct {int s0,  s1,  s2,  s3,  s4,  s5,  s6,  s7,  s8,  s9,  sa,  sb;};
} int12;
channel float16 _aLoader_channel[12][12] __attribute__((depth(256))) ;
typedef struct { float16 s[12][12]; } _aFeeder_channel_array_t;
channel _aFeeder_channel_array_t _aFeeder_channel __attribute__((depth(256))) ;
channel float16 _bLoader_channel[12][12] __attribute__((depth(256))) ;
typedef struct { float16 s[12][12]; } _bFeeder_channel_array_t;
channel _bFeeder_channel_array_t _bFeeder_channel __attribute__((depth(256))) ;
typedef struct { float s[12][12]; } _V_channel_array_t;
channel _V_channel_array_t _V_channel __attribute__((depth(128))) ;
channel float12 _cLoader_channel __attribute__((depth(256))) ;
channel float12 _Out_channel __attribute__((depth(1024))) ;
channel float12 _drainer_channel __attribute__((depth(256))) ;
channel float12 _collector_channel __attribute__((depth(256))) ;
// Address spaces for kernel_aLoader
#if 226492416 <= MAX_CONSTANT_BUFFER_SIZE && 0 < MAX_CONSTANT_ARGS
#define __address_space__aSerializer __constant
#else
#define __address_space__aSerializer __global
#endif
__kernel void kernel_aLoader(
 __address_space__aSerializer const float *restrict _aSerializer)
{
 int _addr_temp;
 _addr_temp = 0;
 for (int _aLoader_s0_i_j_k_kk_ii_iii = 0; _aLoader_s0_i_j_k_kk_ii_iii < 0 + 3538944; _aLoader_s0_i_j_k_kk_ii_iii++)
 {
  int _0 = _addr_temp;
  int _1 = _0 * 16;
  float16 _2 = vload16(0, (__address_space__aSerializer float*)_aSerializer + _1);
  write_channel_intel(_aLoader_channel[0][0], _2);
  (void)_2;
  int _3 = _0 + 1;
  _addr_temp = _3;
 } // for _aLoader_s0_i_j_k_kk_ii_iii
} // kernel kernel_aLoader
#undef __address_space__aSerializer
// Address spaces for kernel_aFeeder
__attribute__((max_global_work_dim(0)))
__attribute__((autorun))
__kernel void kernel_aFeeder(
)
{
 _aFeeder_channel_array_t _aFeeder_channel_array;
 float16 _aFeeder_value_shreg[12][12];
 uint _aFeeder_time_stamp_shreg[12][12];
 float16 _aFeeder_in_v_temp[12];
 uint _aFeeder_cycle_temp[12];
 float16 __attribute__((memory, numbanks(16), singlepump, numwriteports(1), numreadports(1))) _aFeeder_DB_0_ibuffer[2][32][32][16];
 #pragma unroll
 for (int _aFeeder_s0_jjj_init = 0; _aFeeder_s0_jjj_init < 0 + 12; _aFeeder_s0_jjj_init++)
 {
  uint _4 = (uint)(ADD_UINT64_T_SUFFIX(20480));
  _aFeeder_cycle_temp[_aFeeder_s0_jjj_init] = _4;
 } // for _aFeeder_s0_jjj_init
 int _counter_temp;
 _counter_temp = 0;
 while(1)
 {
  bool _aFeeder_channel_temp;
  _aFeeder_channel_temp = 0;
  uint _5 = (uint)(ADD_UINT64_T_SUFFIX(20480));
  uint _6 = _aFeeder_cycle_temp[0];
  uint _7 = (uint)(ADD_UINT64_T_SUFFIX(32767));
  uint _8 = _6 & _7;
  bool _9 = _5 <= _8;
  uint _10 = (uint)(ADD_UINT64_T_SUFFIX(15));
  uint _11 = _6 >> _10;
  int _12 = (int)(_11);
  bool _13 = _12 < 288;
  bool _14 = _9 && _13;
  if (_14)
  {
   float16 __15 = read_channel_intel(_aLoader_channel[0][0]);
   _aFeeder_in_v_temp[0] = __15;
  } // if _14
  #pragma unroll
  for (int _aFeeder_s0_buf = 0; _aFeeder_s0_buf < 0 + 12; _aFeeder_s0_buf++)
  {
   bool _16 = _aFeeder_s0_buf == 0;
   if (_16)
   {
    float16 _17 = _aFeeder_in_v_temp[0];
    _aFeeder_value_shreg[0][0] = _17;
    (void)_17;
    uint _18 = _aFeeder_cycle_temp[0];
    _aFeeder_time_stamp_shreg[0][0] = _18;
    (void)_18;
   } // if _16
   else
   {
    int _19 = _aFeeder_s0_buf + -1;
    float16 _21 = _aFeeder_value_shreg[_19][0];
    float16 _22 = __fpga_reg(__fpga_reg(_21));
    _aFeeder_value_shreg[_aFeeder_s0_buf][0] = _22;
    (void)_22;
    uint _24 = _aFeeder_time_stamp_shreg[_19][0];
    uint _25 = __fpga_reg(__fpga_reg(_24));
    _aFeeder_time_stamp_shreg[_aFeeder_s0_buf][0] = _25;
    (void)_25;
   } // if _16 else
   uint _26 = (uint)(ADD_UINT64_T_SUFFIX(20480));
   uint _28 = _aFeeder_time_stamp_shreg[_aFeeder_s0_buf][0];
   uint _29 = (uint)(ADD_UINT64_T_SUFFIX(32767));
   uint _30 = _28 & _29;
   bool _31 = _26 <= _30;
   if (_31)
   {
    uint _33 = _aFeeder_time_stamp_shreg[_aFeeder_s0_buf][0];
    uint _34 = (uint)(ADD_UINT64_T_SUFFIX(32767));
    uint _35 = _33 & _34;
    uint _36 = (uint)(ADD_UINT64_T_SUFFIX(20480));
    uint _37 = _35 - _36;
    uint _38 = (uint)(ADD_UINT64_T_SUFFIX(12));
    uint _39 = _37 % _38;
    int _40 = (int)(_39);
    bool _41 = _aFeeder_s0_buf == _40;
    if (_41)
    {
     float16 _43 = _aFeeder_value_shreg[_aFeeder_s0_buf][0];
     uint _45 = _aFeeder_time_stamp_shreg[_aFeeder_s0_buf][0];
     uint _46 = (uint)(ADD_UINT64_T_SUFFIX(15));
     uint _47 = _45 >> _46;
     uint _48 = (uint)(ADD_UINT64_T_SUFFIX(1));
     uint _49 = _47 & _48;
     bool _50 = (bool)(_49);
     uint _52 = (uint)(ADD_UINT64_T_SUFFIX(32767));
     uint _53 = _45 & _52;
     uint _54 = (uint)(ADD_UINT64_T_SUFFIX(20480));
     uint _55 = _53 - _54;
     int _56 = (int)(_55);
     int _57 = _56 / 384;
     int _58 = _57 * 384;
     int _59 = _56 - _58;
     int _60 = _59 >> 31;
     int _61 = 384 >> 31;
     int _62 = _60 & _61;
     int _63 = _57 - _62;
     int _64 = ~_61;
     int _65 = _60 & _64;
     int _66 = _63 + _65;
     int _68 = _56 / 12;
     int _69 = _68 * 12;
     int _70 = _56 - _69;
     int _71 = _70 >> 31;
     int _72 = 12 >> 31;
     int _73 = _71 & _72;
     int _74 = _68 - _73;
     int _75 = ~_72;
     int _76 = _71 & _75;
     int _77 = _74 + _76;
     int _78 = _77 & 31;
     _aFeeder_DB_0_ibuffer[_50][_66][_78][_aFeeder_s0_buf] = _43;
    } // if _41
   } // if _31
   uint _80 = _aFeeder_time_stamp_shreg[_aFeeder_s0_buf][0];
   uint _81 = (uint)(ADD_UINT64_T_SUFFIX(15));
   uint _82 = _80 >> _81;
   int _83 = (int)(_82);
   bool _84 = _83 <= 288;
   uint _85 = (uint)(ADD_UINT64_T_SUFFIX(0));
   bool _87 = _85 < _82;
   bool _88 = _84 && _87;
   if (_88)
   {
    uint _90 = _aFeeder_time_stamp_shreg[_aFeeder_s0_buf][0];
    uint _91 = (uint)(ADD_UINT64_T_SUFFIX(15));
    uint _92 = _90 >> _91;
    uint _93 = (uint)(ADD_UINT64_T_SUFFIX(1));
    uint _94 = _92 & _93;
    bool _95 = (bool)(_94);
    bool _96 = !(_95);
    uint _98 = (uint)(ADD_UINT64_T_SUFFIX(32767));
    uint _99 = _90 & _98;
    int _100 = (int)(_99);
    int _101 = _100 >> 10;
    int _103 = _100 >> 5;
    int _104 = _103 & 31;
    float16 _105 = _aFeeder_DB_0_ibuffer[_96][_101][_104][_aFeeder_s0_buf];
    _aFeeder_channel_array.s[_aFeeder_s0_buf][0] = _105;
    (void)0;
    _aFeeder_channel_temp = 1;
   } // if _88
  } // for _aFeeder_s0_buf
  uint _106 = _aFeeder_cycle_temp[0];
  uint _107 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _108 = _106 + _107;
  _aFeeder_cycle_temp[0] = _108;
  uint _109 = _aFeeder_cycle_temp[1];
  uint _110 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _111 = _109 + _110;
  _aFeeder_cycle_temp[1] = _111;
  uint _112 = _aFeeder_cycle_temp[2];
  uint _113 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _114 = _112 + _113;
  _aFeeder_cycle_temp[2] = _114;
  uint _115 = _aFeeder_cycle_temp[3];
  uint _116 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _117 = _115 + _116;
  _aFeeder_cycle_temp[3] = _117;
  uint _118 = _aFeeder_cycle_temp[4];
  uint _119 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _120 = _118 + _119;
  _aFeeder_cycle_temp[4] = _120;
  uint _121 = _aFeeder_cycle_temp[5];
  uint _122 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _123 = _121 + _122;
  _aFeeder_cycle_temp[5] = _123;
  uint _124 = _aFeeder_cycle_temp[6];
  uint _125 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _126 = _124 + _125;
  _aFeeder_cycle_temp[6] = _126;
  uint _127 = _aFeeder_cycle_temp[7];
  uint _128 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _129 = _127 + _128;
  _aFeeder_cycle_temp[7] = _129;
  uint _130 = _aFeeder_cycle_temp[8];
  uint _131 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _132 = _130 + _131;
  _aFeeder_cycle_temp[8] = _132;
  uint _133 = _aFeeder_cycle_temp[9];
  uint _134 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _135 = _133 + _134;
  _aFeeder_cycle_temp[9] = _135;
  uint _136 = _aFeeder_cycle_temp[10];
  uint _137 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _138 = _136 + _137;
  _aFeeder_cycle_temp[10] = _138;
  uint _139 = _aFeeder_cycle_temp[11];
  uint _140 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _141 = _139 + _140;
  _aFeeder_cycle_temp[11] = _141;
  bool _142 = _aFeeder_channel_temp;
  if (_142)
  {
   write_channel_intel(_aFeeder_channel, _aFeeder_channel_array);
   (void)_aFeeder_channel_array;
  } // if _142
  int _143 = _counter_temp;
  int _144 = _143 + 1;
  _counter_temp = _144;
 } // while _aFeeder_s0_outermost_loop_infinite
} // kernel kernel_aFeeder
// Address spaces for kernel_bLoader
#if 226492416 <= MAX_CONSTANT_BUFFER_SIZE && 0 < MAX_CONSTANT_ARGS
#define __address_space__bSerializer __constant
#else
#define __address_space__bSerializer __global
#endif
__kernel void kernel_bLoader(
 __address_space__bSerializer const float *restrict _bSerializer)
{
 int _addr_temp;
 _addr_temp = 0;
 for (int _bLoader_s0_i_j_k_kk_jj_jjj = 0; _bLoader_s0_i_j_k_kk_jj_jjj < 0 + 3538944; _bLoader_s0_i_j_k_kk_jj_jjj++)
 {
  int _145 = _addr_temp;
  int _146 = _145 * 16;
  float16 _147 = vload16(0, (__address_space__bSerializer float*)_bSerializer + _146);
  write_channel_intel(_bLoader_channel[0][0], _147);
  (void)_147;
  int _148 = _145 + 1;
  _addr_temp = _148;
 } // for _bLoader_s0_i_j_k_kk_jj_jjj
} // kernel kernel_bLoader
#undef __address_space__bSerializer
// Address spaces for kernel_bFeeder
__attribute__((max_global_work_dim(0)))
__attribute__((autorun))
__kernel void kernel_bFeeder(
)
{
 _bFeeder_channel_array_t _bFeeder_channel_array;
 float16 _bFeeder_value_shreg[12][12];
 uint _bFeeder_time_stamp_shreg[12][12];
 float16 _bFeeder_in_v_temp[12];
 uint _bFeeder_cycle_temp[12];
 float16 __attribute__((memory, numbanks(16), singlepump, numwriteports(1), numreadports(1))) _bFeeder_DB_0_ibuffer[2][32][32][16];
 #pragma unroll
 for (int _bFeeder_s0_iii_init = 0; _bFeeder_s0_iii_init < 0 + 12; _bFeeder_s0_iii_init++)
 {
  uint _149 = (uint)(ADD_UINT64_T_SUFFIX(20480));
  _bFeeder_cycle_temp[_bFeeder_s0_iii_init] = _149;
 } // for _bFeeder_s0_iii_init
 int _counter_temp;
 _counter_temp = 0;
 while(1)
 {
  bool _bFeeder_channel_temp;
  _bFeeder_channel_temp = 0;
  uint _150 = (uint)(ADD_UINT64_T_SUFFIX(20480));
  uint _151 = _bFeeder_cycle_temp[0];
  uint _152 = (uint)(ADD_UINT64_T_SUFFIX(32767));
  uint _153 = _151 & _152;
  bool _154 = _150 <= _153;
  uint _155 = (uint)(ADD_UINT64_T_SUFFIX(15));
  uint _156 = _151 >> _155;
  int _157 = (int)(_156);
  bool _158 = _157 < 288;
  bool _159 = _154 && _158;
  if (_159)
  {
   float16 __160 = read_channel_intel(_bLoader_channel[0][0]);
   _bFeeder_in_v_temp[0] = __160;
  } // if _159
  #pragma unroll
  for (int _bFeeder_s0_buf = 0; _bFeeder_s0_buf < 0 + 12; _bFeeder_s0_buf++)
  {
   bool _161 = _bFeeder_s0_buf == 0;
   if (_161)
   {
    float16 _162 = _bFeeder_in_v_temp[0];
    _bFeeder_value_shreg[0][0] = _162;
    (void)_162;
    uint _163 = _bFeeder_cycle_temp[0];
    _bFeeder_time_stamp_shreg[0][0] = _163;
    (void)_163;
   } // if _161
   else
   {
    int _164 = _bFeeder_s0_buf + -1;
    float16 _166 = _bFeeder_value_shreg[0][_164];
    float16 _167 = __fpga_reg(__fpga_reg(_166));
    _bFeeder_value_shreg[0][_bFeeder_s0_buf] = _167;
    (void)_167;
    uint _169 = _bFeeder_time_stamp_shreg[0][_164];
    uint _170 = __fpga_reg(__fpga_reg(_169));
    _bFeeder_time_stamp_shreg[0][_bFeeder_s0_buf] = _170;
    (void)_170;
   } // if _161 else
   uint _171 = (uint)(ADD_UINT64_T_SUFFIX(20480));
   uint _173 = _bFeeder_time_stamp_shreg[0][_bFeeder_s0_buf];
   uint _174 = (uint)(ADD_UINT64_T_SUFFIX(32767));
   uint _175 = _173 & _174;
   bool _176 = _171 <= _175;
   if (_176)
   {
    uint _178 = _bFeeder_time_stamp_shreg[0][_bFeeder_s0_buf];
    uint _179 = (uint)(ADD_UINT64_T_SUFFIX(32767));
    uint _180 = _178 & _179;
    uint _181 = (uint)(ADD_UINT64_T_SUFFIX(20480));
    uint _182 = _180 - _181;
    uint _183 = (uint)(ADD_UINT64_T_SUFFIX(12));
    uint _184 = _182 % _183;
    int _185 = (int)(_184);
    bool _186 = _bFeeder_s0_buf == _185;
    if (_186)
    {
     float16 _188 = _bFeeder_value_shreg[0][_bFeeder_s0_buf];
     uint _190 = _bFeeder_time_stamp_shreg[0][_bFeeder_s0_buf];
     uint _191 = (uint)(ADD_UINT64_T_SUFFIX(15));
     uint _192 = _190 >> _191;
     uint _193 = (uint)(ADD_UINT64_T_SUFFIX(1));
     uint _194 = _192 & _193;
     bool _195 = (bool)(_194);
     uint _197 = (uint)(ADD_UINT64_T_SUFFIX(32767));
     uint _198 = _190 & _197;
     uint _199 = (uint)(ADD_UINT64_T_SUFFIX(20480));
     uint _200 = _198 - _199;
     int _201 = (int)(_200);
     int _202 = _201 / 384;
     int _203 = _202 * 384;
     int _204 = _201 - _203;
     int _205 = _204 >> 31;
     int _206 = 384 >> 31;
     int _207 = _205 & _206;
     int _208 = _202 - _207;
     int _209 = ~_206;
     int _210 = _205 & _209;
     int _211 = _208 + _210;
     int _213 = _201 / 12;
     int _214 = _213 * 12;
     int _215 = _201 - _214;
     int _216 = _215 >> 31;
     int _217 = 12 >> 31;
     int _218 = _216 & _217;
     int _219 = _213 - _218;
     int _220 = ~_217;
     int _221 = _216 & _220;
     int _222 = _219 + _221;
     int _223 = _222 & 31;
     _bFeeder_DB_0_ibuffer[_195][_211][_223][_bFeeder_s0_buf] = _188;
    } // if _186
   } // if _176
   uint _225 = _bFeeder_time_stamp_shreg[0][_bFeeder_s0_buf];
   uint _226 = (uint)(ADD_UINT64_T_SUFFIX(15));
   uint _227 = _225 >> _226;
   int _228 = (int)(_227);
   bool _229 = _228 <= 288;
   uint _230 = (uint)(ADD_UINT64_T_SUFFIX(0));
   bool _232 = _230 < _227;
   bool _233 = _229 && _232;
   if (_233)
   {
    uint _235 = _bFeeder_time_stamp_shreg[0][_bFeeder_s0_buf];
    uint _236 = (uint)(ADD_UINT64_T_SUFFIX(15));
    uint _237 = _235 >> _236;
    uint _238 = (uint)(ADD_UINT64_T_SUFFIX(1));
    uint _239 = _237 & _238;
    bool _240 = (bool)(_239);
    bool _241 = !(_240);
    uint _243 = (uint)(ADD_UINT64_T_SUFFIX(32767));
    uint _244 = _235 & _243;
    int _245 = (int)(_244);
    int _246 = _245 >> 10;
    int _248 = _245 & 31;
    float16 _249 = _bFeeder_DB_0_ibuffer[_241][_246][_248][_bFeeder_s0_buf];
    _bFeeder_channel_array.s[0][_bFeeder_s0_buf] = _249;
    (void)_bFeeder_s0_buf;
    _bFeeder_channel_temp = 1;
   } // if _233
  } // for _bFeeder_s0_buf
  uint _250 = _bFeeder_cycle_temp[0];
  uint _251 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _252 = _250 + _251;
  _bFeeder_cycle_temp[0] = _252;
  uint _253 = _bFeeder_cycle_temp[1];
  uint _254 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _255 = _253 + _254;
  _bFeeder_cycle_temp[1] = _255;
  uint _256 = _bFeeder_cycle_temp[2];
  uint _257 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _258 = _256 + _257;
  _bFeeder_cycle_temp[2] = _258;
  uint _259 = _bFeeder_cycle_temp[3];
  uint _260 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _261 = _259 + _260;
  _bFeeder_cycle_temp[3] = _261;
  uint _262 = _bFeeder_cycle_temp[4];
  uint _263 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _264 = _262 + _263;
  _bFeeder_cycle_temp[4] = _264;
  uint _265 = _bFeeder_cycle_temp[5];
  uint _266 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _267 = _265 + _266;
  _bFeeder_cycle_temp[5] = _267;
  uint _268 = _bFeeder_cycle_temp[6];
  uint _269 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _270 = _268 + _269;
  _bFeeder_cycle_temp[6] = _270;
  uint _271 = _bFeeder_cycle_temp[7];
  uint _272 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _273 = _271 + _272;
  _bFeeder_cycle_temp[7] = _273;
  uint _274 = _bFeeder_cycle_temp[8];
  uint _275 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _276 = _274 + _275;
  _bFeeder_cycle_temp[8] = _276;
  uint _277 = _bFeeder_cycle_temp[9];
  uint _278 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _279 = _277 + _278;
  _bFeeder_cycle_temp[9] = _279;
  uint _280 = _bFeeder_cycle_temp[10];
  uint _281 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _282 = _280 + _281;
  _bFeeder_cycle_temp[10] = _282;
  uint _283 = _bFeeder_cycle_temp[11];
  uint _284 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _285 = _283 + _284;
  _bFeeder_cycle_temp[11] = _285;
  bool _286 = _bFeeder_channel_temp;
  if (_286)
  {
   write_channel_intel(_bFeeder_channel, _bFeeder_channel_array);
   (void)_bFeeder_channel_array;
  } // if _286
  int _287 = _counter_temp;
  int _288 = _287 + 1;
  _counter_temp = _288;
 } // while _bFeeder_s0_outermost_loop_infinite
} // kernel kernel_bFeeder
// Address spaces for kernel_V
__attribute__((max_global_work_dim(0)))
__attribute__((autorun))
__kernel void kernel_V(
)
{
 _bFeeder_channel_array_t _bFeeder_channel_array;
 _aFeeder_channel_array_t _aFeeder_channel_array;
 _V_channel_array_t _V_channel_array;
 // produce Z
 float _Z_shreg[1024][12][12];
 // produce uB
 float16 _uB_shreg[12][12];
 float _Z_temp[12][12];
 // produce uA
 float16 _uA_shreg[12][12];
 float _Z_shreg_temp;
 int _counter_temp;
 _counter_temp = 0;
 while(1)
 {
  bool _V_channel_temp;
  _V_channel_temp = 0;
  _bFeeder_channel_array_t __289 = read_channel_intel(_bFeeder_channel);
  _bFeeder_channel_array = __289;
  (void)__289;
  _aFeeder_channel_array_t __290 = read_channel_intel(_aFeeder_channel);
  _aFeeder_channel_array = __290;
  (void)__290;
  #pragma unroll
  for (int _dummy__1_s0_iii = 0; _dummy__1_s0_iii < 0 + 12; _dummy__1_s0_iii++)
  {
   #pragma unroll
   for (int _dummy_s0_jjj = 0; _dummy_s0_jjj < 0 + 12; _dummy_s0_jjj++)
   {
    float _292 = _Z_shreg[1023][_dummy_s0_jjj][_dummy__1_s0_iii];
    _Z_temp[_dummy_s0_jjj][_dummy__1_s0_iii] = _292;
    #pragma unroll
    for (int _dummy__2_s0_l1 = 0; _dummy__2_s0_l1 < 0 + 1023; _dummy__2_s0_l1++)
    {
     int _293 = 1023 - _dummy__2_s0_l1;
     int _294 = 1022 - _dummy__2_s0_l1;
     float _296 = _Z_shreg[_294][_dummy_s0_jjj][_dummy__1_s0_iii];
     _Z_shreg[_293][_dummy_s0_jjj][_dummy__1_s0_iii] = _296;
     (void)_296;
    } // for _dummy__2_s0_l1
    float _297 = _Z_temp[_dummy_s0_jjj][_dummy__1_s0_iii];
    _Z_shreg[0][_dummy_s0_jjj][_dummy__1_s0_iii] = _297;
    (void)_297;
   } // for _dummy_s0_jjj
  } // for _dummy__1_s0_iii
  #pragma unroll
  for (int _uA_s0_iii = 0; _uA_s0_iii < 0 + 12; _uA_s0_iii++)
  {
   #pragma unroll
   for (int _uA_s0_jjj = 0; _uA_s0_jjj < 0 + 12; _uA_s0_jjj++)
   {
    float16 _298;
    bool _299 = _uA_s0_jjj == 0;
    if (_299)
    {
     float16 __300 = _aFeeder_channel_array.s[_uA_s0_iii][_uA_s0_jjj];
     _298 = __300;
    } // if _299
    else
    {
     int _301 = _uA_s0_jjj + -1;
     float16 _303 = _uA_shreg[_301][_uA_s0_iii];
     float16 _304 = __fpga_reg(__fpga_reg(_303));
     _298 = _304;
    } // if _299 else
    float16 _305 = _298;
    _uA_shreg[_uA_s0_jjj][_uA_s0_iii] = _305;
    (void)_305;
    float16 _306;
    bool _307 = _uA_s0_iii == 0;
    if (_307)
    {
     float16 __308 = _bFeeder_channel_array.s[_uA_s0_iii][_uA_s0_jjj];
     _306 = __308;
    } // if _307
    else
    {
     int _309 = _uA_s0_iii + -1;
     float16 _311 = _uB_shreg[_uA_s0_jjj][_309];
     float16 _312 = __fpga_reg(__fpga_reg(_311));
     _306 = _312;
    } // if _307 else
    float16 _313 = _306;
    _uB_shreg[_uA_s0_jjj][_uA_s0_iii] = _313;
    (void)_313;
    float _314;
    int _315 = _counter_temp;
    int _316 = _315 & 262143;
    int _317 = _316 >> 15;
    bool _318 = _317 == 0;
    int _319 = _315 & 32767;
    int _320 = _319 >> 10;
    bool _321 = _320 == 0;
    bool _322 = _318 && _321;
    if (_322)
    {
     float _323 = float_from_bits(0 /* 0 */);
     _314 = _323;
    } // if _322
    else
    {
     float _325 = _Z_shreg[0][_uA_s0_jjj][_uA_s0_iii];
     _314 = _325;
    } // if _322 else
    float _326 = _314;
    _Z_shreg_temp = _326;
    #pragma unroll
    for (int _uA_s0_kkk = 0; _uA_s0_kkk < 0 + 16; _uA_s0_kkk++)
    {
     float _327 = _Z_shreg_temp;
     float _329 = _uA_shreg[_uA_s0_jjj][_uA_s0_iii][_uA_s0_kkk];
     float _331 = _uB_shreg[_uA_s0_jjj][_uA_s0_iii][_uA_s0_kkk];
     float _332 = _329 * _331;
     float _333 = _327 + _332;
     _Z_shreg_temp = _333;
    } // for _uA_s0_kkk
    float _334 = _Z_shreg_temp;
    _Z_shreg[0][_uA_s0_jjj][_uA_s0_iii] = _334;
    (void)_334;
    #pragma unroll
    for (int _uA_s0_kkk = 0; _uA_s0_kkk < 0 + 16; _uA_s0_kkk++)
    {
     bool _335 = _uA_s0_kkk == 15;
     int _336 = _counter_temp;
     int _337 = _336 & 32767;
     int _338 = _337 >> 10;
     bool _339 = _338 == 31;
     bool _340 = _335 && _339;
     int _341 = _336 & 262143;
     int _342 = _341 >> 15;
     bool _343 = _342 == 7;
     bool _344 = _340 && _343;
     if (_344)
     {
      float _346 = _Z_shreg[0][_uA_s0_jjj][_uA_s0_iii];
      _V_channel_array.s[_uA_s0_iii][_uA_s0_jjj] = _346;
      (void)_uA_s0_jjj;
      _V_channel_temp = 1;
     } // if _344
    } // for _uA_s0_kkk
   } // for _uA_s0_jjj
  } // for _uA_s0_iii
  bool _347 = _V_channel_temp;
  if (_347)
  {
   write_channel_intel(_V_channel, _V_channel_array);
   (void)_V_channel_array;
  } // if _347
  int _348 = _counter_temp;
  int _349 = _348 + 1;
  _counter_temp = _349;
 } // while _uA_s0_i_j_k_kk_ii_jj_infinite
} // kernel kernel_V
// Address spaces for kernel_cLoader
#if 21233664 <= MAX_CONSTANT_BUFFER_SIZE && 0 < MAX_CONSTANT_ARGS
#define __address_space__cSerializer __constant
#else
#define __address_space__cSerializer __global
#endif
__kernel void kernel_cLoader(
 __address_space__cSerializer const float *restrict _cSerializer)
{
 int _addr_temp;
 _addr_temp = 0;
 for (int _cLoader_s0_i_j_ii_jj_iii = 0; _cLoader_s0_i_j_ii_jj_iii < 0 + 442368; _cLoader_s0_i_j_ii_jj_iii++)
 {
  int _350 = _addr_temp;
  int _351 = _350 * 12;
  float12 _352 = *((__address_space__cSerializer float12*)(_cSerializer + _351));
  write_channel_intel(_cLoader_channel, _352);
  (void)_352;
  int _353 = _350 + 1;
  _addr_temp = _353;
 } // for _cLoader_s0_i_j_ii_jj_iii
} // kernel kernel_cLoader
#undef __address_space__cSerializer
// Address spaces for kernel_Out
__kernel void kernel_Out(
 const float _Alpha,
 const float _Beta)
{
 _V_channel_array_t _V_channel_array;
 for (int _Out_s0_i_j_ii_jj = 0; _Out_s0_i_j_ii_jj < 0 + 36864; _Out_s0_i_j_ii_jj++)
 {
  _V_channel_array_t __354 = read_channel_intel(_V_channel);
  _V_channel_array = __354;
  (void)__354;
  for (int _Out_s0_iii = 0; _Out_s0_iii < 0 + 12; _Out_s0_iii++)
  {
   int _355 = 0 >> 0;
   int _356 = _355 + 0;
   int _357 = (int)(_356);
   float __358 = _V_channel_array.s[_Out_s0_iii][_357];
   int _359 = _355 + 1;
   int _360 = (int)(_359);
   float __361 = _V_channel_array.s[_Out_s0_iii][_360];
   int _362 = _355 + 2;
   int _363 = (int)(_362);
   float __364 = _V_channel_array.s[_Out_s0_iii][_363];
   int _365 = _355 + 3;
   int _366 = (int)(_365);
   float __367 = _V_channel_array.s[_Out_s0_iii][_366];
   int _368 = _355 + 4;
   int _369 = (int)(_368);
   float __370 = _V_channel_array.s[_Out_s0_iii][_369];
   int _371 = _355 + 5;
   int _372 = (int)(_371);
   float __373 = _V_channel_array.s[_Out_s0_iii][_372];
   int _374 = _355 + 6;
   int _375 = (int)(_374);
   float __376 = _V_channel_array.s[_Out_s0_iii][_375];
   int _377 = _355 + 7;
   int _378 = (int)(_377);
   float __379 = _V_channel_array.s[_Out_s0_iii][_378];
   int _380 = _355 + 8;
   int _381 = (int)(_380);
   float __382 = _V_channel_array.s[_Out_s0_iii][_381];
   int _383 = _355 + 9;
   int _384 = (int)(_383);
   float __385 = _V_channel_array.s[_Out_s0_iii][_384];
   int _386 = _355 + 10;
   int _387 = (int)(_386);
   float __388 = _V_channel_array.s[_Out_s0_iii][_387];
   int _389 = _355 + 11;
   int _390 = (int)(_389);
   float __391 = _V_channel_array.s[_Out_s0_iii][_390];
   float12 _392 = (float12){__358, __361, __364, __367, __370, __373, __376, __379, __382, __385, __388, __391};
   float12 _393 = {_Alpha, _Alpha, _Alpha, _Alpha, _Alpha, _Alpha, _Alpha, _Alpha, _Alpha, _Alpha, _Alpha, _Alpha};
   float12 _394 = {_392.s0*_393.s0, _392.s1*_393.s1, _392.s2*_393.s2, _392.s3*_393.s3, _392.s4*_393.s4, _392.s5*_393.s5, _392.s6*_393.s6, _392.s7*_393.s7, _392.s8*_393.s8, _392.s9*_393.s9, _392.sa*_393.sa, _392.sb*_393.sb};
;
   float12 __395 = read_channel_intel(_cLoader_channel);
   float12 _396 = {_Beta, _Beta, _Beta, _Beta, _Beta, _Beta, _Beta, _Beta, _Beta, _Beta, _Beta, _Beta};
   float12 _397 = {__395.s0*_396.s0, __395.s1*_396.s1, __395.s2*_396.s2, __395.s3*_396.s3, __395.s4*_396.s4, __395.s5*_396.s5, __395.s6*_396.s6, __395.s7*_396.s7, __395.s8*_396.s8, __395.s9*_396.s9, __395.sa*_396.sa, __395.sb*_396.sb};
;
   float12 _398 = {_394.s0+_397.s0, _394.s1+_397.s1, _394.s2+_397.s2, _394.s3+_397.s3, _394.s4+_397.s4, _394.s5+_397.s5, _394.s6+_397.s6, _394.s7+_397.s7, _394.s8+_397.s8, _394.s9+_397.s9, _394.sa+_397.sa, _394.sb+_397.sb};
;
   write_channel_intel(_Out_channel, _398);
   (void)_398;
  } // for _Out_s0_iii
 } // for _Out_s0_i_j_ii_jj
} // kernel kernel_Out
// Address spaces for kernel_drainer
__attribute__((max_global_work_dim(0)))
__attribute__((autorun))
__kernel void kernel_drainer(
)
{
 int _counter_temp;
 _counter_temp = 0;
 while(1)
 {
  float12 __399 = read_channel_intel(_Out_channel);
  write_channel_intel(_drainer_channel, __399);
  (void)__399;
  int _400 = _counter_temp;
  int _401 = _400 + 1;
  _counter_temp = _401;
 } // while _drainer_s0_i_j_ii_jj_iii_infinite
} // kernel kernel_drainer
// Address spaces for kernel_collector
__attribute__((max_global_work_dim(0)))
__attribute__((autorun))
__kernel void kernel_collector(
)
{
 int _counter_temp;
 _counter_temp = 0;
 while(1)
 {
  float12 __402 = read_channel_intel(_drainer_channel);
  write_channel_intel(_collector_channel, __402);
  (void)__402;
  int _403 = _counter_temp;
  int _404 = _403 + 1;
  _counter_temp = _404;
 } // while _collector_s0_i_j_ii_jj_iii_infinite
} // kernel kernel_collector
// Address spaces for kernel_unloader
#define __address_space__unloader __global
__kernel void kernel_unloader(
 __address_space__unloader float *restrict _unloader)
{
 int _addr_temp;
 _addr_temp = 0;
 for (int _unloader_s0_i_j_ii_jj_iii = 0; _unloader_s0_i_j_ii_jj_iii < 0 + 442368; _unloader_s0_i_j_ii_jj_iii++)
 {
  float12 __405 = read_channel_intel(_collector_channel);
  int _406 = _addr_temp;
  int _407 = _406 * 12;
  *((__address_space__unloader float12*)(_unloader + _407)) = __405;
  int _408 = _addr_temp;
  int _409 = _408 + 1;
  _addr_temp = _409;
 } // for _unloader_s0_i_j_ii_jj_iii
} // kernel kernel_unloader
#undef __address_space__unloader

