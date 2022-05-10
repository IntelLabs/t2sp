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
bool __attribute__ ((aligned(16))) s[16];
struct {bool s0,  s1,  s2,  s3,  s4,  s5,  s6,  s7,  s8,  s9,  sa,  sb,  sc,  sd,  se,  sf;};
} bool16;
typedef union {
bool __attribute__ ((aligned(8))) s[8];
struct {bool s0,  s1,  s2,  s3,  s4,  s5,  s6,  s7;};
} bool8;
channel float16 _iLoader_channel __attribute__((depth(256))) ;
typedef struct { float16 s[10]; } _iFeeder_channel_array_t;
channel _iFeeder_channel_array_t _iFeeder_channel __attribute__((depth(256))) ;
channel float16 _kLoader_channel __attribute__((depth(256))) ;
typedef struct { float16 s[8]; } _kFeeder_channel_array_t;
channel _kFeeder_channel_array_t _kFeeder_channel __attribute__((depth(256))) ;
channel float8 _Out_channel __attribute__((depth(256))) ;
// Address spaces for kernel_iLoader
#define __address_space__I_serializer_mem_channel __global
__kernel void kernel_iLoader(
 const int _I_extent_0,
 __address_space__I_serializer_mem_channel const float *restrict _I_serializer_mem_channel)
{
 int _addr_temp;
 _addr_temp = 0;
 int _0 = _I_extent_0 >> 8;
 for (int _iLoader_s0_n = 0; _iLoader_s0_n < 0 + _0; _iLoader_s0_n++)
 {
  for (int _iLoader_s0_co_x_y_ci_kx_ky_xx_yy_xxx_yyy = 0; _iLoader_s0_co_x_y_ci_kx_ky_xx_yy_xxx_yyy < 0 + 1105920; _iLoader_s0_co_x_y_ci_kx_ky_xx_yy_xxx_yyy++)
  {
   int _1 = _addr_temp;
   int _2 = _1 / 1105920;
   int _3 = _2 * 552960;
   int _4 = _1 % 552960;
   int _5 = _3 + _4;
   int _6 = _5 * 16;
   float16 _7 = vload16(0, (__address_space__I_serializer_mem_channel float*)_I_serializer_mem_channel + _6);
   write_channel_intel(_iLoader_channel, _7);
   (void)_7;
   int _8 = _1 + 1;
   _addr_temp = _8;
  } // for _iLoader_s0_co_x_y_ci_kx_ky_xx_yy_xxx_yyy
 } // for _iLoader_s0_n
} // kernel kernel_iLoader
#undef __address_space__I_serializer_mem_channel
// Address spaces for kernel_iFeeder
__kernel void kernel_iFeeder(
 const int _I_extent_0)
{
 _iFeeder_channel_array_t _iFeeder_channel_array;
 float16 _iFeeder_value_shreg;
 uint _iFeeder_time_stamp_shreg;
 float16 _iFeeder_in_v_temp;
 uint _iFeeder_cycle_temp;
 float16 __attribute__((memory, numbanks(16), singlepump, numwriteports(1), numreadports(1))) _iFeeder_DB_0_ibuffer[2][3][2][2][16][16];
 #pragma unroll
 for (int _iFeeder_s0_cooo_init = 0; _iFeeder_s0_cooo_init < 0 + 8; _iFeeder_s0_cooo_init++)
 {
  bool _9 = _iFeeder_s0_cooo_init == 0;
  if (_9)
  {
   uint _10 = (uint)(ADD_UINT64_T_SUFFIX(1152));
   _iFeeder_cycle_temp = _10;
  } // if _9
 } // for _iFeeder_s0_cooo_init
 int _11 = _I_extent_0 >> 8;
 int _12 = _11 * 1769472;
 int _13 = _12 + 3072;
 for (int _iFeeder_s0_outermost_loop = 0; _iFeeder_s0_outermost_loop < 0 + _13; _iFeeder_s0_outermost_loop++)
 {
  uint _14 = (uint)(ADD_UINT64_T_SUFFIX(1152));
  uint _15 = _iFeeder_cycle_temp;
  uint _16 = (uint)(ADD_UINT64_T_SUFFIX(3072));
  uint _17 = _15 % _16;
  bool _18 = _14 <= _17;
  uint _19 = _15 / _16;
  int _20 = (int)(_19);
  int _21 = _I_extent_0 >> 8;
  int _22 = _21 * 576;
  bool _23 = _20 < _22;
  bool _24 = _18 && _23;
  if (_24)
  {
   float16 __25 = read_channel_intel(_iLoader_channel);
   _iFeeder_in_v_temp = __25;
  } // if _24
  #pragma unroll
  for (int _iFeeder_s0_buf = 0; _iFeeder_s0_buf < 0 + 10; _iFeeder_s0_buf++)
  {
   bool _26 = _iFeeder_s0_buf == 0;
   if (_26)
   {
    float16 _27 = _iFeeder_in_v_temp;
    _iFeeder_value_shreg = _27;
    (void)_27;
    uint _28 = _iFeeder_cycle_temp;
    _iFeeder_time_stamp_shreg = _28;
    (void)_28;
   } // if _26
   else
   {
    float16 _30 = _iFeeder_value_shreg;
    _iFeeder_value_shreg = _30;
    (void)_30;
    uint _32 = _iFeeder_time_stamp_shreg;
    _iFeeder_time_stamp_shreg = _32;
    (void)_32;
   } // if _26 else
   float16 _34 = _iFeeder_value_shreg;
   float16 _35 = __fpga_reg(__fpga_reg(_34));
   _iFeeder_value_shreg = _35;
   (void)_35;
   uint _37 = _iFeeder_time_stamp_shreg;
   uint _38 = __fpga_reg(__fpga_reg(_37));
   _iFeeder_time_stamp_shreg = _38;
   (void)_38;
   uint _39 = (uint)(ADD_UINT64_T_SUFFIX(1152));
   uint _41 = _iFeeder_time_stamp_shreg;
   uint _42 = (uint)(ADD_UINT64_T_SUFFIX(3072));
   uint _43 = _41 % _42;
   bool _44 = _39 <= _43;
   if (_44)
   {
    uint _46 = _iFeeder_time_stamp_shreg;
    uint _47 = (uint)(ADD_UINT64_T_SUFFIX(3072));
    uint _48 = _46 % _47;
    uint _49 = (uint)(ADD_UINT64_T_SUFFIX(1152));
    uint _50 = _48 - _49;
    uint _51 = (uint)(ADD_UINT64_T_SUFFIX(10));
    uint _52 = _50 % _51;
    int _53 = (int)(_52);
    bool _54 = _iFeeder_s0_buf == _53;
    if (_54)
    {
     float16 _56 = _iFeeder_value_shreg;
     uint _58 = _iFeeder_time_stamp_shreg;
     uint _59 = (uint)(ADD_UINT64_T_SUFFIX(3072));
     uint _60 = _58 / _59;
     uint _61 = (uint)(ADD_UINT64_T_SUFFIX(1));
     uint _62 = _60 & _61;
     bool _63 = (bool)(_62);
     uint _65 = _58 % _59;
     uint _66 = (uint)(ADD_UINT64_T_SUFFIX(1152));
     uint _67 = _65 - _66;
     int _68 = (int)(_67);
     int _69 = _68 / 640;
     int _71 = _68 / 320;
     int _72 = _71 & 1;
     int _74 = _68 / 160;
     int _75 = _74 & 1;
     int _77 = _68 / 10;
     int _78 = _77 & 15;
     _iFeeder_DB_0_ibuffer[_63][_69][_72][_75][_78][_iFeeder_s0_buf] = _56;
    } // if _54
   } // if _44
   uint _80 = _iFeeder_time_stamp_shreg;
   uint _81 = (uint)(ADD_UINT64_T_SUFFIX(3072));
   uint _82 = _80 / _81;
   int _83 = (int)(_82);
   int _84 = _I_extent_0 >> 8;
   int _85 = _84 * 576;
   bool _86 = _83 <= _85;
   uint _87 = (uint)(ADD_UINT64_T_SUFFIX(0));
   bool _89 = _87 < _82;
   bool _90 = _86 && _89;
   if (_90)
   {
    uint _92 = _iFeeder_time_stamp_shreg;
    uint _93 = (uint)(ADD_UINT64_T_SUFFIX(3072));
    uint _94 = _92 / _93;
    uint _95 = (uint)(ADD_UINT64_T_SUFFIX(1));
    uint _96 = _94 & _95;
    bool _97 = (bool)(_96);
    bool _98 = !(_97);
    uint _100 = _92 % _93;
    int _101 = (int)(_100);
    int _102 = _101 >> 10;
    int _104 = _101 >> 9;
    int _105 = _104 & 1;
    int _107 = _101 >> 8;
    int _108 = _107 & 1;
    int _110 = _101 & 15;
    float16 _111 = _iFeeder_DB_0_ibuffer[_98][_102][_105][_108][_110][_iFeeder_s0_buf];
    _iFeeder_channel_array.s[_iFeeder_s0_buf] = _111;
    (void)_iFeeder_s0_buf;
   } // if _90
  } // for _iFeeder_s0_buf
  uint _113 = _iFeeder_time_stamp_shreg;
  uint _114 = (uint)(ADD_UINT64_T_SUFFIX(3072));
  uint _115 = _113 / _114;
  int _116 = (int)(_115);
  int _117 = _I_extent_0 >> 8;
  int _118 = _117 * 576;
  bool _119 = _116 <= _118;
  uint _120 = (uint)(ADD_UINT64_T_SUFFIX(0));
  bool _122 = _120 < _115;
  bool _123 = _119 && _122;
  if (_123)
  {
   write_channel_intel(_iFeeder_channel, _iFeeder_channel_array);
   (void)_iFeeder_channel_array;
  } // if _123
  uint _124 = _iFeeder_cycle_temp;
  uint _125 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _126 = _124 + _125;
  _iFeeder_cycle_temp = _126;
 } // for _iFeeder_s0_outermost_loop
} // kernel kernel_iFeeder
// Address spaces for kernel_kLoader
#if 2359296 <= MAX_CONSTANT_BUFFER_SIZE && 0 < MAX_CONSTANT_ARGS
#define __address_space__K_serializer_mem_channel __constant
#else
#define __address_space__K_serializer_mem_channel __global
#endif
__kernel void kernel_kLoader(
 const int _I_extent_0,
 __address_space__K_serializer_mem_channel const float *restrict _K_serializer_mem_channel)
{
 int _addr_temp;
 _addr_temp = 0;
 int _127 = _I_extent_0 >> 8;
 for (int _kLoader_s0_n = 0; _kLoader_s0_n < 0 + _127; _kLoader_s0_n++)
 {
  for (int _kLoader_s0_co_x_y_ci_kx_ky_coo_cooo = 0; _kLoader_s0_co_x_y_ci_kx_ky_coo_cooo < 0 + 221184; _kLoader_s0_co_x_y_ci_kx_ky_coo_cooo++)
  {
   int _128 = _addr_temp;
   int _129 = _128 / 110592;
   int _130 = _129 * 18432;
   int _131 = _128 % 18432;
   int _132 = _130 + _131;
   int _133 = _132 % 36864;
   int _134 = _133 * 16;
   float16 _135 = vload16(0, (__address_space__K_serializer_mem_channel float*)_K_serializer_mem_channel + _134);
   write_channel_intel(_kLoader_channel, _135);
   (void)_135;
   int _136 = _128 + 1;
   _addr_temp = _136;
  } // for _kLoader_s0_co_x_y_ci_kx_ky_coo_cooo
 } // for _kLoader_s0_n
} // kernel kernel_kLoader
#undef __address_space__K_serializer_mem_channel
// Address spaces for kernel_kFeeder
__kernel void kernel_kFeeder(
 const int _I_extent_0)
{
 _kFeeder_channel_array_t _kFeeder_channel_array;
 float16 _kFeeder_value_shreg;
 uint _kFeeder_time_stamp_shreg;
 float16 _kFeeder_in_v_temp;
 uint _kFeeder_cycle_temp;
 float16 __attribute__((memory, numbanks(8), singlepump, numwriteports(1), numreadports(1))) _kFeeder_DB_0_ibuffer[2][3][16][8];
 #pragma unroll
 for (int _kFeeder_s0_yyy_init = 0; _kFeeder_s0_yyy_init < 0 + 10; _kFeeder_s0_yyy_init++)
 {
  bool _137 = _kFeeder_s0_yyy_init == 0;
  if (_137)
  {
   uint _138 = (uint)(ADD_UINT64_T_SUFFIX(2688));
   _kFeeder_cycle_temp = _138;
  } // if _137
 } // for _kFeeder_s0_yyy_init
 int _139 = _I_extent_0 >> 8;
 int _140 = _139 * 1769472;
 int _141 = _140 + 3072;
 for (int _kFeeder_s0_outermost_loop = 0; _kFeeder_s0_outermost_loop < 0 + _141; _kFeeder_s0_outermost_loop++)
 {
  uint _142 = (uint)(ADD_UINT64_T_SUFFIX(2688));
  uint _143 = _kFeeder_cycle_temp;
  uint _144 = (uint)(ADD_UINT64_T_SUFFIX(3072));
  uint _145 = _143 % _144;
  bool _146 = _142 <= _145;
  uint _147 = _143 / _144;
  int _148 = (int)(_147);
  int _149 = _I_extent_0 >> 8;
  int _150 = _149 * 576;
  bool _151 = _148 < _150;
  bool _152 = _146 && _151;
  if (_152)
  {
   float16 __153 = read_channel_intel(_kLoader_channel);
   _kFeeder_in_v_temp = __153;
  } // if _152
  #pragma unroll
  for (int _kFeeder_s0_buf = 0; _kFeeder_s0_buf < 0 + 8; _kFeeder_s0_buf++)
  {
   bool _154 = _kFeeder_s0_buf == 0;
   if (_154)
   {
    float16 _155 = _kFeeder_in_v_temp;
    _kFeeder_value_shreg = _155;
    (void)_155;
    uint _156 = _kFeeder_cycle_temp;
    _kFeeder_time_stamp_shreg = _156;
    (void)_156;
   } // if _154
   else
   {
    float16 _158 = _kFeeder_value_shreg;
    _kFeeder_value_shreg = _158;
    (void)_158;
    uint _160 = _kFeeder_time_stamp_shreg;
    _kFeeder_time_stamp_shreg = _160;
    (void)_160;
   } // if _154 else
   float16 _162 = _kFeeder_value_shreg;
   float16 _163 = __fpga_reg(__fpga_reg(_162));
   _kFeeder_value_shreg = _163;
   (void)_163;
   uint _165 = _kFeeder_time_stamp_shreg;
   uint _166 = __fpga_reg(__fpga_reg(_165));
   _kFeeder_time_stamp_shreg = _166;
   (void)_166;
   uint _167 = (uint)(ADD_UINT64_T_SUFFIX(2688));
   uint _169 = _kFeeder_time_stamp_shreg;
   uint _170 = (uint)(ADD_UINT64_T_SUFFIX(3072));
   uint _171 = _169 % _170;
   bool _172 = _167 <= _171;
   if (_172)
   {
    uint _174 = _kFeeder_time_stamp_shreg;
    uint _175 = (uint)(ADD_UINT64_T_SUFFIX(3072));
    uint _176 = _174 % _175;
    uint _177 = (uint)(ADD_UINT64_T_SUFFIX(2688));
    uint _178 = _176 - _177;
    uint _179 = (uint)(ADD_UINT64_T_SUFFIX(7));
    uint _180 = _178 & _179;
    int _181 = (int)(_180);
    bool _182 = _kFeeder_s0_buf == _181;
    if (_182)
    {
     float16 _184 = _kFeeder_value_shreg;
     uint _186 = _kFeeder_time_stamp_shreg;
     uint _187 = (uint)(ADD_UINT64_T_SUFFIX(3072));
     uint _188 = _186 / _187;
     uint _189 = (uint)(ADD_UINT64_T_SUFFIX(1));
     uint _190 = _188 & _189;
     bool _191 = (bool)(_190);
     uint _193 = _186 % _187;
     uint _194 = (uint)(ADD_UINT64_T_SUFFIX(2688));
     uint _195 = _193 - _194;
     int _196 = (int)(_195);
     int _197 = _196 >> 7;
     int _199 = _196 >> 3;
     int _200 = _199 & 15;
     _kFeeder_DB_0_ibuffer[_191][_197][_200][_kFeeder_s0_buf] = _184;
    } // if _182
   } // if _172
   uint _202 = _kFeeder_time_stamp_shreg;
   uint _203 = (uint)(ADD_UINT64_T_SUFFIX(3072));
   uint _204 = _202 / _203;
   int _205 = (int)(_204);
   int _206 = _I_extent_0 >> 8;
   int _207 = _206 * 576;
   bool _208 = _205 <= _207;
   uint _209 = (uint)(ADD_UINT64_T_SUFFIX(0));
   bool _211 = _209 < _204;
   bool _212 = _208 && _211;
   if (_212)
   {
    uint _214 = _kFeeder_time_stamp_shreg;
    uint _215 = (uint)(ADD_UINT64_T_SUFFIX(3072));
    uint _216 = _214 / _215;
    uint _217 = (uint)(ADD_UINT64_T_SUFFIX(1));
    uint _218 = _216 & _217;
    bool _219 = (bool)(_218);
    bool _220 = !(_219);
    uint _222 = _214 % _215;
    int _223 = (int)(_222);
    int _224 = _223 >> 10;
    int _226 = _223 >> 4;
    int _227 = _226 & 15;
    float16 _228 = _kFeeder_DB_0_ibuffer[_220][_224][_227][_kFeeder_s0_buf];
    _kFeeder_channel_array.s[_kFeeder_s0_buf] = _228;
    (void)_kFeeder_s0_buf;
   } // if _212
  } // for _kFeeder_s0_buf
  uint _230 = _kFeeder_time_stamp_shreg;
  uint _231 = (uint)(ADD_UINT64_T_SUFFIX(3072));
  uint _232 = _230 / _231;
  int _233 = (int)(_232);
  int _234 = _I_extent_0 >> 8;
  int _235 = _234 * 576;
  bool _236 = _233 <= _235;
  uint _237 = (uint)(ADD_UINT64_T_SUFFIX(0));
  bool _239 = _237 < _232;
  bool _240 = _236 && _239;
  if (_240)
  {
   write_channel_intel(_kFeeder_channel, _kFeeder_channel_array);
   (void)_kFeeder_channel_array;
  } // if _240
  uint _241 = _kFeeder_cycle_temp;
  uint _242 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _243 = _241 + _242;
  _kFeeder_cycle_temp = _243;
 } // for _kFeeder_s0_outermost_loop
} // kernel kernel_kFeeder
// Address spaces for kernel_Out
__kernel void kernel_Out(
 const int _I_extent_0)
{
 _kFeeder_channel_array_t _kFeeder_channel_array;
 _iFeeder_channel_array_t _iFeeder_channel_array;
 // produce C
 float _C_shreg[1024][8][10];
 float _C_pipe_shreg[8][9217];
 // produce B
 float16 _B_shreg[8];
 float _C_temp[8][10];
 // produce A
 float16 _A_shreg[10];
 float _C_shreg_temp;
 int _C_pipe_iter_temp;
 int _C_pipe_base_temp;
 _C_pipe_iter_temp = 10240;
 _C_pipe_base_temp = 0;
 int _244 = _I_extent_0 >> 8;
 int _245 = _244 * 192;
 int _246 = _245 + 1;
 for (int _A_s0_n_co_x_y_ci = 0; _A_s0_n_co_x_y_ci < 0 + _246; _A_s0_n_co_x_y_ci++)
 {
  for (int _A_s0_kx_ky_xx_yy_coo_xxx = 0; _A_s0_kx_ky_xx_yy_coo_xxx < 0 + 9216; _A_s0_kx_ky_xx_yy_coo_xxx++)
  {
   #pragma unroll
   for (int _dummy__1_s0_yyy = 0; _dummy__1_s0_yyy < 0 + 10; _dummy__1_s0_yyy++)
   {
    #pragma unroll
    for (int _dummy_s0_cooo = 0; _dummy_s0_cooo < 0 + 8; _dummy_s0_cooo++)
    {
     float _248 = _C_shreg[1023][_dummy_s0_cooo][_dummy__1_s0_yyy];
     _C_temp[_dummy_s0_cooo][_dummy__1_s0_yyy] = _248;
     #pragma unroll
     for (int _dummy__2_s0_l1 = 0; _dummy__2_s0_l1 < 0 + 1023; _dummy__2_s0_l1++)
     {
      int _249 = 1023 - _dummy__2_s0_l1;
      int _250 = 1022 - _dummy__2_s0_l1;
      float _252 = _C_shreg[_250][_dummy_s0_cooo][_dummy__1_s0_yyy];
      _C_shreg[_249][_dummy_s0_cooo][_dummy__1_s0_yyy] = _252;
      (void)_252;
     } // for _dummy__2_s0_l1
     float _253 = _C_temp[_dummy_s0_cooo][_dummy__1_s0_yyy];
     _C_shreg[0][_dummy_s0_cooo][_dummy__1_s0_yyy] = _253;
     (void)_253;
    } // for _dummy_s0_cooo
   } // for _dummy__1_s0_yyy
   int _254 = _I_extent_0 >> 8;
   int _255 = _254 * 192;
   bool _256 = _A_s0_n_co_x_y_ci < _255;
   if (_256)
   {
    _kFeeder_channel_array_t __257 = read_channel_intel(_kFeeder_channel);
    _kFeeder_channel_array = __257;
    (void)__257;
    _iFeeder_channel_array_t __258 = read_channel_intel(_iFeeder_channel);
    _iFeeder_channel_array = __258;
    (void)__258;
   } // if _256
   #pragma unroll
   for (int _A_s0_yyy = 0; _A_s0_yyy < 0 + 10; _A_s0_yyy++)
   {
    #pragma unroll
    for (int _A_s0_cooo = 0; _A_s0_cooo < 0 + 8; _A_s0_cooo++)
    {
     float16 _259;
     bool _260 = _A_s0_cooo == 0;
     if (_260)
     {
      float16 __261 = _iFeeder_channel_array.s[_A_s0_yyy];
      _259 = __261;
     } // if _260
     else
     {
      float16 _263 = _A_shreg[_A_s0_yyy];
      _259 = _263;
     } // if _260 else
     float16 _264 = _259;
     _A_shreg[_A_s0_yyy] = _264;
     (void)_264;
     float16 _266 = _A_shreg[_A_s0_yyy];
     float16 _267 = __fpga_reg(__fpga_reg(_266));
     _A_shreg[_A_s0_yyy] = _267;
     (void)_267;
     float16 _268;
     bool _269 = _A_s0_yyy == 0;
     if (_269)
     {
      float16 __270 = _kFeeder_channel_array.s[_A_s0_cooo];
      _268 = __270;
     } // if _269
     else
     {
      float16 _272 = _B_shreg[_A_s0_cooo];
      _268 = _272;
     } // if _269 else
     float16 _273 = _268;
     _B_shreg[_A_s0_cooo] = _273;
     (void)_273;
     float16 _275 = _B_shreg[_A_s0_cooo];
     float16 _276 = __fpga_reg(__fpga_reg(_275));
     _B_shreg[_A_s0_cooo] = _276;
     (void)_276;
     float _277;
     int _278 = _A_s0_n_co_x_y_ci & 15;
     bool _279 = _278 == 0;
     int _280 = _A_s0_kx_ky_xx_yy_coo_xxx / 3072;
     bool _281 = _280 == 0;
     bool _282 = _279 && _281;
     int _283 = _A_s0_kx_ky_xx_yy_coo_xxx % 3072;
     int _284 = _283 >> 10;
     bool _285 = _284 == 0;
     bool _286 = _282 && _285;
     if (_286)
     {
      float _287 = float_from_bits(0 /* 0 */);
      _277 = _287;
     } // if _286
     else
     {
      float _289 = _C_shreg[0][_A_s0_cooo][_A_s0_yyy];
      float _290 = __fpga_reg(_289);
      _277 = _290;
     } // if _286 else
     float _291 = _277;
     _C_shreg_temp = _291;
     #pragma unroll
     for (int _A_s0_cii = 0; _A_s0_cii < 0 + 16; _A_s0_cii++)
     {
      float _292 = _C_shreg_temp;
      float _294 = _A_shreg[_A_s0_yyy][_A_s0_cii];
      float _296 = _B_shreg[_A_s0_cooo][_A_s0_cii];
      float _297 = _294 * _296;
      float _298 = _292 + _297;
      _C_shreg_temp = _298;
      int _299 = _A_s0_cii & 3;
      bool _300 = _299 == 3;
      if (_300)
      {
       float _301 = _C_shreg_temp;
       float _302 = __fpga_reg(_301);
       _C_shreg_temp = _302;
      } // if _300
     } // for _A_s0_cii
     float _303 = _C_shreg_temp;
     _C_shreg[0][_A_s0_cooo][_A_s0_yyy] = _303;
     (void)_303;
     #pragma unroll
     for (int _A_s0_cii = 0; _A_s0_cii < 0 + 16; _A_s0_cii++)
     {
      bool _304 = _A_s0_cii == 15;
      int _305 = _A_s0_kx_ky_xx_yy_coo_xxx % 3072;
      int _306 = _305 >> 10;
      bool _307 = _306 == 2;
      bool _308 = _304 && _307;
      int _309 = _A_s0_kx_ky_xx_yy_coo_xxx / 3072;
      bool _310 = _309 == 2;
      bool _311 = _308 && _310;
      int _312 = _A_s0_n_co_x_y_ci & 15;
      bool _313 = _312 == 15;
      bool _314 = _311 && _313;
      if (_314)
      {
       int _315 = _A_s0_yyy * 1024;
       float _317 = _C_shreg[0][_A_s0_cooo][_A_s0_yyy];
       _C_pipe_shreg[_A_s0_cooo][_315] = _317;
       (void)_317;
      } // if _314
     } // for _A_s0_cii
    } // for _A_s0_cooo
   } // for _A_s0_yyy
   int _318 = _A_s0_n_co_x_y_ci & 15;
   bool _319 = _318 == 15;
   int _320 = _A_s0_kx_ky_xx_yy_coo_xxx / 3072;
   bool _321 = _320 == 2;
   bool _322 = _319 && _321;
   int _323 = _A_s0_kx_ky_xx_yy_coo_xxx % 3072;
   int _324 = _323 >> 10;
   bool _325 = _324 == 2;
   bool _326 = _322 && _325;
   int _327 = _A_s0_kx_ky_xx_yy_coo_xxx & 15;
   bool _328 = _327 == 0;
   int _329 = _A_s0_kx_ky_xx_yy_coo_xxx & 255;
   int _330 = _329 >> 4;
   bool _331 = _330 == 0;
   bool _332 = _328 && _331;
   int _333 = _A_s0_kx_ky_xx_yy_coo_xxx & 511;
   int _334 = _333 >> 8;
   bool _335 = _334 == 0;
   bool _336 = _332 && _335;
   int _337 = _A_s0_kx_ky_xx_yy_coo_xxx & 1023;
   int _338 = _337 >> 9;
   bool _339 = _338 == 0;
   bool _340 = _336 && _339;
   bool _341 = _326 && _340;
   if (_341)
   {
    int _342 = _C_pipe_iter_temp;
    _C_pipe_base_temp = _342;
   } // if _341
   float8 _Out_channel_temp;
   #pragma unroll
   for (int _C_pipe_b__126 = 0; _C_pipe_b__126 < 0 + 8; _C_pipe_b__126++)
   {
    float _344 = _C_pipe_shreg[_C_pipe_b__126][0];
    _Out_channel_temp[_C_pipe_b__126] = _344;
    #pragma unroll
    for (int _C_pipe_b__126_dummy = 0; _C_pipe_b__126_dummy < 0 + 8; _C_pipe_b__126_dummy++)
    {
     float _345 = _Out_channel_temp[_C_pipe_b__126_dummy];
     float _346 = __fpga_reg(__fpga_reg(_345));
     _Out_channel_temp[_C_pipe_b__126_dummy] = _346;
    } // for _C_pipe_b__126_dummy
   } // for _C_pipe_b__126
   int _347 = _C_pipe_iter_temp;
   int _348 = _C_pipe_base_temp;
   int _349 = _348 + 10240;
   bool _350 = _347 < _349;
   if (_350)
   {
    float8 _351 = _Out_channel_temp;
    write_channel_intel(_Out_channel, _351);
    (void)_351;
   } // if _350
   #pragma unroll
   for (int _C_pipe_b__127 = 0; _C_pipe_b__127 < 0 + 8; _C_pipe_b__127++)
   {
    #pragma unroll
    for (int _C_pipe_p__63 = 0; _C_pipe_p__63 < 0 + 9; _C_pipe_p__63++)
    {
     #pragma unroll
     for (int _C_pipe_l__63 = 0; _C_pipe_l__63 < 0 + 1023; _C_pipe_l__63++)
     {
      int _352 = _C_pipe_p__63 * 1024;
      int _353 = _352 + _C_pipe_l__63;
      int _354 = _353 + 1;
      float _356 = _C_pipe_shreg[_C_pipe_b__127][_354];
      _C_pipe_shreg[_C_pipe_b__127][_353] = _356;
      (void)_356;
     } // for _C_pipe_l__63
     int _357 = _C_pipe_p__63 * 1024;
     int _358 = _357 + 1023;
     int _359 = _357 + 1024;
     float _361 = _C_pipe_shreg[_C_pipe_b__127][_359];
     float _362 = __fpga_reg(__fpga_reg(_361));
     _C_pipe_shreg[_C_pipe_b__127][_358] = _362;
     (void)_362;
    } // for _C_pipe_p__63
   } // for _C_pipe_b__127
   int _363 = _C_pipe_iter_temp;
   int _364 = _363 + 1;
   _C_pipe_iter_temp = _364;
  } // for _A_s0_kx_ky_xx_yy_coo_xxx
 } // for _A_s0_n_co_x_y_ci
} // kernel kernel_Out
// Address spaces for kernel_unloader
#define __address_space__unloader_mem_channel __global
__kernel void kernel_unloader(
 const int _I_extent_0,
 __address_space__unloader_mem_channel float *restrict _unloader_mem_channel)
{
 int _addr_temp;
 _addr_temp = 0;
 int _365 = _I_extent_0 >> 8;
 for (int _unloader_s0_n = 0; _unloader_s0_n < 0 + _365; _unloader_s0_n++)
 {
  for (int _unloader_s0_co_x_y_yyy_xx_yy_coo_xxx = 0; _unloader_s0_co_x_y_yyy_xx_yy_coo_xxx < 0 + 122880; _unloader_s0_co_x_y_yyy_xx_yy_coo_xxx++)
  {
   float8 __366 = read_channel_intel(_Out_channel);
   int _367 = _addr_temp;
   int _368 = _367 * 8;
   vstore8(__366, 0, (__address_space__unloader_mem_channel float*)_unloader_mem_channel + _368);
   int _369 = _addr_temp;
   int _370 = _369 + 1;
   _addr_temp = _370;
  } // for _unloader_s0_co_x_y_yyy_xx_yy_coo_xxx
 } // for _unloader_s0_n
} // kernel kernel_unloader
#undef __address_space__unloader_mem_channel

