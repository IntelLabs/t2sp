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
channel float16 _PLoader_channel __attribute__((depth(256))) ;
typedef struct { float16 s[10]; } _PFeeder_channel_array_t;
channel _PFeeder_channel_array_t _PFeeder_channel __attribute__((depth(256))) ;
channel float16 _WLoader_channel __attribute__((depth(256))) ;
typedef struct { float16 s[8]; } _WFeeder_channel_array_t;
channel _WFeeder_channel_array_t _WFeeder_channel __attribute__((depth(256))) ;
channel float8 _Out_channel __attribute__((depth(256))) ;
// Address spaces for kernel_PLoader
#define __address_space__P_serializer_mem_channel __global
__kernel void kernel_PLoader(
 const int _P_extent_1,
 __address_space__P_serializer_mem_channel const float *restrict _P_serializer_mem_channel)
{
 int _addr_temp;
 _addr_temp = 0;
 int _0 = _P_extent_1 / 900;
 for (int _PLoader_s0_n = 0; _PLoader_s0_n < 0 + _0; _PLoader_s0_n++)
 {
  for (int _PLoader_s0_mk_ci_kx_ky_nn_mx_y_x_yyy_xxx = 0; _PLoader_s0_mk_ci_kx_ky_nn_mx_y_x_yyy_xxx < 0 + 57600; _PLoader_s0_mk_ci_kx_ky_nn_mx_y_x_yyy_xxx++)
  {
   int _1 = _addr_temp;
   int _2 = _1 * 16;
   float16 _3 = vload16(0, (__address_space__P_serializer_mem_channel float*)_P_serializer_mem_channel + _2);
   write_channel_intel(_PLoader_channel, _3);
   (void)_3;
   int _4 = _1 + 1;
   _addr_temp = _4;
  } // for _PLoader_s0_mk_ci_kx_ky_nn_mx_y_x_yyy_xxx
 } // for _PLoader_s0_n
} // kernel kernel_PLoader
#undef __address_space__P_serializer_mem_channel
// Address spaces for kernel_PFeeder
__kernel void kernel_PFeeder(
 const int _P_extent_1)
{
 _PFeeder_channel_array_t _PFeeder_channel_array;
 float16 _PFeeder_value_shreg;
 uint _PFeeder_time_stamp_shreg;
 float16 _PFeeder_in_v_temp;
 uint _PFeeder_cycle_temp;
 float16 __attribute__((memory, numbanks(16), singlepump, numwriteports(1), numreadports(1))) _PFeeder_DB_0_ibuffer[2][3][3][4][4][5][16];
 #pragma unroll
 for (int _PFeeder_s0_cooo_init = 0; _PFeeder_s0_cooo_init < 0 + 8; _PFeeder_s0_cooo_init++)
 {
  bool _5 = _PFeeder_s0_cooo_init == 0;
  if (_5)
  {
   uint _6 = (uint)(ADD_UINT64_T_SUFFIX(4320));
   _PFeeder_cycle_temp = _6;
  } // if _5
 } // for _PFeeder_s0_cooo_init
 int _7 = _P_extent_1 / 900;
 int _8 = _7 * 92160;
 int _9 = _8 + 11520;
 for (int _PFeeder_s0_outermost_loop = 0; _PFeeder_s0_outermost_loop < 0 + _9; _PFeeder_s0_outermost_loop++)
 {
  uint _10 = (uint)(ADD_UINT64_T_SUFFIX(4320));
  uint _11 = _PFeeder_cycle_temp;
  uint _12 = (uint)(ADD_UINT64_T_SUFFIX(11520));
  uint _13 = _11 % _12;
  bool _14 = _10 <= _13;
  uint _15 = _11 / _12;
  int _16 = (int)(_15);
  int _17 = _P_extent_1 / 900;
  int _18 = _17 * 8;
  bool _19 = _16 < _18;
  bool _20 = _14 && _19;
  if (_20)
  {
   float16 __21 = read_channel_intel(_PLoader_channel);
   _PFeeder_in_v_temp = __21;
  } // if _20
  #pragma unroll
  for (int _PFeeder_s0_buf = 0; _PFeeder_s0_buf < 0 + 10; _PFeeder_s0_buf++)
  {
   bool _22 = _PFeeder_s0_buf == 0;
   if (_22)
   {
    float16 _23 = _PFeeder_in_v_temp;
    _PFeeder_value_shreg = _23;
    (void)_23;
    uint _24 = _PFeeder_cycle_temp;
    _PFeeder_time_stamp_shreg = _24;
    (void)_24;
   } // if _22
   else
   {
    float16 _26 = _PFeeder_value_shreg;
    _PFeeder_value_shreg = _26;
    (void)_26;
    uint _28 = _PFeeder_time_stamp_shreg;
    _PFeeder_time_stamp_shreg = _28;
    (void)_28;
   } // if _22 else
   float16 _30 = _PFeeder_value_shreg;
   float16 _31 = __fpga_reg(__fpga_reg(_30));
   _PFeeder_value_shreg = _31;
   (void)_31;
   uint _33 = _PFeeder_time_stamp_shreg;
   uint _34 = __fpga_reg(__fpga_reg(_33));
   _PFeeder_time_stamp_shreg = _34;
   (void)_34;
   uint _35 = (uint)(ADD_UINT64_T_SUFFIX(4320));
   uint _37 = _PFeeder_time_stamp_shreg;
   uint _38 = (uint)(ADD_UINT64_T_SUFFIX(11520));
   uint _39 = _37 % _38;
   bool _40 = _35 <= _39;
   if (_40)
   {
    uint _42 = _PFeeder_time_stamp_shreg;
    uint _43 = (uint)(ADD_UINT64_T_SUFFIX(11520));
    uint _44 = _42 % _43;
    uint _45 = (uint)(ADD_UINT64_T_SUFFIX(4320));
    uint _46 = _44 - _45;
    uint _47 = (uint)(ADD_UINT64_T_SUFFIX(10));
    uint _48 = _46 % _47;
    int _49 = (int)(_48);
    bool _50 = _PFeeder_s0_buf == _49;
    if (_50)
    {
     float16 _52 = _PFeeder_value_shreg;
     uint _54 = _PFeeder_time_stamp_shreg;
     uint _55 = (uint)(ADD_UINT64_T_SUFFIX(11520));
     uint _56 = _54 / _55;
     uint _57 = (uint)(ADD_UINT64_T_SUFFIX(1));
     uint _58 = _56 & _57;
     bool _59 = (bool)(_58);
     uint _61 = _54 % _55;
     uint _62 = (uint)(ADD_UINT64_T_SUFFIX(4320));
     uint _63 = _61 - _62;
     int _64 = (int)(_63);
     int _65 = _64 / 2400;
     int _67 = _64 / 800;
     int _68 = _67 % 3;
     int _70 = _64 / 200;
     int _71 = _70 & 3;
     int _73 = _64 / 50;
     int _74 = _73 & 3;
     int _76 = _64 / 10;
     int _77 = _76 % 5;
     _PFeeder_DB_0_ibuffer[_59][_65][_68][_71][_74][_77][_PFeeder_s0_buf] = _52;
    } // if _50
   } // if _40
   uint _79 = _PFeeder_time_stamp_shreg;
   uint _80 = (uint)(ADD_UINT64_T_SUFFIX(11520));
   uint _81 = _79 / _80;
   int _82 = (int)(_81);
   int _83 = _P_extent_1 / 900;
   int _84 = _83 * 8;
   bool _85 = _82 <= _84;
   uint _86 = (uint)(ADD_UINT64_T_SUFFIX(0));
   bool _88 = _86 < _81;
   bool _89 = _85 && _88;
   if (_89)
   {
    uint _91 = _PFeeder_time_stamp_shreg;
    uint _92 = (uint)(ADD_UINT64_T_SUFFIX(11520));
    uint _93 = _91 / _92;
    uint _94 = (uint)(ADD_UINT64_T_SUFFIX(1));
    uint _95 = _93 & _94;
    bool _96 = (bool)(_95);
    bool _97 = !(_96);
    uint _99 = _91 % _92;
    int _100 = (int)(_99);
    int _101 = _100 / 3840;
    int _103 = _100 / 1280;
    int _104 = _103 % 3;
    int _106 = _100 / 320;
    int _107 = _106 & 3;
    int _109 = _100 / 20;
    int _110 = _109 & 3;
    int _112 = _100 % 5;
    float16 _113 = _PFeeder_DB_0_ibuffer[_97][_101][_104][_107][_110][_112][_PFeeder_s0_buf];
    _PFeeder_channel_array.s[_PFeeder_s0_buf] = _113;
    (void)_PFeeder_s0_buf;
   } // if _89
  } // for _PFeeder_s0_buf
  uint _115 = _PFeeder_time_stamp_shreg;
  uint _116 = (uint)(ADD_UINT64_T_SUFFIX(11520));
  uint _117 = _115 / _116;
  int _118 = (int)(_117);
  int _119 = _P_extent_1 / 900;
  int _120 = _119 * 8;
  bool _121 = _118 <= _120;
  uint _122 = (uint)(ADD_UINT64_T_SUFFIX(0));
  bool _124 = _122 < _117;
  bool _125 = _121 && _124;
  if (_125)
  {
   write_channel_intel(_PFeeder_channel, _PFeeder_channel_array);
   (void)_PFeeder_channel_array;
  } // if _125
  uint _126 = _PFeeder_cycle_temp;
  uint _127 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _128 = _126 + _127;
  _PFeeder_cycle_temp = _128;
 } // for _PFeeder_s0_outermost_loop
} // kernel kernel_PFeeder
// Address spaces for kernel_WLoader
#if 589824 <= MAX_CONSTANT_BUFFER_SIZE && 0 < MAX_CONSTANT_ARGS
#define __address_space__W_serializer_mem_channel __constant
#else
#define __address_space__W_serializer_mem_channel __global
#endif
__kernel void kernel_WLoader(
 const int _P_extent_1,
 __address_space__W_serializer_mem_channel const float *restrict _W_serializer_mem_channel)
{
 int _addr_temp;
 _addr_temp = 0;
 int _129 = _P_extent_1 / 900;
 for (int _WLoader_s0_n = 0; _WLoader_s0_n < 0 + _129; _WLoader_s0_n++)
 {
  for (int _WLoader_s0_mk_ci_kx_ky_coo_my_cooo = 0; _WLoader_s0_mk_ci_kx_ky_coo_my_cooo < 0 + 9216; _WLoader_s0_mk_ci_kx_ky_coo_my_cooo++)
  {
   int _130 = _addr_temp;
   int _131 = _130 % 9216;
   int _132 = _131 * 16;
   float16 _133 = vload16(0, (__address_space__W_serializer_mem_channel float*)_W_serializer_mem_channel + _132);
   write_channel_intel(_WLoader_channel, _133);
   (void)_133;
   int _134 = _130 + 1;
   _addr_temp = _134;
  } // for _WLoader_s0_mk_ci_kx_ky_coo_my_cooo
 } // for _WLoader_s0_n
} // kernel kernel_WLoader
#undef __address_space__W_serializer_mem_channel
// Address spaces for kernel_WFeeder
__kernel void kernel_WFeeder(
 const int _P_extent_1)
{
 _WFeeder_channel_array_t _WFeeder_channel_array;
 float16 _WFeeder_value_shreg;
 uint _WFeeder_time_stamp_shreg;
 float16 _WFeeder_in_v_temp;
 uint _WFeeder_cycle_temp;
 float16 __attribute__((memory, numbanks(8), singlepump, numwriteports(1), numreadports(1))) _WFeeder_DB_0_ibuffer[2][3][3][4][4][8];
 #pragma unroll
 for (int _WFeeder_s0_yyy_xxx_init = 0; _WFeeder_s0_yyy_xxx_init < 0 + 10; _WFeeder_s0_yyy_xxx_init++)
 {
  bool _135 = _WFeeder_s0_yyy_xxx_init == 0;
  if (_135)
  {
   uint _136 = (uint)(ADD_UINT64_T_SUFFIX(10368));
   _WFeeder_cycle_temp = _136;
  } // if _135
 } // for _WFeeder_s0_yyy_xxx_init
 int _137 = _P_extent_1 / 900;
 int _138 = _137 * 92160;
 int _139 = _138 + 11520;
 for (int _WFeeder_s0_outermost_loop = 0; _WFeeder_s0_outermost_loop < 0 + _139; _WFeeder_s0_outermost_loop++)
 {
  uint _140 = (uint)(ADD_UINT64_T_SUFFIX(10368));
  uint _141 = _WFeeder_cycle_temp;
  uint _142 = (uint)(ADD_UINT64_T_SUFFIX(11520));
  uint _143 = _141 % _142;
  bool _144 = _140 <= _143;
  uint _145 = _141 / _142;
  int _146 = (int)(_145);
  int _147 = _P_extent_1 / 900;
  int _148 = _147 * 8;
  bool _149 = _146 < _148;
  bool _150 = _144 && _149;
  if (_150)
  {
   float16 __151 = read_channel_intel(_WLoader_channel);
   _WFeeder_in_v_temp = __151;
  } // if _150
  #pragma unroll
  for (int _WFeeder_s0_buf = 0; _WFeeder_s0_buf < 0 + 8; _WFeeder_s0_buf++)
  {
   bool _152 = _WFeeder_s0_buf == 0;
   if (_152)
   {
    float16 _153 = _WFeeder_in_v_temp;
    _WFeeder_value_shreg = _153;
    (void)_153;
    uint _154 = _WFeeder_cycle_temp;
    _WFeeder_time_stamp_shreg = _154;
    (void)_154;
   } // if _152
   else
   {
    float16 _156 = _WFeeder_value_shreg;
    _WFeeder_value_shreg = _156;
    (void)_156;
    uint _158 = _WFeeder_time_stamp_shreg;
    _WFeeder_time_stamp_shreg = _158;
    (void)_158;
   } // if _152 else
   float16 _160 = _WFeeder_value_shreg;
   float16 _161 = __fpga_reg(__fpga_reg(_160));
   _WFeeder_value_shreg = _161;
   (void)_161;
   uint _163 = _WFeeder_time_stamp_shreg;
   uint _164 = __fpga_reg(__fpga_reg(_163));
   _WFeeder_time_stamp_shreg = _164;
   (void)_164;
   uint _165 = (uint)(ADD_UINT64_T_SUFFIX(10368));
   uint _167 = _WFeeder_time_stamp_shreg;
   uint _168 = (uint)(ADD_UINT64_T_SUFFIX(11520));
   uint _169 = _167 % _168;
   bool _170 = _165 <= _169;
   if (_170)
   {
    uint _172 = _WFeeder_time_stamp_shreg;
    uint _173 = (uint)(ADD_UINT64_T_SUFFIX(11520));
    uint _174 = _172 % _173;
    uint _175 = (uint)(ADD_UINT64_T_SUFFIX(10368));
    uint _176 = _174 - _175;
    uint _177 = (uint)(ADD_UINT64_T_SUFFIX(7));
    uint _178 = _176 & _177;
    int _179 = (int)(_178);
    bool _180 = _WFeeder_s0_buf == _179;
    if (_180)
    {
     float16 _182 = _WFeeder_value_shreg;
     uint _184 = _WFeeder_time_stamp_shreg;
     uint _185 = (uint)(ADD_UINT64_T_SUFFIX(11520));
     uint _186 = _184 / _185;
     uint _187 = (uint)(ADD_UINT64_T_SUFFIX(1));
     uint _188 = _186 & _187;
     bool _189 = (bool)(_188);
     uint _191 = _184 % _185;
     uint _192 = (uint)(ADD_UINT64_T_SUFFIX(10368));
     uint _193 = _191 - _192;
     int _194 = (int)(_193);
     int _195 = _194 / 384;
     int _197 = _194 >> 7;
     int _198 = _197 % 3;
     int _200 = _194 >> 5;
     int _201 = _200 & 3;
     int _203 = _194 >> 3;
     int _204 = _203 & 3;
     _WFeeder_DB_0_ibuffer[_189][_195][_198][_201][_204][_WFeeder_s0_buf] = _182;
    } // if _180
   } // if _170
   uint _206 = _WFeeder_time_stamp_shreg;
   uint _207 = (uint)(ADD_UINT64_T_SUFFIX(11520));
   uint _208 = _206 / _207;
   int _209 = (int)(_208);
   int _210 = _P_extent_1 / 900;
   int _211 = _210 * 8;
   bool _212 = _209 <= _211;
   uint _213 = (uint)(ADD_UINT64_T_SUFFIX(0));
   bool _215 = _213 < _208;
   bool _216 = _212 && _215;
   if (_216)
   {
    uint _218 = _WFeeder_time_stamp_shreg;
    uint _219 = (uint)(ADD_UINT64_T_SUFFIX(11520));
    uint _220 = _218 / _219;
    uint _221 = (uint)(ADD_UINT64_T_SUFFIX(1));
    uint _222 = _220 & _221;
    bool _223 = (bool)(_222);
    bool _224 = !(_223);
    uint _226 = _218 % _219;
    int _227 = (int)(_226);
    int _228 = _227 / 3840;
    int _230 = _227 / 1280;
    int _231 = _230 % 3;
    int _233 = _227 / 80;
    int _234 = _233 & 3;
    int _236 = _227 / 5;
    int _237 = _236 & 3;
    float16 _238 = _WFeeder_DB_0_ibuffer[_224][_228][_231][_234][_237][_WFeeder_s0_buf];
    _WFeeder_channel_array.s[_WFeeder_s0_buf] = _238;
    (void)_WFeeder_s0_buf;
   } // if _216
  } // for _WFeeder_s0_buf
  uint _240 = _WFeeder_time_stamp_shreg;
  uint _241 = (uint)(ADD_UINT64_T_SUFFIX(11520));
  uint _242 = _240 / _241;
  int _243 = (int)(_242);
  int _244 = _P_extent_1 / 900;
  int _245 = _244 * 8;
  bool _246 = _243 <= _245;
  uint _247 = (uint)(ADD_UINT64_T_SUFFIX(0));
  bool _249 = _247 < _242;
  bool _250 = _246 && _249;
  if (_250)
  {
   write_channel_intel(_WFeeder_channel, _WFeeder_channel_array);
   (void)_WFeeder_channel_array;
  } // if _250
  uint _251 = _WFeeder_cycle_temp;
  uint _252 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _253 = _251 + _252;
  _WFeeder_cycle_temp = _253;
 } // for _WFeeder_s0_outermost_loop
} // kernel kernel_WFeeder
// Address spaces for kernel_Out
__kernel void kernel_Out(
 const int _P_extent_1)
{
 _WFeeder_channel_array_t _WFeeder_channel_array;
 _PFeeder_channel_array_t _PFeeder_channel_array;
 // produce C
 float _C_shreg[1280][8][10];
 float _C_pipe_shreg[8][11521];
 // produce B
 float16 _B_shreg[8];
 float _C_temp[8][10];
 // produce A
 float16 _A_shreg[10];
 float _C_shreg_temp;
 int _C_pipe_iter_temp;
 int _C_pipe_base_temp;
 _C_pipe_iter_temp = 12800;
 _C_pipe_base_temp = 0;
 int _254 = _P_extent_1 / 900;
 int _255 = _254 * 8;
 int _256 = _255 + 1;
 for (int _A_s0_n_mk_ci = 0; _A_s0_n_mk_ci < 0 + _256; _A_s0_n_mk_ci++)
 {
  for (int _A_s0_kx_ky_nn_coo_mx_my_y_x = 0; _A_s0_kx_ky_nn_coo_mx_my_y_x < 0 + 11520; _A_s0_kx_ky_nn_coo_mx_my_y_x++)
  {
   #pragma unroll
   for (int _dummy__1_s0_yyy_xxx = 0; _dummy__1_s0_yyy_xxx < 0 + 10; _dummy__1_s0_yyy_xxx++)
   {
    #pragma unroll
    for (int _dummy_s0_cooo = 0; _dummy_s0_cooo < 0 + 8; _dummy_s0_cooo++)
    {
     float _258 = _C_shreg[1279][_dummy_s0_cooo][_dummy__1_s0_yyy_xxx];
     _C_temp[_dummy_s0_cooo][_dummy__1_s0_yyy_xxx] = _258;
     #pragma unroll
     for (int _dummy__2_s0_l1 = 0; _dummy__2_s0_l1 < 0 + 1279; _dummy__2_s0_l1++)
     {
      int _259 = 1279 - _dummy__2_s0_l1;
      int _260 = 1278 - _dummy__2_s0_l1;
      float _262 = _C_shreg[_260][_dummy_s0_cooo][_dummy__1_s0_yyy_xxx];
      _C_shreg[_259][_dummy_s0_cooo][_dummy__1_s0_yyy_xxx] = _262;
      (void)_262;
     } // for _dummy__2_s0_l1
     float _263 = _C_temp[_dummy_s0_cooo][_dummy__1_s0_yyy_xxx];
     _C_shreg[0][_dummy_s0_cooo][_dummy__1_s0_yyy_xxx] = _263;
     (void)_263;
    } // for _dummy_s0_cooo
   } // for _dummy__1_s0_yyy_xxx
   int _264 = _P_extent_1 / 900;
   int _265 = _264 * 8;
   bool _266 = _A_s0_n_mk_ci < _265;
   if (_266)
   {
    _WFeeder_channel_array_t __267 = read_channel_intel(_WFeeder_channel);
    _WFeeder_channel_array = __267;
    (void)__267;
    _PFeeder_channel_array_t __268 = read_channel_intel(_PFeeder_channel);
    _PFeeder_channel_array = __268;
    (void)__268;
   } // if _266
   #pragma unroll
   for (int _A_s0_yyy_xxx = 0; _A_s0_yyy_xxx < 0 + 10; _A_s0_yyy_xxx++)
   {
    #pragma unroll
    for (int _A_s0_cooo = 0; _A_s0_cooo < 0 + 8; _A_s0_cooo++)
    {
     float16 _269;
     bool _270 = _A_s0_cooo == 0;
     if (_270)
     {
      float16 __271 = _PFeeder_channel_array.s[_A_s0_yyy_xxx];
      _269 = __271;
     } // if _270
     else
     {
      float16 _273 = _A_shreg[_A_s0_yyy_xxx];
      _269 = _273;
     } // if _270 else
     float16 _274 = _269;
     _A_shreg[_A_s0_yyy_xxx] = _274;
     (void)_274;
     float16 _276 = _A_shreg[_A_s0_yyy_xxx];
     float16 _277 = __fpga_reg(__fpga_reg(_276));
     _A_shreg[_A_s0_yyy_xxx] = _277;
     (void)_277;
     float16 _278;
     bool _279 = _A_s0_yyy_xxx == 0;
     if (_279)
     {
      float16 __280 = _WFeeder_channel_array.s[_A_s0_cooo];
      _278 = __280;
     } // if _279
     else
     {
      float16 _282 = _B_shreg[_A_s0_cooo];
      _278 = _282;
     } // if _279 else
     float16 _283 = _278;
     _B_shreg[_A_s0_cooo] = _283;
     (void)_283;
     float16 _285 = _B_shreg[_A_s0_cooo];
     float16 _286 = __fpga_reg(__fpga_reg(_285));
     _B_shreg[_A_s0_cooo] = _286;
     (void)_286;
     float _287;
     int _288 = _A_s0_kx_ky_nn_coo_mx_my_y_x / 3840;
     bool _289 = _288 == 0;
     int _290 = _A_s0_kx_ky_nn_coo_mx_my_y_x % 3840;
     int _291 = _290 / 1280;
     bool _292 = _291 == 0;
     bool _293 = _289 && _292;
     int _294 = _A_s0_n_mk_ci & 7;
     int _295 = _294 >> 1;
     bool _296 = _295 == 0;
     bool _297 = _293 && _296;
     int _298 = _A_s0_n_mk_ci & 1;
     bool _299 = _298 == 0;
     bool _300 = _297 && _299;
     if (_300)
     {
      float _301 = float_from_bits(0 /* 0 */);
      _287 = _301;
     } // if _300
     else
     {
      float _303 = _C_shreg[0][_A_s0_cooo][_A_s0_yyy_xxx];
      float _304 = __fpga_reg(_303);
      _287 = _304;
     } // if _300 else
     float _305 = _287;
     _C_shreg_temp = _305;
     #pragma unroll
     for (int _A_s0_cii = 0; _A_s0_cii < 0 + 16; _A_s0_cii++)
     {
      float _306 = _C_shreg_temp;
      float _308 = _A_shreg[_A_s0_yyy_xxx][_A_s0_cii];
      float _310 = _B_shreg[_A_s0_cooo][_A_s0_cii];
      float _311 = _308 * _310;
      float _312 = _306 + _311;
      _C_shreg_temp = _312;
      int _313 = _A_s0_cii & 3;
      bool _314 = _313 == 3;
      if (_314)
      {
       float _315 = _C_shreg_temp;
       float _316 = __fpga_reg(_315);
       _C_shreg_temp = _316;
      } // if _314
     } // for _A_s0_cii
     float _317 = _C_shreg_temp;
     _C_shreg[0][_A_s0_cooo][_A_s0_yyy_xxx] = _317;
     (void)_317;
     #pragma unroll
     for (int _A_s0_cii = 0; _A_s0_cii < 0 + 16; _A_s0_cii++)
     {
      bool _318 = _A_s0_cii == 15;
      int _319 = _A_s0_n_mk_ci & 1;
      bool _320 = _319 == 1;
      bool _321 = _318 && _320;
      int _322 = _A_s0_n_mk_ci & 7;
      int _323 = _322 >> 1;
      bool _324 = _323 == 3;
      bool _325 = _321 && _324;
      int _326 = _A_s0_kx_ky_nn_coo_mx_my_y_x % 3840;
      int _327 = _326 / 1280;
      bool _328 = _327 == 2;
      bool _329 = _325 && _328;
      int _330 = _A_s0_kx_ky_nn_coo_mx_my_y_x / 3840;
      bool _331 = _330 == 2;
      bool _332 = _329 && _331;
      if (_332)
      {
       int _333 = _A_s0_yyy_xxx * 1280;
       float _335 = _C_shreg[0][_A_s0_cooo][_A_s0_yyy_xxx];
       _C_pipe_shreg[_A_s0_cooo][_333] = _335;
       (void)_335;
      } // if _332
     } // for _A_s0_cii
    } // for _A_s0_cooo
   } // for _A_s0_yyy_xxx
   int _336 = _A_s0_kx_ky_nn_coo_mx_my_y_x / 3840;
   bool _337 = _336 == 2;
   int _338 = _A_s0_kx_ky_nn_coo_mx_my_y_x % 3840;
   int _339 = _338 / 1280;
   bool _340 = _339 == 2;
   bool _341 = _337 && _340;
   int _342 = _A_s0_n_mk_ci & 7;
   int _343 = _342 >> 1;
   bool _344 = _343 == 3;
   bool _345 = _341 && _344;
   int _346 = _A_s0_n_mk_ci & 1;
   bool _347 = _346 == 1;
   bool _348 = _345 && _347;
   int _349 = _A_s0_kx_ky_nn_coo_mx_my_y_x % 5;
   bool _350 = _349 == 0;
   int _351 = _A_s0_kx_ky_nn_coo_mx_my_y_x % 20;
   int _352 = _351 / 5;
   bool _353 = _352 == 0;
   bool _354 = _350 && _353;
   int _355 = _A_s0_kx_ky_nn_coo_mx_my_y_x % 80;
   int _356 = _355 / 20;
   bool _357 = _356 == 0;
   bool _358 = _354 && _357;
   int _359 = _A_s0_kx_ky_nn_coo_mx_my_y_x % 320;
   int _360 = _359 / 80;
   bool _361 = _360 == 0;
   bool _362 = _358 && _361;
   int _363 = _A_s0_kx_ky_nn_coo_mx_my_y_x % 1280;
   int _364 = _363 / 320;
   bool _365 = _364 == 0;
   bool _366 = _362 && _365;
   bool _367 = _348 && _366;
   if (_367)
   {
    int _368 = _C_pipe_iter_temp;
    _C_pipe_base_temp = _368;
   } // if _367
   float8 _Out_channel_temp;
   #pragma unroll
   for (int _C_pipe_b__254 = 0; _C_pipe_b__254 < 0 + 8; _C_pipe_b__254++)
   {
    float _370 = _C_pipe_shreg[_C_pipe_b__254][0];
    _Out_channel_temp[_C_pipe_b__254] = _370;
    #pragma unroll
    for (int _C_pipe_b__254_dummy = 0; _C_pipe_b__254_dummy < 0 + 8; _C_pipe_b__254_dummy++)
    {
     float _371 = _Out_channel_temp[_C_pipe_b__254_dummy];
     float _372 = __fpga_reg(__fpga_reg(_371));
     _Out_channel_temp[_C_pipe_b__254_dummy] = _372;
    } // for _C_pipe_b__254_dummy
   } // for _C_pipe_b__254
   int _373 = _C_pipe_iter_temp;
   int _374 = _C_pipe_base_temp;
   int _375 = _374 + 12800;
   bool _376 = _373 < _375;
   if (_376)
   {
    float8 _377 = _Out_channel_temp;
    write_channel_intel(_Out_channel, _377);
    (void)_377;
   } // if _376
   #pragma unroll
   for (int _C_pipe_b__255 = 0; _C_pipe_b__255 < 0 + 8; _C_pipe_b__255++)
   {
    #pragma unroll
    for (int _C_pipe_p__127 = 0; _C_pipe_p__127 < 0 + 9; _C_pipe_p__127++)
    {
     #pragma unroll
     for (int _C_pipe_l__127 = 0; _C_pipe_l__127 < 0 + 1279; _C_pipe_l__127++)
     {
      int _378 = _C_pipe_p__127 * 1280;
      int _379 = _378 + _C_pipe_l__127;
      int _380 = _379 + 1;
      float _382 = _C_pipe_shreg[_C_pipe_b__255][_380];
      _C_pipe_shreg[_C_pipe_b__255][_379] = _382;
      (void)_382;
     } // for _C_pipe_l__127
     int _383 = _C_pipe_p__127 * 1280;
     int _384 = _383 + 1279;
     int _385 = _383 + 1280;
     float _387 = _C_pipe_shreg[_C_pipe_b__255][_385];
     float _388 = __fpga_reg(__fpga_reg(_387));
     _C_pipe_shreg[_C_pipe_b__255][_384] = _388;
     (void)_388;
    } // for _C_pipe_p__127
   } // for _C_pipe_b__255
   int _389 = _C_pipe_iter_temp;
   int _390 = _389 + 1;
   _C_pipe_iter_temp = _390;
  } // for _A_s0_kx_ky_nn_coo_mx_my_y_x
 } // for _A_s0_n_mk_ci
} // kernel kernel_Out
// Address spaces for kernel_unloader
#define __address_space__unloader_mem_channel __global
__kernel void kernel_unloader(
 const int _P_extent_1,
 __address_space__unloader_mem_channel float *restrict _unloader_mem_channel)
{
 int _addr_temp;
 _addr_temp = 0;
 int _391 = _P_extent_1 / 900;
 for (int _unloader_s0_n = 0; _unloader_s0_n < 0 + _391; _unloader_s0_n++)
 {
  for (int _unloader_s0_yyy_xxx_nn_coo_mx_my_y_x = 0; _unloader_s0_yyy_xxx_nn_coo_mx_my_y_x < 0 + 12800; _unloader_s0_yyy_xxx_nn_coo_mx_my_y_x++)
  {
   float8 __392 = read_channel_intel(_Out_channel);
   int _393 = _addr_temp;
   int _394 = _393 * 8;
   vstore8(__392, 0, (__address_space__unloader_mem_channel float*)_unloader_mem_channel + _394);
   int _395 = _addr_temp;
   int _396 = _395 + 1;
   _addr_temp = _396;
  } // for _unloader_s0_yyy_xxx_nn_coo_mx_my_y_x
 } // for _unloader_s0_n
} // kernel kernel_unloader
#undef __address_space__unloader_mem_channel

