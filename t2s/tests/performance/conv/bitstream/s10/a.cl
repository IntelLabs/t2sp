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
channel float16 _iLoader_channel __attribute__((depth(256))) ;
typedef struct { float16 s[14]; } _iFeeder_channel_array_t;
channel _iFeeder_channel_array_t _iFeeder_channel __attribute__((depth(256))) ;
channel float16 _kLoader_channel __attribute__((depth(256))) ;
typedef struct { float16 s[16]; } _kFeeder_channel_array_t;
channel _kFeeder_channel_array_t _kFeeder_channel __attribute__((depth(256))) ;
channel float16 _Out_channel __attribute__((depth(256))) ;
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
  for (int _iLoader_s0_x_y_ci_kx_ky_xx_yy_xxx_yyy = 0; _iLoader_s0_x_y_ci_kx_ky_xx_yy_xxx_yyy < 0 + 516096; _iLoader_s0_x_y_ci_kx_ky_xx_yy_xxx_yyy++)
  {
   int _1 = _addr_temp;
   int _2 = _1 * 16;
   float16 _3 = vload16(0, (__address_space__I_serializer_mem_channel float*)_I_serializer_mem_channel + _2);
   write_channel_intel(_iLoader_channel, _3);
   (void)_3;
   int _4 = _1 + 1;
   _addr_temp = _4;
  } // for _iLoader_s0_x_y_ci_kx_ky_xx_yy_xxx_yyy
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
 for (int _iFeeder_s0_cooo_init = 0; _iFeeder_s0_cooo_init < 0 + 16; _iFeeder_s0_cooo_init++)
 {
  bool _5 = _iFeeder_s0_cooo_init == 0;
  if (_5)
  {
   uint _6 = (uint)(ADD_UINT64_T_SUFFIX(384));
   _iFeeder_cycle_temp = _6;
  } // if _5
 } // for _iFeeder_s0_cooo_init
 int _7 = _I_extent_0 >> 8;
 int _8 = _7 * 589824;
 int _9 = _8 + 3072;
 for (int _iFeeder_s0_outermost_loop = 0; _iFeeder_s0_outermost_loop < 0 + _9; _iFeeder_s0_outermost_loop++)
 {
  uint _10 = (uint)(ADD_UINT64_T_SUFFIX(384));
  uint _11 = _iFeeder_cycle_temp;
  uint _12 = (uint)(ADD_UINT64_T_SUFFIX(3072));
  uint _13 = _11 % _12;
  bool _14 = _10 <= _13;
  uint _15 = _11 / _12;
  int _16 = (int)(_15);
  int _17 = _I_extent_0 >> 8;
  int _18 = _17 * 192;
  bool _19 = _16 < _18;
  bool _20 = _14 && _19;
  if (_20)
  {
   float16 __21 = read_channel_intel(_iLoader_channel);
   _iFeeder_in_v_temp = __21;
  } // if _20
  #pragma unroll
  for (int _iFeeder_s0_buf = 0; _iFeeder_s0_buf < 0 + 14; _iFeeder_s0_buf++)
  {
   bool _22 = _iFeeder_s0_buf == 0;
   if (_22)
   {
    float16 _23 = _iFeeder_in_v_temp;
    _iFeeder_value_shreg = _23;
    (void)_23;
    uint _24 = _iFeeder_cycle_temp;
    _iFeeder_time_stamp_shreg = _24;
    (void)_24;
   } // if _22
   else
   {
    float16 _26 = _iFeeder_value_shreg;
    _iFeeder_value_shreg = _26;
    (void)_26;
    uint _28 = _iFeeder_time_stamp_shreg;
    _iFeeder_time_stamp_shreg = _28;
    (void)_28;
   } // if _22 else
   float16 _30 = _iFeeder_value_shreg;
   float16 _31 = __fpga_reg(__fpga_reg(_30));
   _iFeeder_value_shreg = _31;
   (void)_31;
   uint _33 = _iFeeder_time_stamp_shreg;
   uint _34 = __fpga_reg(__fpga_reg(_33));
   _iFeeder_time_stamp_shreg = _34;
   (void)_34;
   uint _35 = (uint)(ADD_UINT64_T_SUFFIX(384));
   uint _37 = _iFeeder_time_stamp_shreg;
   uint _38 = (uint)(ADD_UINT64_T_SUFFIX(3072));
   uint _39 = _37 % _38;
   bool _40 = _35 <= _39;
   if (_40)
   {
    uint _42 = _iFeeder_time_stamp_shreg;
    uint _43 = (uint)(ADD_UINT64_T_SUFFIX(3072));
    uint _44 = _42 % _43;
    uint _45 = (uint)(ADD_UINT64_T_SUFFIX(384));
    uint _46 = _44 - _45;
    uint _47 = (uint)(ADD_UINT64_T_SUFFIX(14));
    uint _48 = _46 % _47;
    int _49 = (int)(_48);
    bool _50 = _iFeeder_s0_buf == _49;
    if (_50)
    {
     float16 _52 = _iFeeder_value_shreg;
     uint _54 = _iFeeder_time_stamp_shreg;
     uint _55 = (uint)(ADD_UINT64_T_SUFFIX(3072));
     uint _56 = _54 / _55;
     uint _57 = (uint)(ADD_UINT64_T_SUFFIX(1));
     uint _58 = _56 & _57;
     bool _59 = (bool)(_58);
     uint _61 = _54 % _55;
     uint _62 = (uint)(ADD_UINT64_T_SUFFIX(384));
     uint _63 = _61 - _62;
     int _64 = (int)(_63);
     int _65 = _64 / 896;
     int _67 = _64 / 448;
     int _68 = _67 & 1;
     int _70 = _64 / 224;
     int _71 = _70 & 1;
     int _73 = _64 / 14;
     int _74 = _73 & 15;
     _iFeeder_DB_0_ibuffer[_59][_65][_68][_71][_74][_iFeeder_s0_buf] = _52;
    } // if _50
   } // if _40
   uint _76 = _iFeeder_time_stamp_shreg;
   uint _77 = (uint)(ADD_UINT64_T_SUFFIX(3072));
   uint _78 = _76 / _77;
   int _79 = (int)(_78);
   int _80 = _I_extent_0 >> 8;
   int _81 = _80 * 192;
   bool _82 = _79 <= _81;
   uint _83 = (uint)(ADD_UINT64_T_SUFFIX(0));
   bool _85 = _83 < _78;
   bool _86 = _82 && _85;
   if (_86)
   {
    uint _88 = _iFeeder_time_stamp_shreg;
    uint _89 = (uint)(ADD_UINT64_T_SUFFIX(3072));
    uint _90 = _88 / _89;
    uint _91 = (uint)(ADD_UINT64_T_SUFFIX(1));
    uint _92 = _90 & _91;
    bool _93 = (bool)(_92);
    bool _94 = !(_93);
    uint _96 = _88 % _89;
    int _97 = (int)(_96);
    int _98 = _97 >> 10;
    int _100 = _97 >> 9;
    int _101 = _100 & 1;
    int _103 = _97 >> 8;
    int _104 = _103 & 1;
    int _106 = _97 & 15;
    float16 _107 = _iFeeder_DB_0_ibuffer[_94][_98][_101][_104][_106][_iFeeder_s0_buf];
    _iFeeder_channel_array.s[_iFeeder_s0_buf] = _107;
    (void)_iFeeder_s0_buf;
   } // if _86
  } // for _iFeeder_s0_buf
  uint _109 = _iFeeder_time_stamp_shreg;
  uint _110 = (uint)(ADD_UINT64_T_SUFFIX(3072));
  uint _111 = _109 / _110;
  int _112 = (int)(_111);
  int _113 = _I_extent_0 >> 8;
  int _114 = _113 * 192;
  bool _115 = _112 <= _114;
  uint _116 = (uint)(ADD_UINT64_T_SUFFIX(0));
  bool _118 = _116 < _111;
  bool _119 = _115 && _118;
  if (_119)
  {
   write_channel_intel(_iFeeder_channel, _iFeeder_channel_array);
   (void)_iFeeder_channel_array;
  } // if _119
  uint _120 = _iFeeder_cycle_temp;
  uint _121 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _122 = _120 + _121;
  _iFeeder_cycle_temp = _122;
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
 int _123 = _I_extent_0 >> 8;
 for (int _kLoader_s0_n = 0; _kLoader_s0_n < 0 + _123; _kLoader_s0_n++)
 {
  for (int _kLoader_s0_x_y_ci_kx_ky_coo_cooo = 0; _kLoader_s0_x_y_ci_kx_ky_coo_cooo < 0 + 147456; _kLoader_s0_x_y_ci_kx_ky_coo_cooo++)
  {
   int _124 = _addr_temp;
   int _125 = _124 % 36864;
   int _126 = _125 * 16;
   float16 _127 = vload16(0, (__address_space__K_serializer_mem_channel float*)_K_serializer_mem_channel + _126);
   write_channel_intel(_kLoader_channel, _127);
   (void)_127;
   int _128 = _124 + 1;
   _addr_temp = _128;
  } // for _kLoader_s0_x_y_ci_kx_ky_coo_cooo
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
 float16 __attribute__((memory, numbanks(16), singlepump, numwriteports(1), numreadports(1))) _kFeeder_DB_0_ibuffer[2][3][16][16];
 #pragma unroll
 for (int _kFeeder_s0_yyy_init = 0; _kFeeder_s0_yyy_init < 0 + 14; _kFeeder_s0_yyy_init++)
 {
  bool _129 = _kFeeder_s0_yyy_init == 0;
  if (_129)
  {
   uint _130 = (uint)(ADD_UINT64_T_SUFFIX(2304));
   _kFeeder_cycle_temp = _130;
  } // if _129
 } // for _kFeeder_s0_yyy_init
 int _131 = _I_extent_0 >> 8;
 int _132 = _131 * 589824;
 int _133 = _132 + 3072;
 for (int _kFeeder_s0_outermost_loop = 0; _kFeeder_s0_outermost_loop < 0 + _133; _kFeeder_s0_outermost_loop++)
 {
  uint _134 = (uint)(ADD_UINT64_T_SUFFIX(2304));
  uint _135 = _kFeeder_cycle_temp;
  uint _136 = (uint)(ADD_UINT64_T_SUFFIX(3072));
  uint _137 = _135 % _136;
  bool _138 = _134 <= _137;
  uint _139 = _135 / _136;
  int _140 = (int)(_139);
  int _141 = _I_extent_0 >> 8;
  int _142 = _141 * 192;
  bool _143 = _140 < _142;
  bool _144 = _138 && _143;
  if (_144)
  {
   float16 __145 = read_channel_intel(_kLoader_channel);
   _kFeeder_in_v_temp = __145;
  } // if _144
  #pragma unroll
  for (int _kFeeder_s0_buf = 0; _kFeeder_s0_buf < 0 + 16; _kFeeder_s0_buf++)
  {
   bool _146 = _kFeeder_s0_buf == 0;
   if (_146)
   {
    float16 _147 = _kFeeder_in_v_temp;
    _kFeeder_value_shreg = _147;
    (void)_147;
    uint _148 = _kFeeder_cycle_temp;
    _kFeeder_time_stamp_shreg = _148;
    (void)_148;
   } // if _146
   else
   {
    float16 _150 = _kFeeder_value_shreg;
    _kFeeder_value_shreg = _150;
    (void)_150;
    uint _152 = _kFeeder_time_stamp_shreg;
    _kFeeder_time_stamp_shreg = _152;
    (void)_152;
   } // if _146 else
   float16 _154 = _kFeeder_value_shreg;
   float16 _155 = __fpga_reg(__fpga_reg(_154));
   _kFeeder_value_shreg = _155;
   (void)_155;
   uint _157 = _kFeeder_time_stamp_shreg;
   uint _158 = __fpga_reg(__fpga_reg(_157));
   _kFeeder_time_stamp_shreg = _158;
   (void)_158;
   uint _159 = (uint)(ADD_UINT64_T_SUFFIX(2304));
   uint _161 = _kFeeder_time_stamp_shreg;
   uint _162 = (uint)(ADD_UINT64_T_SUFFIX(3072));
   uint _163 = _161 % _162;
   bool _164 = _159 <= _163;
   if (_164)
   {
    uint _166 = _kFeeder_time_stamp_shreg;
    uint _167 = (uint)(ADD_UINT64_T_SUFFIX(3072));
    uint _168 = _166 % _167;
    uint _169 = (uint)(ADD_UINT64_T_SUFFIX(2304));
    uint _170 = _168 - _169;
    uint _171 = (uint)(ADD_UINT64_T_SUFFIX(15));
    uint _172 = _170 & _171;
    int _173 = (int)(_172);
    bool _174 = _kFeeder_s0_buf == _173;
    if (_174)
    {
     float16 _176 = _kFeeder_value_shreg;
     uint _178 = _kFeeder_time_stamp_shreg;
     uint _179 = (uint)(ADD_UINT64_T_SUFFIX(3072));
     uint _180 = _178 / _179;
     uint _181 = (uint)(ADD_UINT64_T_SUFFIX(1));
     uint _182 = _180 & _181;
     bool _183 = (bool)(_182);
     uint _185 = _178 % _179;
     uint _186 = (uint)(ADD_UINT64_T_SUFFIX(2304));
     uint _187 = _185 - _186;
     int _188 = (int)(_187);
     int _189 = _188 >> 8;
     int _191 = _188 >> 4;
     int _192 = _191 & 15;
     _kFeeder_DB_0_ibuffer[_183][_189][_192][_kFeeder_s0_buf] = _176;
    } // if _174
   } // if _164
   uint _194 = _kFeeder_time_stamp_shreg;
   uint _195 = (uint)(ADD_UINT64_T_SUFFIX(3072));
   uint _196 = _194 / _195;
   int _197 = (int)(_196);
   int _198 = _I_extent_0 >> 8;
   int _199 = _198 * 192;
   bool _200 = _197 <= _199;
   uint _201 = (uint)(ADD_UINT64_T_SUFFIX(0));
   bool _203 = _201 < _196;
   bool _204 = _200 && _203;
   if (_204)
   {
    uint _206 = _kFeeder_time_stamp_shreg;
    uint _207 = (uint)(ADD_UINT64_T_SUFFIX(3072));
    uint _208 = _206 / _207;
    uint _209 = (uint)(ADD_UINT64_T_SUFFIX(1));
    uint _210 = _208 & _209;
    bool _211 = (bool)(_210);
    bool _212 = !(_211);
    uint _214 = _206 % _207;
    int _215 = (int)(_214);
    int _216 = _215 >> 10;
    int _218 = _215 >> 4;
    int _219 = _218 & 15;
    float16 _220 = _kFeeder_DB_0_ibuffer[_212][_216][_219][_kFeeder_s0_buf];
    _kFeeder_channel_array.s[_kFeeder_s0_buf] = _220;
    (void)_kFeeder_s0_buf;
   } // if _204
  } // for _kFeeder_s0_buf
  uint _222 = _kFeeder_time_stamp_shreg;
  uint _223 = (uint)(ADD_UINT64_T_SUFFIX(3072));
  uint _224 = _222 / _223;
  int _225 = (int)(_224);
  int _226 = _I_extent_0 >> 8;
  int _227 = _226 * 192;
  bool _228 = _225 <= _227;
  uint _229 = (uint)(ADD_UINT64_T_SUFFIX(0));
  bool _231 = _229 < _224;
  bool _232 = _228 && _231;
  if (_232)
  {
   write_channel_intel(_kFeeder_channel, _kFeeder_channel_array);
   (void)_kFeeder_channel_array;
  } // if _232
  uint _233 = _kFeeder_cycle_temp;
  uint _234 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _235 = _233 + _234;
  _kFeeder_cycle_temp = _235;
 } // for _kFeeder_s0_outermost_loop
} // kernel kernel_kFeeder
// Address spaces for kernel_Out
__kernel void kernel_Out(
 const int _I_extent_0)
{
 _kFeeder_channel_array_t _kFeeder_channel_array;
 _iFeeder_channel_array_t _iFeeder_channel_array;
 // produce C
 float _C_shreg[1024][16][14];
 float _C_pipe_shreg[16][13313];
 // produce B
 float16 _B_shreg[16];
 float _C_temp[16][14];
 // produce A
 float16 _A_shreg[14];
 float _C_shreg_temp;
 int _C_pipe_iter_temp;
 int _C_pipe_base_temp;
 _C_pipe_iter_temp = 14336;
 _C_pipe_base_temp = 0;
 int _236 = _I_extent_0 >> 8;
 int _237 = _236 * 4;
 int _238 = _237 + 1;
 for (int _A_s0_n_x_y = 0; _A_s0_n_x_y < 0 + _238; _A_s0_n_x_y++)
 {
  for (int _A_s0_ci_kx_ky_xx_yy_coo_xxx = 0; _A_s0_ci_kx_ky_xx_yy_coo_xxx < 0 + 147456; _A_s0_ci_kx_ky_xx_yy_coo_xxx++)
  {
   #pragma unroll
   for (int _dummy__1_s0_yyy = 0; _dummy__1_s0_yyy < 0 + 14; _dummy__1_s0_yyy++)
   {
    #pragma unroll
    for (int _dummy_s0_cooo = 0; _dummy_s0_cooo < 0 + 16; _dummy_s0_cooo++)
    {
     float _240 = _C_shreg[1023][_dummy_s0_cooo][_dummy__1_s0_yyy];
     _C_temp[_dummy_s0_cooo][_dummy__1_s0_yyy] = _240;
     #pragma unroll
     for (int _dummy__2_s0_l1 = 0; _dummy__2_s0_l1 < 0 + 1023; _dummy__2_s0_l1++)
     {
      int _241 = 1023 - _dummy__2_s0_l1;
      int _242 = 1022 - _dummy__2_s0_l1;
      float _244 = _C_shreg[_242][_dummy_s0_cooo][_dummy__1_s0_yyy];
      _C_shreg[_241][_dummy_s0_cooo][_dummy__1_s0_yyy] = _244;
      (void)_244;
     } // for _dummy__2_s0_l1
     float _245 = _C_temp[_dummy_s0_cooo][_dummy__1_s0_yyy];
     _C_shreg[0][_dummy_s0_cooo][_dummy__1_s0_yyy] = _245;
     (void)_245;
    } // for _dummy_s0_cooo
   } // for _dummy__1_s0_yyy
   int _246 = _I_extent_0 >> 8;
   int _247 = _246 * 4;
   bool _248 = _A_s0_n_x_y < _247;
   if (_248)
   {
    _kFeeder_channel_array_t __249 = read_channel_intel(_kFeeder_channel);
    _kFeeder_channel_array = __249;
    (void)__249;
    _iFeeder_channel_array_t __250 = read_channel_intel(_iFeeder_channel);
    _iFeeder_channel_array = __250;
    (void)__250;
   } // if _248
   #pragma unroll
   for (int _A_s0_yyy = 0; _A_s0_yyy < 0 + 14; _A_s0_yyy++)
   {
    #pragma unroll
    for (int _A_s0_cooo = 0; _A_s0_cooo < 0 + 16; _A_s0_cooo++)
    {
     float16 _251;
     bool _252 = _A_s0_cooo == 0;
     if (_252)
     {
      float16 __253 = _iFeeder_channel_array.s[_A_s0_yyy];
      _251 = __253;
     } // if _252
     else
     {
      float16 _255 = _A_shreg[_A_s0_yyy];
      _251 = _255;
     } // if _252 else
     float16 _256 = _251;
     _A_shreg[_A_s0_yyy] = _256;
     (void)_256;
     float16 _258 = _A_shreg[_A_s0_yyy];
     float16 _259 = __fpga_reg(__fpga_reg(_258));
     _A_shreg[_A_s0_yyy] = _259;
     (void)_259;
     float16 _260;
     bool _261 = _A_s0_yyy == 0;
     if (_261)
     {
      float16 __262 = _kFeeder_channel_array.s[_A_s0_cooo];
      _260 = __262;
     } // if _261
     else
     {
      float16 _264 = _B_shreg[_A_s0_cooo];
      _260 = _264;
     } // if _261 else
     float16 _265 = _260;
     _B_shreg[_A_s0_cooo] = _265;
     (void)_265;
     float16 _267 = _B_shreg[_A_s0_cooo];
     float16 _268 = __fpga_reg(__fpga_reg(_267));
     _B_shreg[_A_s0_cooo] = _268;
     (void)_268;
     float _269;
     int _270 = _A_s0_ci_kx_ky_xx_yy_coo_xxx / 9216;
     bool _271 = _270 == 0;
     int _272 = _A_s0_ci_kx_ky_xx_yy_coo_xxx % 9216;
     int _273 = _272 / 3072;
     bool _274 = _273 == 0;
     bool _275 = _271 && _274;
     int _276 = _A_s0_ci_kx_ky_xx_yy_coo_xxx % 3072;
     int _277 = _276 >> 10;
     bool _278 = _277 == 0;
     bool _279 = _275 && _278;
     if (_279)
     {
      float _280 = float_from_bits(0 /* 0 */);
      _269 = _280;
     } // if _279
     else
     {
      float _282 = _C_shreg[0][_A_s0_cooo][_A_s0_yyy];
      float _283 = __fpga_reg(_282);
      _269 = _283;
     } // if _279 else
     float _284 = _269;
     _C_shreg_temp = _284;
     #pragma unroll
     for (int _A_s0_cii = 0; _A_s0_cii < 0 + 16; _A_s0_cii++)
     {
      float _285 = _C_shreg_temp;
      float _287 = _A_shreg[_A_s0_yyy][_A_s0_cii];
      float _289 = _B_shreg[_A_s0_cooo][_A_s0_cii];
      float _290 = _287 * _289;
      float _291 = _285 + _290;
      _C_shreg_temp = _291;
      int _292 = _A_s0_cii & 3;
      bool _293 = _292 == 3;
      if (_293)
      {
       float _294 = _C_shreg_temp;
       float _295 = __fpga_reg(_294);
       _C_shreg_temp = _295;
      } // if _293
     } // for _A_s0_cii
     float _296 = _C_shreg_temp;
     _C_shreg[0][_A_s0_cooo][_A_s0_yyy] = _296;
     (void)_296;
     #pragma unroll
     for (int _A_s0_cii = 0; _A_s0_cii < 0 + 16; _A_s0_cii++)
     {
      bool _297 = _A_s0_cii == 15;
      int _298 = _A_s0_ci_kx_ky_xx_yy_coo_xxx % 3072;
      int _299 = _298 >> 10;
      bool _300 = _299 == 2;
      bool _301 = _297 && _300;
      int _302 = _A_s0_ci_kx_ky_xx_yy_coo_xxx % 9216;
      int _303 = _302 / 3072;
      bool _304 = _303 == 2;
      bool _305 = _301 && _304;
      int _306 = _A_s0_ci_kx_ky_xx_yy_coo_xxx / 9216;
      bool _307 = _306 == 15;
      bool _308 = _305 && _307;
      if (_308)
      {
       int _309 = _A_s0_yyy * 1024;
       float _311 = _C_shreg[0][_A_s0_cooo][_A_s0_yyy];
       _C_pipe_shreg[_A_s0_cooo][_309] = _311;
       (void)_311;
      } // if _308
     } // for _A_s0_cii
    } // for _A_s0_cooo
   } // for _A_s0_yyy
   int _312 = _A_s0_ci_kx_ky_xx_yy_coo_xxx & 15;
   bool _313 = _312 == 0;
   int _314 = _A_s0_ci_kx_ky_xx_yy_coo_xxx & 255;
   int _315 = _314 >> 4;
   bool _316 = _315 == 0;
   bool _317 = _313 && _316;
   int _318 = _A_s0_ci_kx_ky_xx_yy_coo_xxx & 511;
   int _319 = _318 >> 8;
   bool _320 = _319 == 0;
   bool _321 = _317 && _320;
   int _322 = _A_s0_ci_kx_ky_xx_yy_coo_xxx & 1023;
   int _323 = _322 >> 9;
   bool _324 = _323 == 0;
   bool _325 = _321 && _324;
   int _326 = _A_s0_ci_kx_ky_xx_yy_coo_xxx / 9216;
   bool _327 = _326 == 15;
   bool _328 = _325 && _327;
   int _329 = _A_s0_ci_kx_ky_xx_yy_coo_xxx % 9216;
   int _330 = _329 / 3072;
   bool _331 = _330 == 2;
   bool _332 = _328 && _331;
   int _333 = _A_s0_ci_kx_ky_xx_yy_coo_xxx % 3072;
   int _334 = _333 >> 10;
   bool _335 = _334 == 2;
   bool _336 = _332 && _335;
   int _337 = _I_extent_0 >> 8;
   int _338 = _337 * 4;
   bool _339 = _A_s0_n_x_y < _338;
   bool _340 = _336 && _339;
   if (_340)
   {
    int _341 = _C_pipe_iter_temp;
    _C_pipe_base_temp = _341;
   } // if _340
   float16 _Out_channel_temp;
   #pragma unroll
   for (int _C_pipe_b__254 = 0; _C_pipe_b__254 < 0 + 16; _C_pipe_b__254++)
   {
    float _343 = _C_pipe_shreg[_C_pipe_b__254][0];
    _Out_channel_temp[_C_pipe_b__254] = _343;
    #pragma unroll
    for (int _C_pipe_b__254_dummy = 0; _C_pipe_b__254_dummy < 0 + 16; _C_pipe_b__254_dummy++)
    {
     float _344 = _Out_channel_temp[_C_pipe_b__254_dummy];
     float _345 = __fpga_reg(__fpga_reg(_344));
     _Out_channel_temp[_C_pipe_b__254_dummy] = _345;
    } // for _C_pipe_b__254_dummy
   } // for _C_pipe_b__254
   int _346 = _C_pipe_iter_temp;
   int _347 = _C_pipe_base_temp;
   int _348 = _347 + 14336;
   bool _349 = _346 < _348;
   if (_349)
   {
    float16 _350 = _Out_channel_temp;
    write_channel_intel(_Out_channel, _350);
    (void)_350;
   } // if _349
   #pragma unroll
   for (int _C_pipe_b__255 = 0; _C_pipe_b__255 < 0 + 16; _C_pipe_b__255++)
   {
    #pragma unroll
    for (int _C_pipe_p__127 = 0; _C_pipe_p__127 < 0 + 13; _C_pipe_p__127++)
    {
     #pragma unroll
     for (int _C_pipe_l__127 = 0; _C_pipe_l__127 < 0 + 1023; _C_pipe_l__127++)
     {
      int _351 = _C_pipe_p__127 * 1024;
      int _352 = _351 + _C_pipe_l__127;
      int _353 = _352 + 1;
      float _355 = _C_pipe_shreg[_C_pipe_b__255][_353];
      _C_pipe_shreg[_C_pipe_b__255][_352] = _355;
      (void)_355;
     } // for _C_pipe_l__127
     int _356 = _C_pipe_p__127 * 1024;
     int _357 = _356 + 1023;
     int _358 = _356 + 1024;
     float _360 = _C_pipe_shreg[_C_pipe_b__255][_358];
     float _361 = __fpga_reg(__fpga_reg(_360));
     _C_pipe_shreg[_C_pipe_b__255][_357] = _361;
     (void)_361;
    } // for _C_pipe_p__127
   } // for _C_pipe_b__255
   int _362 = _C_pipe_iter_temp;
   int _363 = _362 + 1;
   _C_pipe_iter_temp = _363;
  } // for _A_s0_ci_kx_ky_xx_yy_coo_xxx
 } // for _A_s0_n_x_y
} // kernel kernel_Out
// Address spaces for kernel_unloader
#define __address_space__unloader_mem_channel __global
__kernel void kernel_unloader(
 const int _I_extent_0,
 __address_space__unloader_mem_channel float *restrict _unloader_mem_channel)
{
 int _addr_temp;
 _addr_temp = 0;
 int _364 = _I_extent_0 >> 8;
 for (int _unloader_s0_n = 0; _unloader_s0_n < 0 + _364; _unloader_s0_n++)
 {
  for (int _unloader_s0_x_y_yyy_xx_yy_coo_xxx = 0; _unloader_s0_x_y_yyy_xx_yy_coo_xxx < 0 + 57344; _unloader_s0_x_y_yyy_xx_yy_coo_xxx++)
  {
   float16 __365 = read_channel_intel(_Out_channel);
   int _366 = _addr_temp;
   int _367 = _366 * 16;
   vstore16(__365, 0, (__address_space__unloader_mem_channel float*)_unloader_mem_channel + _367);
   int _368 = _addr_temp;
   int _369 = _368 + 1;
   _addr_temp = _369;
  } // for _unloader_s0_x_y_yyy_xx_yy_coo_xxx
 } // for _unloader_s0_n
} // kernel kernel_unloader
#undef __address_space__unloader_mem_channel

