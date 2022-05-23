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
typedef struct {
	uchar f0;
	float f1;
	float f2;
	float f3;
	float f4;
	float f5;
	float f6;
	float f7;
} cgs;
channel cgs _rLoader_channel __attribute__((depth(16))) ;
typedef struct { cgs s[8]; } _rFeeder_channel_array_t;
channel _rFeeder_channel_array_t _rFeeder_channel __attribute__((depth(16))) ;
channel uchar _hLoader_channel __attribute__((depth(128))) ;
typedef struct { uchar s[24]; } _hFeeder_channel_array_t;
channel _hFeeder_channel_array_t _hFeeder_channel __attribute__((depth(128))) ;
typedef struct { float s; } _Out_channel_array_t;
channel _Out_channel_array_t _Out_channel __attribute__((depth(128))) ;
// Address spaces for kernel_rLoader_beta_gap_im
#define __address_space__R_serializer_mem_channel __global
__kernel void kernel_rLoader_beta_gap_im(
 const int _H_extent_0,
 const int _R_extent_1,
 __address_space__R_serializer_mem_channel const cgs *restrict _R_serializer_mem_channel)
{
 // produce rLoader_beta_match_im
 // produce rLoader_alpha_gap_im
 // produce rLoader_alpha_match_im
 // produce rLoader_eta_im
 // produce rLoader_zeta_im
 // produce rLoader_delta_im
 // produce rLoader
 float _t__17_temp;
 float _t__16_temp;
 float _t__15_temp;
 float _t__14_temp;
 float _t__13_temp;
 float _t__12_temp;
 float _t__11_temp;
 uchar _t__10_temp;
 cgs _t_temp;
 int _addr_temp;
 _addr_temp = 0;
 int _0 = _R_extent_1 / 14;
 for (int _rLoader_s0_r = 0; _rLoader_s0_r < 0 + _0; _rLoader_s0_r++)
 {
  int _1 = _H_extent_0 / 14;
  for (int _rLoader_s0_h = 0; _rLoader_s0_h < 0 + _1; _rLoader_s0_h++)
  {
   for (int _rLoader_s0_i_rr_ii = 0; _rLoader_s0_i_rr_ii < 0 + 1792; _rLoader_s0_i_rr_ii++)
   {
    int _2 = _addr_temp;
    int _3 = _H_extent_0 / 14;
    int _4 = _3 * 1792;
    int _5 = _2 / _4;
    int _6 = _5 * 1792;
    int _7 = _2 % 1792;
    int _8 = _6 + _7;
    cgs _9 = _R_serializer_mem_channel[_8];
    _t_temp = _9;
    cgs _10 = _t_temp;
    uchar _11 = _10.f0;
    _t__10_temp = _11;
    cgs _12 = _t_temp;
    float _13 = _12.f1;
    _t__11_temp = _13;
    cgs _14 = _t_temp;
    float _15 = _14.f2;
    _t__12_temp = _15;
    cgs _16 = _t_temp;
    float _17 = _16.f3;
    _t__13_temp = _17;
    cgs _18 = _t_temp;
    float _19 = _18.f4;
    _t__14_temp = _19;
    cgs _20 = _t_temp;
    float _21 = _20.f5;
    _t__15_temp = _21;
    cgs _22 = _t_temp;
    float _23 = _22.f6;
    _t__16_temp = _23;
    cgs _24 = _t_temp;
    float _25 = _24.f7;
    _t__17_temp = _25;
    uchar _26 = _t__10_temp;
    float _27 = _t__11_temp;
    float _28 = _t__12_temp;
    float _29 = _t__13_temp;
    float _30 = _t__14_temp;
    float _31 = _t__15_temp;
    float _32 = _t__16_temp;
    float _33 = _t__17_temp;
    cgs _34 = {    _26,     _27,     _28,     _29,     _30,     _31,     _32,     _33};
    write_channel_intel(_rLoader_channel, _34);
    (void)_34;
    int _35 = _addr_temp;
    int _36 = _35 + 1;
    _addr_temp = _36;
   } // for _rLoader_s0_i_rr_ii
  } // for _rLoader_s0_h
 } // for _rLoader_s0_r
} // kernel kernel_rLoader_beta_gap_im
#undef __address_space__R_serializer_mem_channel
// Address spaces for kernel_rFeeder_beta_gap_im
__kernel void kernel_rFeeder_beta_gap_im(
 const int _H_extent_0,
 const int _R_extent_1)
{
 _rFeeder_channel_array_t _rFeeder_channel_array;
 // produce rFeeder_beta_match_im
 // produce rFeeder_alpha_gap_im
 // produce rFeeder_alpha_match_im
 // produce rFeeder_eta_im
 // produce rFeeder_zeta_im
 // produce rFeeder_delta_im
 // produce rFeeder
 cgs _rFeeder_value_shreg;
 uint _rFeeder_time_stamp_shreg;
 cgs _rFeeder_in_v_temp;
 uint _rFeeder_cycle_temp;
 float __attribute__((memory, numbanks(8), singlepump, numwriteports(1), numreadports(1))) _rFeeder_DB_f7_0_ibuffer[2][16][14][8];
 float __attribute__((memory, numbanks(8), singlepump, numwriteports(1), numreadports(1))) _rFeeder_DB_f6_0_ibuffer[2][16][14][8];
 float __attribute__((memory, numbanks(8), singlepump, numwriteports(1), numreadports(1))) _rFeeder_DB_f5_0_ibuffer[2][16][14][8];
 float __attribute__((memory, numbanks(8), singlepump, numwriteports(1), numreadports(1))) _rFeeder_DB_f4_0_ibuffer[2][16][14][8];
 float __attribute__((memory, numbanks(8), singlepump, numwriteports(1), numreadports(1))) _rFeeder_DB_f3_0_ibuffer[2][16][14][8];
 float __attribute__((memory, numbanks(8), singlepump, numwriteports(1), numreadports(1))) _rFeeder_DB_f2_0_ibuffer[2][16][14][8];
 float __attribute__((memory, numbanks(8), singlepump, numwriteports(1), numreadports(1))) _rFeeder_DB_f1_0_ibuffer[2][16][14][8];
 uchar __attribute__((memory, numbanks(8), singlepump, numwriteports(1), numreadports(1))) _rFeeder_DB_f0_0_ibuffer[2][16][14][8];
 cgs _t__9_temp;
 float _t__26_temp;
 float _t__25_temp;
 float _t__24_temp;
 float _t__23_temp;
 float _t__22_temp;
 float _t__21_temp;
 float _t__20_temp;
 uchar _t__19_temp;
 #pragma unroll
 for (int _rFeeder_s0_jj_init = 0; _rFeeder_s0_jj_init < 0 + 24; _rFeeder_s0_jj_init++)
 {
  bool _37 = _rFeeder_s0_jj_init == 0;
  if (_37)
  {
   uint _38 = (uint)(ADD_UINT64_T_SUFFIX(48384));
   _rFeeder_cycle_temp = _38;
  } // if _37
 } // for _rFeeder_s0_jj_init
 int _39 = _R_extent_1 / 14;
 int _40 = _H_extent_0 / 14;
 int _41 = _39 * _40;
 int _42 = _41 * 50176;
 int _43 = _42 + 50176;
 for (int _rFeeder_s0_outermost_loop = 0; _rFeeder_s0_outermost_loop < 0 + _43; _rFeeder_s0_outermost_loop++)
 {
  uint _44 = (uint)(ADD_UINT64_T_SUFFIX(48384));
  uint _45 = _rFeeder_cycle_temp;
  uint _46 = (uint)(ADD_UINT64_T_SUFFIX(50176));
  uint _47 = _45 % _46;
  bool _48 = _44 <= _47;
  uint _49 = _45 / _46;
  int _50 = (int)(_49);
  int _51 = _R_extent_1 / 14;
  int _52 = _H_extent_0 / 14;
  int _53 = _51 * _52;
  bool _54 = _50 < _53;
  bool _55 = _48 && _54;
  if (_55)
  {
   cgs __56 = read_channel_intel(_rLoader_channel);
   _rFeeder_in_v_temp = __56;
  } // if _55
  #pragma unroll
  for (int _rFeeder_s0_buf = 0; _rFeeder_s0_buf < 0 + 8; _rFeeder_s0_buf++)
  {
   bool _57 = _rFeeder_s0_buf == 0;
   if (_57)
   {
    cgs _58 = _rFeeder_in_v_temp;
    _rFeeder_value_shreg = _58;
    (void)_58;
    uint _59 = _rFeeder_cycle_temp;
    _rFeeder_time_stamp_shreg = _59;
    (void)_59;
   } // if _57
   else
   {
    cgs _61 = _rFeeder_value_shreg;
    _rFeeder_value_shreg = _61;
    (void)_61;
    uint _63 = _rFeeder_time_stamp_shreg;
    _rFeeder_time_stamp_shreg = _63;
    (void)_63;
   } // if _57 else
   cgs _65 = _rFeeder_value_shreg;
   cgs _66 = __fpga_reg(__fpga_reg(_65));
   _rFeeder_value_shreg = _66;
   (void)_66;
   uint _68 = _rFeeder_time_stamp_shreg;
   uint _69 = __fpga_reg(__fpga_reg(_68));
   _rFeeder_time_stamp_shreg = _69;
   (void)_69;
   uint _70 = (uint)(ADD_UINT64_T_SUFFIX(48384));
   uint _72 = _rFeeder_time_stamp_shreg;
   uint _73 = (uint)(ADD_UINT64_T_SUFFIX(50176));
   uint _74 = _72 % _73;
   bool _75 = _70 <= _74;
   if (_75)
   {
    uint _77 = _rFeeder_time_stamp_shreg;
    uint _78 = (uint)(ADD_UINT64_T_SUFFIX(50176));
    uint _79 = _77 % _78;
    uint _80 = (uint)(ADD_UINT64_T_SUFFIX(48384));
    uint _81 = _79 - _80;
    uint _82 = (uint)(ADD_UINT64_T_SUFFIX(7));
    uint _83 = _81 & _82;
    int _84 = (int)(_83);
    bool _85 = _rFeeder_s0_buf == _84;
    if (_85)
    {
     cgs _87 = _rFeeder_value_shreg;
     uchar _88 = _87.f0;
     uint _90 = _rFeeder_time_stamp_shreg;
     uint _91 = (uint)(ADD_UINT64_T_SUFFIX(50176));
     uint _92 = _90 / _91;
     uint _93 = (uint)(ADD_UINT64_T_SUFFIX(1));
     uint _94 = _92 & _93;
     bool _95 = (bool)(_94);
     uint _97 = _90 % _91;
     uint _98 = (uint)(ADD_UINT64_T_SUFFIX(48384));
     uint _99 = _97 - _98;
     int _100 = (int)(_99);
     int _101 = _100 / 112;
     int _103 = _100 >> 3;
     int _104 = _103 % 14;
     _rFeeder_DB_f0_0_ibuffer[_95][_101][_104][_rFeeder_s0_buf] = _88;
     cgs _106 = _rFeeder_value_shreg;
     float _107 = _106.f1;
     uint _109 = _rFeeder_time_stamp_shreg;
     uint _110 = (uint)(ADD_UINT64_T_SUFFIX(50176));
     uint _111 = _109 / _110;
     uint _112 = (uint)(ADD_UINT64_T_SUFFIX(1));
     uint _113 = _111 & _112;
     bool _114 = (bool)(_113);
     uint _116 = _109 % _110;
     uint _117 = (uint)(ADD_UINT64_T_SUFFIX(48384));
     uint _118 = _116 - _117;
     int _119 = (int)(_118);
     int _120 = _119 / 112;
     int _122 = _119 >> 3;
     int _123 = _122 % 14;
     _rFeeder_DB_f1_0_ibuffer[_114][_120][_123][_rFeeder_s0_buf] = _107;
     cgs _125 = _rFeeder_value_shreg;
     float _126 = _125.f2;
     uint _128 = _rFeeder_time_stamp_shreg;
     uint _129 = (uint)(ADD_UINT64_T_SUFFIX(50176));
     uint _130 = _128 / _129;
     uint _131 = (uint)(ADD_UINT64_T_SUFFIX(1));
     uint _132 = _130 & _131;
     bool _133 = (bool)(_132);
     uint _135 = _128 % _129;
     uint _136 = (uint)(ADD_UINT64_T_SUFFIX(48384));
     uint _137 = _135 - _136;
     int _138 = (int)(_137);
     int _139 = _138 / 112;
     int _141 = _138 >> 3;
     int _142 = _141 % 14;
     _rFeeder_DB_f2_0_ibuffer[_133][_139][_142][_rFeeder_s0_buf] = _126;
     cgs _144 = _rFeeder_value_shreg;
     float _145 = _144.f3;
     uint _147 = _rFeeder_time_stamp_shreg;
     uint _148 = (uint)(ADD_UINT64_T_SUFFIX(50176));
     uint _149 = _147 / _148;
     uint _150 = (uint)(ADD_UINT64_T_SUFFIX(1));
     uint _151 = _149 & _150;
     bool _152 = (bool)(_151);
     uint _154 = _147 % _148;
     uint _155 = (uint)(ADD_UINT64_T_SUFFIX(48384));
     uint _156 = _154 - _155;
     int _157 = (int)(_156);
     int _158 = _157 / 112;
     int _160 = _157 >> 3;
     int _161 = _160 % 14;
     _rFeeder_DB_f3_0_ibuffer[_152][_158][_161][_rFeeder_s0_buf] = _145;
     cgs _163 = _rFeeder_value_shreg;
     float _164 = _163.f4;
     uint _166 = _rFeeder_time_stamp_shreg;
     uint _167 = (uint)(ADD_UINT64_T_SUFFIX(50176));
     uint _168 = _166 / _167;
     uint _169 = (uint)(ADD_UINT64_T_SUFFIX(1));
     uint _170 = _168 & _169;
     bool _171 = (bool)(_170);
     uint _173 = _166 % _167;
     uint _174 = (uint)(ADD_UINT64_T_SUFFIX(48384));
     uint _175 = _173 - _174;
     int _176 = (int)(_175);
     int _177 = _176 / 112;
     int _179 = _176 >> 3;
     int _180 = _179 % 14;
     _rFeeder_DB_f4_0_ibuffer[_171][_177][_180][_rFeeder_s0_buf] = _164;
     cgs _182 = _rFeeder_value_shreg;
     float _183 = _182.f5;
     uint _185 = _rFeeder_time_stamp_shreg;
     uint _186 = (uint)(ADD_UINT64_T_SUFFIX(50176));
     uint _187 = _185 / _186;
     uint _188 = (uint)(ADD_UINT64_T_SUFFIX(1));
     uint _189 = _187 & _188;
     bool _190 = (bool)(_189);
     uint _192 = _185 % _186;
     uint _193 = (uint)(ADD_UINT64_T_SUFFIX(48384));
     uint _194 = _192 - _193;
     int _195 = (int)(_194);
     int _196 = _195 / 112;
     int _198 = _195 >> 3;
     int _199 = _198 % 14;
     _rFeeder_DB_f5_0_ibuffer[_190][_196][_199][_rFeeder_s0_buf] = _183;
     cgs _201 = _rFeeder_value_shreg;
     float _202 = _201.f6;
     uint _204 = _rFeeder_time_stamp_shreg;
     uint _205 = (uint)(ADD_UINT64_T_SUFFIX(50176));
     uint _206 = _204 / _205;
     uint _207 = (uint)(ADD_UINT64_T_SUFFIX(1));
     uint _208 = _206 & _207;
     bool _209 = (bool)(_208);
     uint _211 = _204 % _205;
     uint _212 = (uint)(ADD_UINT64_T_SUFFIX(48384));
     uint _213 = _211 - _212;
     int _214 = (int)(_213);
     int _215 = _214 / 112;
     int _217 = _214 >> 3;
     int _218 = _217 % 14;
     _rFeeder_DB_f6_0_ibuffer[_209][_215][_218][_rFeeder_s0_buf] = _202;
     cgs _220 = _rFeeder_value_shreg;
     float _221 = _220.f7;
     uint _223 = _rFeeder_time_stamp_shreg;
     uint _224 = (uint)(ADD_UINT64_T_SUFFIX(50176));
     uint _225 = _223 / _224;
     uint _226 = (uint)(ADD_UINT64_T_SUFFIX(1));
     uint _227 = _225 & _226;
     bool _228 = (bool)(_227);
     uint _230 = _223 % _224;
     uint _231 = (uint)(ADD_UINT64_T_SUFFIX(48384));
     uint _232 = _230 - _231;
     int _233 = (int)(_232);
     int _234 = _233 / 112;
     int _236 = _233 >> 3;
     int _237 = _236 % 14;
     _rFeeder_DB_f7_0_ibuffer[_228][_234][_237][_rFeeder_s0_buf] = _221;
    } // if _85
   } // if _75
   uint _239 = _rFeeder_time_stamp_shreg;
   uint _240 = (uint)(ADD_UINT64_T_SUFFIX(50176));
   uint _241 = _239 / _240;
   int _242 = (int)(_241);
   int _243 = _R_extent_1 / 14;
   int _244 = _H_extent_0 / 14;
   int _245 = _243 * _244;
   bool _246 = _242 <= _245;
   uint _247 = (uint)(ADD_UINT64_T_SUFFIX(0));
   bool _249 = _247 < _241;
   bool _250 = _246 && _249;
   if (_250)
   {
    uint _252 = _rFeeder_time_stamp_shreg;
    uint _253 = (uint)(ADD_UINT64_T_SUFFIX(50176));
    uint _254 = _252 / _253;
    uint _255 = (uint)(ADD_UINT64_T_SUFFIX(1));
    uint _256 = _254 & _255;
    bool _257 = (bool)(_256);
    bool _258 = !(_257);
    uint _260 = _252 % _253;
    int _261 = (int)(_260);
    int _262 = _261 / 3136;
    int _264 = _261 / 14;
    int _265 = _264 % 14;
    uchar _266 = _rFeeder_DB_f0_0_ibuffer[_258][_262][_265][_rFeeder_s0_buf];
    float _270 = _rFeeder_DB_f1_0_ibuffer[_258][_262][_265][_rFeeder_s0_buf];
    float _274 = _rFeeder_DB_f2_0_ibuffer[_258][_262][_265][_rFeeder_s0_buf];
    float _278 = _rFeeder_DB_f3_0_ibuffer[_258][_262][_265][_rFeeder_s0_buf];
    float _282 = _rFeeder_DB_f4_0_ibuffer[_258][_262][_265][_rFeeder_s0_buf];
    float _286 = _rFeeder_DB_f5_0_ibuffer[_258][_262][_265][_rFeeder_s0_buf];
    float _290 = _rFeeder_DB_f6_0_ibuffer[_258][_262][_265][_rFeeder_s0_buf];
    float _294 = _rFeeder_DB_f7_0_ibuffer[_258][_262][_265][_rFeeder_s0_buf];
    cgs _295 = {    _266,     _270,     _274,     _278,     _282,     _286,     _290,     _294};
    _t__9_temp = _295;
    cgs _296 = _t__9_temp;
    uchar _297 = _296.f0;
    _t__19_temp = _297;
    cgs _298 = _t__9_temp;
    float _299 = _298.f1;
    _t__20_temp = _299;
    cgs _300 = _t__9_temp;
    float _301 = _300.f2;
    _t__21_temp = _301;
    cgs _302 = _t__9_temp;
    float _303 = _302.f3;
    _t__22_temp = _303;
    cgs _304 = _t__9_temp;
    float _305 = _304.f4;
    _t__23_temp = _305;
    cgs _306 = _t__9_temp;
    float _307 = _306.f5;
    _t__24_temp = _307;
    cgs _308 = _t__9_temp;
    float _309 = _308.f6;
    _t__25_temp = _309;
    cgs _310 = _t__9_temp;
    float _311 = _310.f7;
    _t__26_temp = _311;
    uchar _312 = _t__19_temp;
    float _313 = _t__20_temp;
    float _314 = _t__21_temp;
    float _315 = _t__22_temp;
    float _316 = _t__23_temp;
    float _317 = _t__24_temp;
    float _318 = _t__25_temp;
    float _319 = _t__26_temp;
    cgs _320 = {    _312,     _313,     _314,     _315,     _316,     _317,     _318,     _319};
    _rFeeder_channel_array.s[_rFeeder_s0_buf] = _320;
    (void)_rFeeder_s0_buf;
   } // if _250
  } // for _rFeeder_s0_buf
  uint _322 = _rFeeder_time_stamp_shreg;
  uint _323 = (uint)(ADD_UINT64_T_SUFFIX(50176));
  uint _324 = _322 / _323;
  int _325 = (int)(_324);
  int _326 = _R_extent_1 / 14;
  int _327 = _H_extent_0 / 14;
  int _328 = _326 * _327;
  bool _329 = _325 <= _328;
  uint _330 = (uint)(ADD_UINT64_T_SUFFIX(0));
  bool _332 = _330 < _324;
  bool _333 = _329 && _332;
  if (_333)
  {
   write_channel_intel(_rFeeder_channel, _rFeeder_channel_array);
   (void)_rFeeder_channel_array;
  } // if _333
  uint _334 = _rFeeder_cycle_temp;
  uint _335 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _336 = _334 + _335;
  _rFeeder_cycle_temp = _336;
 } // for _rFeeder_s0_outermost_loop
} // kernel kernel_rFeeder_beta_gap_im
// Address spaces for kernel_hLoader
#define __address_space__H_serializer_mem_channel __global
__kernel void kernel_hLoader(
 const int _H_extent_0,
 const int _R_extent_1,
 __address_space__H_serializer_mem_channel const uchar *restrict _H_serializer_mem_channel)
{
 int _addr_temp;
 _addr_temp = 0;
 int _337 = _R_extent_1 / 14;
 for (int _hLoader_s0_r = 0; _hLoader_s0_r < 0 + _337; _hLoader_s0_r++)
 {
  int _338 = _H_extent_0 / 14;
  for (int _hLoader_s0_h = 0; _hLoader_s0_h < 0 + _338; _hLoader_s0_h++)
  {
   for (int _hLoader_s0_j_t_hh_jj = 0; _hLoader_s0_j_t_hh_jj < 0 + 5376; _hLoader_s0_j_t_hh_jj++)
   {
    int _339 = _addr_temp;
    int _340 = _H_extent_0 / 14;
    int _341 = _340 * 5376;
    int _342 = _339 % _341;
    uchar _343 = _H_serializer_mem_channel[_342];
    write_channel_intel(_hLoader_channel, _343);
    (void)_343;
    int _344 = _339 + 1;
    _addr_temp = _344;
   } // for _hLoader_s0_j_t_hh_jj
  } // for _hLoader_s0_h
 } // for _hLoader_s0_r
} // kernel kernel_hLoader
#undef __address_space__H_serializer_mem_channel
// Address spaces for kernel_hFeeder
__kernel void kernel_hFeeder(
 const int _H_extent_0,
 const int _R_extent_1)
{
 _hFeeder_channel_array_t _hFeeder_channel_array;
 uchar _hFeeder_value_shreg;
 uint _hFeeder_time_stamp_shreg;
 uchar _hFeeder_in_v_temp;
 uint _hFeeder_cycle_temp;
 uchar __attribute__((memory, numbanks(32), singlepump, numwriteports(1), numreadports(1))) _hFeeder_DB_0_ibuffer[2][16][14][32];
 #pragma unroll
 for (int _hFeeder_s0_ii_init = 0; _hFeeder_s0_ii_init < 0 + 8; _hFeeder_s0_ii_init++)
 {
  bool _345 = _hFeeder_s0_ii_init == 0;
  if (_345)
  {
   uint _346 = (uint)(ADD_UINT64_T_SUFFIX(44800));
   _hFeeder_cycle_temp = _346;
  } // if _345
 } // for _hFeeder_s0_ii_init
 int _347 = _R_extent_1 / 14;
 int _348 = _H_extent_0 / 14;
 int _349 = _347 * _348;
 int _350 = _349 * 50176;
 int _351 = _350 + 50176;
 for (int _hFeeder_s0_outermost_loop = 0; _hFeeder_s0_outermost_loop < 0 + _351; _hFeeder_s0_outermost_loop++)
 {
  uint _352 = (uint)(ADD_UINT64_T_SUFFIX(44800));
  uint _353 = _hFeeder_cycle_temp;
  uint _354 = (uint)(ADD_UINT64_T_SUFFIX(50176));
  uint _355 = _353 % _354;
  bool _356 = _352 <= _355;
  uint _357 = _353 / _354;
  int _358 = (int)(_357);
  int _359 = _R_extent_1 / 14;
  int _360 = _H_extent_0 / 14;
  int _361 = _359 * _360;
  bool _362 = _358 < _361;
  bool _363 = _356 && _362;
  if (_363)
  {
   uchar __364 = read_channel_intel(_hLoader_channel);
   _hFeeder_in_v_temp = __364;
  } // if _363
  #pragma unroll
  for (int _hFeeder_s0_buf = 0; _hFeeder_s0_buf < 0 + 24; _hFeeder_s0_buf++)
  {
   bool _365 = _hFeeder_s0_buf == 0;
   if (_365)
   {
    uchar _366 = _hFeeder_in_v_temp;
    _hFeeder_value_shreg = _366;
    (void)_366;
    uint _367 = _hFeeder_cycle_temp;
    _hFeeder_time_stamp_shreg = _367;
    (void)_367;
   } // if _365
   else
   {
    uchar _369 = _hFeeder_value_shreg;
    _hFeeder_value_shreg = _369;
    (void)_369;
    uint _371 = _hFeeder_time_stamp_shreg;
    _hFeeder_time_stamp_shreg = _371;
    (void)_371;
   } // if _365 else
   uchar _373 = _hFeeder_value_shreg;
   uchar _374 = __fpga_reg(__fpga_reg(_373));
   _hFeeder_value_shreg = _374;
   (void)_374;
   uint _376 = _hFeeder_time_stamp_shreg;
   uint _377 = __fpga_reg(__fpga_reg(_376));
   _hFeeder_time_stamp_shreg = _377;
   (void)_377;
   uint _378 = (uint)(ADD_UINT64_T_SUFFIX(44800));
   uint _380 = _hFeeder_time_stamp_shreg;
   uint _381 = (uint)(ADD_UINT64_T_SUFFIX(50176));
   uint _382 = _380 % _381;
   bool _383 = _378 <= _382;
   if (_383)
   {
    uint _385 = _hFeeder_time_stamp_shreg;
    uint _386 = (uint)(ADD_UINT64_T_SUFFIX(50176));
    uint _387 = _385 % _386;
    uint _388 = (uint)(ADD_UINT64_T_SUFFIX(44800));
    uint _389 = _387 - _388;
    uint _390 = (uint)(ADD_UINT64_T_SUFFIX(24));
    uint _391 = _389 % _390;
    int _392 = (int)(_391);
    bool _393 = _hFeeder_s0_buf == _392;
    if (_393)
    {
     uchar _395 = _hFeeder_value_shreg;
     uint _397 = _hFeeder_time_stamp_shreg;
     uint _398 = (uint)(ADD_UINT64_T_SUFFIX(50176));
     uint _399 = _397 / _398;
     uint _400 = (uint)(ADD_UINT64_T_SUFFIX(1));
     uint _401 = _399 & _400;
     bool _402 = (bool)(_401);
     uint _404 = _397 % _398;
     uint _405 = (uint)(ADD_UINT64_T_SUFFIX(44800));
     uint _406 = _404 - _405;
     int _407 = (int)(_406);
     int _408 = _407 / 336;
     int _410 = _407 / 24;
     int _411 = _410 % 14;
     _hFeeder_DB_0_ibuffer[_402][_408][_411][_hFeeder_s0_buf] = _395;
    } // if _393
   } // if _383
   uint _413 = _hFeeder_time_stamp_shreg;
   uint _414 = (uint)(ADD_UINT64_T_SUFFIX(50176));
   uint _415 = _413 / _414;
   int _416 = (int)(_415);
   int _417 = _R_extent_1 / 14;
   int _418 = _H_extent_0 / 14;
   int _419 = _417 * _418;
   bool _420 = _416 <= _419;
   uint _421 = (uint)(ADD_UINT64_T_SUFFIX(0));
   bool _423 = _421 < _415;
   bool _424 = _420 && _423;
   if (_424)
   {
    uint _426 = _hFeeder_time_stamp_shreg;
    uint _427 = (uint)(ADD_UINT64_T_SUFFIX(50176));
    uint _428 = _426 / _427;
    uint _429 = (uint)(ADD_UINT64_T_SUFFIX(1));
    uint _430 = _428 & _429;
    bool _431 = (bool)(_430);
    bool _432 = !(_431);
    uint _434 = _426 % _427;
    int _435 = (int)(_434);
    int _436 = _435 / 196;
    int _437 = _436 & 15;
    int _439 = _435 % 14;
    uchar _440 = _hFeeder_DB_0_ibuffer[_432][_437][_439][_hFeeder_s0_buf];
    _hFeeder_channel_array.s[_hFeeder_s0_buf] = _440;
    (void)_hFeeder_s0_buf;
   } // if _424
  } // for _hFeeder_s0_buf
  uint _442 = _hFeeder_time_stamp_shreg;
  uint _443 = (uint)(ADD_UINT64_T_SUFFIX(50176));
  uint _444 = _442 / _443;
  int _445 = (int)(_444);
  int _446 = _R_extent_1 / 14;
  int _447 = _H_extent_0 / 14;
  int _448 = _446 * _447;
  bool _449 = _445 <= _448;
  uint _450 = (uint)(ADD_UINT64_T_SUFFIX(0));
  bool _452 = _450 < _444;
  bool _453 = _449 && _452;
  if (_453)
  {
   write_channel_intel(_hFeeder_channel, _hFeeder_channel_array);
   (void)_hFeeder_channel_array;
  } // if _453
  uint _454 = _hFeeder_cycle_temp;
  uint _455 = (uint)(ADD_UINT64_T_SUFFIX(1));
  uint _456 = _454 + _455;
  _hFeeder_cycle_temp = _456;
 } // for _hFeeder_s0_outermost_loop
} // kernel kernel_hFeeder
// Address spaces for kernel_Out
__kernel void kernel_Out(
 const int _H_extent_0,
 const int _R_extent_1)
{
 _hFeeder_channel_array_t _hFeeder_channel_array;
 _rFeeder_channel_array_t _rFeeder_channel_array;
 _Out_channel_array_t _Out_channel_array;
 // produce Sum
 float _Sum_shreg[8][24][196];
 float _Sum_temp_shreg[8][24];
 // produce D
 float _D_shreg[8][24][3332];
 float _D_temp_shreg[8][24];
 // produce I
 float _I_shreg[8][24][3332];
 float _I_temp_shreg[8][24];
 // produce M
 float _M_shreg[8][24][3332];
 float _M_temp_shreg[8][24];
 // produce Beta
 float _Beta_shreg[8][24];
 // produce Alpha
 float _Alpha_shreg[8][24];
 // produce BetaGap
 float _BetaGap_shreg[8];
 // produce BetaMatch
 float _BetaMatch_shreg[8];
 // produce AlphaGap
 float _AlphaGap_shreg[8];
 // produce AlphaMatch
 float _AlphaMatch_shreg[8];
 // produce Eta
 float _Eta_shreg[8];
 // produce Zeta
 float _Zeta_shreg[8];
 // produce Delta
 float _Delta_shreg[8];
 // produce Read
 uchar _Read_shreg[8];
 // produce Hap
 uchar _Hap_shreg[24];
 cgs _t__18_temp;
 int _457 = _R_extent_1 / 14;
 for (int _Hap_s0_r = 0; _Hap_s0_r < 0 + _457; _Hap_s0_r++)
 {
  int _458 = _H_extent_0 / 14;
  for (int _Hap_s0_h = 0; _Hap_s0_h < 0 + _458; _Hap_s0_h++)
  {
   for (int _Hap_s0_i_j_rr_t_hh = 0; _Hap_s0_i_j_rr_t_hh < 0 + 50176; _Hap_s0_i_j_rr_t_hh++)
   {
    _hFeeder_channel_array_t __459 = read_channel_intel(_hFeeder_channel);
    _hFeeder_channel_array = __459;
    (void)__459;
    _rFeeder_channel_array_t __460 = read_channel_intel(_rFeeder_channel);
    _rFeeder_channel_array = __460;
    (void)__460;
    #pragma unroll
    for (int _Hap_s0_jj = 0; _Hap_s0_jj < 0 + 24; _Hap_s0_jj++)
    {
     #pragma unroll
     for (int _Hap_s0_ii = 0; _Hap_s0_ii < 0 + 8; _Hap_s0_ii++)
     {
      bool _461 = _Hap_s0_jj == 0;
      if (_461)
      {
       cgs __462 = _rFeeder_channel_array.s[_Hap_s0_ii];
       _t__18_temp = __462;
      } // if _461
      uchar _463;
      bool _464 = _Hap_s0_ii == 0;
      if (_464)
      {
       uchar __465 = _hFeeder_channel_array.s[_Hap_s0_jj];
       _463 = __465;
      } // if _464
      else
      {
       uchar _467 = _Hap_shreg[_Hap_s0_jj];
       _463 = _467;
      } // if _464 else
      uchar _468 = _463;
      _Hap_shreg[_Hap_s0_jj] = _468;
      (void)_468;
      uchar _470 = _Hap_shreg[_Hap_s0_jj];
      uchar _471 = __fpga_reg(__fpga_reg(_470));
      _Hap_shreg[_Hap_s0_jj] = _471;
      (void)_471;
      uchar _472;
      bool _473 = _Hap_s0_jj == 0;
      if (_473)
      {
       cgs _474 = _t__18_temp;
       uchar _475 = _474.f0;
       _472 = _475;
      } // if _473
      else
      {
       uchar _477 = _Read_shreg[_Hap_s0_ii];
       _472 = _477;
      } // if _473 else
      uchar _478 = _472;
      _Read_shreg[_Hap_s0_ii] = _478;
      (void)_478;
      uchar _480 = _Read_shreg[_Hap_s0_ii];
      uchar _481 = __fpga_reg(__fpga_reg(_480));
      _Read_shreg[_Hap_s0_ii] = _481;
      (void)_481;
      float _482;
      bool _483 = _Hap_s0_jj == 0;
      if (_483)
      {
       cgs _484 = _t__18_temp;
       float _485 = _484.f1;
       _482 = _485;
      } // if _483
      else
      {
       float _487 = _Delta_shreg[_Hap_s0_ii];
       _482 = _487;
      } // if _483 else
      float _488 = _482;
      _Delta_shreg[_Hap_s0_ii] = _488;
      (void)_488;
      float _490 = _Delta_shreg[_Hap_s0_ii];
      float _491 = __fpga_reg(__fpga_reg(_490));
      _Delta_shreg[_Hap_s0_ii] = _491;
      (void)_491;
      float _492;
      bool _493 = _Hap_s0_jj == 0;
      if (_493)
      {
       cgs _494 = _t__18_temp;
       float _495 = _494.f2;
       _492 = _495;
      } // if _493
      else
      {
       float _497 = _Zeta_shreg[_Hap_s0_ii];
       _492 = _497;
      } // if _493 else
      float _498 = _492;
      _Zeta_shreg[_Hap_s0_ii] = _498;
      (void)_498;
      float _500 = _Zeta_shreg[_Hap_s0_ii];
      float _501 = __fpga_reg(__fpga_reg(_500));
      _Zeta_shreg[_Hap_s0_ii] = _501;
      (void)_501;
      float _502;
      bool _503 = _Hap_s0_jj == 0;
      if (_503)
      {
       cgs _504 = _t__18_temp;
       float _505 = _504.f3;
       _502 = _505;
      } // if _503
      else
      {
       float _507 = _Eta_shreg[_Hap_s0_ii];
       _502 = _507;
      } // if _503 else
      float _508 = _502;
      _Eta_shreg[_Hap_s0_ii] = _508;
      (void)_508;
      float _510 = _Eta_shreg[_Hap_s0_ii];
      float _511 = __fpga_reg(__fpga_reg(_510));
      _Eta_shreg[_Hap_s0_ii] = _511;
      (void)_511;
      float _512;
      bool _513 = _Hap_s0_jj == 0;
      if (_513)
      {
       cgs _514 = _t__18_temp;
       float _515 = _514.f4;
       _512 = _515;
      } // if _513
      else
      {
       float _517 = _AlphaMatch_shreg[_Hap_s0_ii];
       _512 = _517;
      } // if _513 else
      float _518 = _512;
      _AlphaMatch_shreg[_Hap_s0_ii] = _518;
      (void)_518;
      float _520 = _AlphaMatch_shreg[_Hap_s0_ii];
      float _521 = __fpga_reg(__fpga_reg(_520));
      _AlphaMatch_shreg[_Hap_s0_ii] = _521;
      (void)_521;
      float _522;
      bool _523 = _Hap_s0_jj == 0;
      if (_523)
      {
       cgs _524 = _t__18_temp;
       float _525 = _524.f5;
       _522 = _525;
      } // if _523
      else
      {
       float _527 = _AlphaGap_shreg[_Hap_s0_ii];
       _522 = _527;
      } // if _523 else
      float _528 = _522;
      _AlphaGap_shreg[_Hap_s0_ii] = _528;
      (void)_528;
      float _530 = _AlphaGap_shreg[_Hap_s0_ii];
      float _531 = __fpga_reg(__fpga_reg(_530));
      _AlphaGap_shreg[_Hap_s0_ii] = _531;
      (void)_531;
      float _532;
      bool _533 = _Hap_s0_jj == 0;
      if (_533)
      {
       cgs _534 = _t__18_temp;
       float _535 = _534.f6;
       _532 = _535;
      } // if _533
      else
      {
       float _537 = _BetaMatch_shreg[_Hap_s0_ii];
       _532 = _537;
      } // if _533 else
      float _538 = _532;
      _BetaMatch_shreg[_Hap_s0_ii] = _538;
      (void)_538;
      float _540 = _BetaMatch_shreg[_Hap_s0_ii];
      float _541 = __fpga_reg(__fpga_reg(_540));
      _BetaMatch_shreg[_Hap_s0_ii] = _541;
      (void)_541;
      float _542;
      bool _543 = _Hap_s0_jj == 0;
      if (_543)
      {
       cgs _544 = _t__18_temp;
       float _545 = _544.f7;
       _542 = _545;
      } // if _543
      else
      {
       float _547 = _BetaGap_shreg[_Hap_s0_ii];
       _542 = _547;
      } // if _543 else
      float _548 = _542;
      _BetaGap_shreg[_Hap_s0_ii] = _548;
      (void)_548;
      float _550 = _BetaGap_shreg[_Hap_s0_ii];
      float _551 = __fpga_reg(__fpga_reg(_550));
      _BetaGap_shreg[_Hap_s0_ii] = _551;
      (void)_551;
      float _552;
      uchar _554 = _Read_shreg[_Hap_s0_ii];
      uchar _556 = _Hap_shreg[_Hap_s0_jj];
      bool _557 = _554 == _556;
      if (_557)
      {
       float _559 = _AlphaMatch_shreg[_Hap_s0_ii];
       _552 = _559;
      } // if _557
      else
      {
       float _561 = _AlphaGap_shreg[_Hap_s0_ii];
       _552 = _561;
      } // if _557 else
      float _562 = _552;
      _Alpha_shreg[_Hap_s0_ii][_Hap_s0_jj] = _562;
      (void)_562;
      float _563;
      uchar _565 = _Read_shreg[_Hap_s0_ii];
      uchar _567 = _Hap_shreg[_Hap_s0_jj];
      bool _568 = _565 == _567;
      if (_568)
      {
       float _570 = _BetaMatch_shreg[_Hap_s0_ii];
       _563 = _570;
      } // if _568
      else
      {
       float _572 = _BetaGap_shreg[_Hap_s0_ii];
       _563 = _572;
      } // if _568 else
      float _573 = _563;
      _Beta_shreg[_Hap_s0_ii][_Hap_s0_jj] = _573;
      (void)_573;
      float _574;
      int _575 = _Hap_s0_i_j_rr_t_hh / 3136;
      bool _576 = _575 == 0;
      bool _577 = _Hap_s0_ii == 0;
      bool _578 = _576 && _577;
      int _579 = _Hap_s0_i_j_rr_t_hh % 3136;
      int _580 = _579 / 196;
      bool _581 = _580 == 0;
      bool _582 = _Hap_s0_jj == 0;
      bool _583 = _581 && _582;
      bool _584 = _578 || _583;
      if (_584)
      {
       float _585 = float_from_bits(0 /* 0 */);
       _574 = _585;
      } // if _584
      else
      {
       float _586;
       bool _587 = _Hap_s0_ii == 0;
       if (_587)
       {
        float _588;
        bool _589 = _Hap_s0_jj == 0;
        if (_589)
        {
         int _590 = _Hap_s0_ii + 7;
         int _591 = _Hap_s0_jj + 23;
         float _593 = _M_shreg[_590][_591][3331];
         float _595 = _Alpha_shreg[_Hap_s0_ii][_Hap_s0_jj];
         float _596 = _593 * _595;
         float _598 = _Beta_shreg[_Hap_s0_ii][_Hap_s0_jj];
         float _600 = _I_shreg[_590][_591][3331];
         float _602 = _D_shreg[_590][_591][3331];
         float _603 = _600 + _602;
         float _604 = _598 * _603;
         float _605 = _596 + _604;
         _588 = _605;
        } // if _589
        else
        {
         int _606 = _Hap_s0_ii + 7;
         int _607 = _Hap_s0_jj + -1;
         float _609 = _M_shreg[_606][_607][3135];
         float _611 = _Alpha_shreg[_Hap_s0_ii][_Hap_s0_jj];
         float _612 = _609 * _611;
         float _614 = _Beta_shreg[_Hap_s0_ii][_Hap_s0_jj];
         float _616 = _I_shreg[_606][_607][3135];
         float _618 = _D_shreg[_606][_607][3135];
         float _619 = _616 + _618;
         float _620 = _614 * _619;
         float _621 = _612 + _620;
         _588 = _621;
        } // if _589 else
        float _622 = _588;
        _586 = _622;
       } // if _587
       else
       {
        float _623;
        bool _624 = _Hap_s0_jj == 0;
        if (_624)
        {
         int _625 = _Hap_s0_ii + -1;
         int _626 = _Hap_s0_jj + 23;
         float _628 = _M_shreg[_625][_626][195];
         float _630 = _Alpha_shreg[_Hap_s0_ii][_Hap_s0_jj];
         float _631 = _628 * _630;
         float _633 = _Beta_shreg[_Hap_s0_ii][_Hap_s0_jj];
         float _635 = _I_shreg[_625][_626][195];
         float _637 = _D_shreg[_625][_626][195];
         float _638 = _635 + _637;
         float _639 = _633 * _638;
         float _640 = _631 + _639;
         _623 = _640;
        } // if _624
        else
        {
         int _641 = _Hap_s0_ii + -1;
         int _642 = _Hap_s0_jj + -1;
         float _644 = _M_temp_shreg[_641][_642];
         float _646 = _Alpha_shreg[_Hap_s0_ii][_Hap_s0_jj];
         float _647 = _644 * _646;
         float _649 = _Beta_shreg[_Hap_s0_ii][_Hap_s0_jj];
         float _651 = _I_temp_shreg[_641][_642];
         float _653 = _D_temp_shreg[_641][_642];
         float _654 = _651 + _653;
         float _655 = _649 * _654;
         float _656 = _647 + _655;
         _623 = _656;
        } // if _624 else
        float _657 = _623;
        _586 = _657;
       } // if _587 else
       float _658 = _586;
       _574 = _658;
      } // if _584 else
      float _659 = _574;
      _M_temp_shreg[_Hap_s0_ii][_Hap_s0_jj] = _659;
      (void)_659;
      float _660;
      int _661 = _Hap_s0_i_j_rr_t_hh / 3136;
      bool _662 = _661 == 0;
      bool _663 = _Hap_s0_ii == 0;
      bool _664 = _662 && _663;
      bool _665 = _Hap_s0_jj == 0;
      int _666 = _Hap_s0_i_j_rr_t_hh % 3136;
      int _667 = _666 / 196;
      bool _668 = _667 == 0;
      bool _669 = _665 && _668;
      bool _670 = _664 || _669;
      if (_670)
      {
       float _671 = float_from_bits(0 /* 0 */);
       _660 = _671;
      } // if _670
      else
      {
       float _672;
       bool _673 = _Hap_s0_ii == 0;
       if (_673)
       {
        int _674 = _Hap_s0_ii + 7;
        float _676 = _M_shreg[_674][_Hap_s0_jj][3135];
        float _678 = _Delta_shreg[_Hap_s0_ii];
        float _679 = _676 * _678;
        float _681 = _I_shreg[_674][_Hap_s0_jj][3135];
        float _683 = _Eta_shreg[_Hap_s0_ii];
        float _684 = _681 * _683;
        float _685 = _679 + _684;
        _672 = _685;
       } // if _673
       else
       {
        int _686 = _Hap_s0_ii + -1;
        float _688 = _M_temp_shreg[_686][_Hap_s0_jj];
        float _690 = _Delta_shreg[_Hap_s0_ii];
        float _691 = _688 * _690;
        float _693 = _I_temp_shreg[_686][_Hap_s0_jj];
        float _695 = _Eta_shreg[_Hap_s0_ii];
        float _696 = _693 * _695;
        float _697 = _691 + _696;
        _672 = _697;
       } // if _673 else
       float _698 = _672;
       _660 = _698;
      } // if _670 else
      float _699 = _660;
      _I_temp_shreg[_Hap_s0_ii][_Hap_s0_jj] = _699;
      (void)_699;
      float _700;
      bool _701 = _Hap_s0_ii == 0;
      int _702 = _Hap_s0_i_j_rr_t_hh / 3136;
      bool _703 = _702 == 0;
      bool _704 = _701 && _703;
      if (_704)
      {
       float _705 = float_from_bits(992681150 /* 0.00261097 */);
       _700 = _705;
      } // if _704
      else
      {
       float _706;
       int _707 = _Hap_s0_i_j_rr_t_hh % 3136;
       int _708 = _707 / 196;
       bool _709 = _708 == 0;
       bool _710 = _Hap_s0_jj == 0;
       bool _711 = _709 && _710;
       if (_711)
       {
        float _712 = float_from_bits(0 /* 0 */);
        _706 = _712;
       } // if _711
       else
       {
        float _713;
        bool _714 = _Hap_s0_jj == 0;
        if (_714)
        {
         int _715 = _Hap_s0_jj + 23;
         float _717 = _M_shreg[_Hap_s0_ii][_715][195];
         float _719 = _Zeta_shreg[_Hap_s0_ii];
         float _720 = _717 * _719;
         float _722 = _D_shreg[_Hap_s0_ii][_715][195];
         float _724 = _Eta_shreg[_Hap_s0_ii];
         float _725 = _722 * _724;
         float _726 = _720 + _725;
         _713 = _726;
        } // if _714
        else
        {
         int _727 = _Hap_s0_jj + -1;
         float _729 = _M_temp_shreg[_Hap_s0_ii][_727];
         float _731 = _Zeta_shreg[_Hap_s0_ii];
         float _732 = _729 * _731;
         float _734 = _D_temp_shreg[_Hap_s0_ii][_727];
         float _736 = _Eta_shreg[_Hap_s0_ii];
         float _737 = _734 * _736;
         float _738 = _732 + _737;
         _713 = _738;
        } // if _714 else
        float _739 = _713;
        _706 = _739;
       } // if _711 else
       float _740 = _706;
       _700 = _740;
      } // if _704 else
      float _741 = _700;
      _D_temp_shreg[_Hap_s0_ii][_Hap_s0_jj] = _741;
      (void)_741;
      float _742;
      bool _743 = _Hap_s0_ii == 7;
      int _744 = _Hap_s0_i_j_rr_t_hh / 3136;
      bool _745 = _744 == 15;
      bool _746 = _743 && _745;
      if (_746)
      {
       float _748 = _I_temp_shreg[_Hap_s0_ii][_Hap_s0_jj];
       float _750 = _M_temp_shreg[_Hap_s0_ii][_Hap_s0_jj];
       float _751;
       int _752 = _Hap_s0_i_j_rr_t_hh % 3136;
       int _753 = _752 / 196;
       bool _754 = _753 == 0;
       bool _755 = _Hap_s0_jj == 0;
       bool _756 = _754 && _755;
       if (_756)
       {
        float _757 = float_from_bits(0 /* 0 */);
        _751 = _757;
       } // if _756
       else
       {
        float _758;
        bool _759 = _Hap_s0_jj == 0;
        if (_759)
        {
         int _760 = _Hap_s0_jj + 23;
         float _762 = _Sum_shreg[_Hap_s0_ii][_760][195];
         _758 = _762;
        } // if _759
        else
        {
         int _763 = _Hap_s0_jj + -1;
         float _765 = _Sum_temp_shreg[_Hap_s0_ii][_763];
         _758 = _765;
        } // if _759 else
        float _766 = _758;
        _751 = _766;
       } // if _756 else
       float _767 = _751;
       float _768 = _750 + _767;
       float _769 = _748 + _768;
       _742 = _769;
      } // if _746
      else
      {
       float _770 = float_from_bits(0 /* 0 */);
       _742 = _770;
      } // if _746 else
      float _771 = _742;
      _Sum_temp_shreg[_Hap_s0_ii][_Hap_s0_jj] = _771;
      (void)_771;
      bool _772 = _Hap_s0_ii == 7;
      int _773 = _Hap_s0_i_j_rr_t_hh / 3136;
      bool _774 = _773 == 15;
      bool _775 = _772 && _774;
      bool _776 = _Hap_s0_jj == 23;
      int _777 = _Hap_s0_i_j_rr_t_hh % 3136;
      int _778 = _777 / 196;
      bool _779 = _778 == 15;
      bool _780 = _776 && _779;
      bool _781 = _775 && _780;
      if (_781)
      {
       float _783 = _Sum_temp_shreg[7][23];
       _Out_channel_array.s = _783;
       (void)_783;
      } // if _781
     } // for _Hap_s0_ii
    } // for _Hap_s0_jj
    int _784 = _Hap_s0_i_j_rr_t_hh / 3136;
    bool _785 = _784 == 15;
    int _786 = _Hap_s0_i_j_rr_t_hh % 3136;
    int _787 = _786 / 196;
    bool _788 = _787 == 15;
    bool _789 = _785 && _788;
    if (_789)
    {
     write_channel_intel(_Out_channel, _Out_channel_array);
     (void)_Out_channel_array;
    } // if _789
    #pragma unroll
    for (int _dummy__5_s0_s_jj = 0; _dummy__5_s0_s_jj < 0 + 24; _dummy__5_s0_s_jj++)
    {
     #pragma unroll
     for (int _dummy__4_s0_s_ii = 0; _dummy__4_s0_s_ii < 0 + 8; _dummy__4_s0_s_ii++)
     {
      #pragma unroll
      for (int _dummy__3_s0_t = 0; _dummy__3_s0_t < 0 + 195; _dummy__3_s0_t++)
      {
       int _790 = 195 - _dummy__3_s0_t;
       int _791 = 194 - _dummy__3_s0_t;
       float _793 = _Sum_shreg[_dummy__4_s0_s_ii][_dummy__5_s0_s_jj][_791];
       _Sum_shreg[_dummy__4_s0_s_ii][_dummy__5_s0_s_jj][_790] = _793;
       (void)_793;
      } // for _dummy__3_s0_t
      #pragma unroll
      for (int _dummy__2_s0_t = 0; _dummy__2_s0_t < 0 + 3331; _dummy__2_s0_t++)
      {
       int _794 = 3331 - _dummy__2_s0_t;
       int _795 = 3330 - _dummy__2_s0_t;
       float _797 = _M_shreg[_dummy__4_s0_s_ii][_dummy__5_s0_s_jj][_795];
       _M_shreg[_dummy__4_s0_s_ii][_dummy__5_s0_s_jj][_794] = _797;
       (void)_797;
      } // for _dummy__2_s0_t
      #pragma unroll
      for (int _dummy__1_s0_t = 0; _dummy__1_s0_t < 0 + 3331; _dummy__1_s0_t++)
      {
       int _798 = 3331 - _dummy__1_s0_t;
       int _799 = 3330 - _dummy__1_s0_t;
       float _801 = _I_shreg[_dummy__4_s0_s_ii][_dummy__5_s0_s_jj][_799];
       _I_shreg[_dummy__4_s0_s_ii][_dummy__5_s0_s_jj][_798] = _801;
       (void)_801;
      } // for _dummy__1_s0_t
      #pragma unroll
      for (int _dummy_s0_t = 0; _dummy_s0_t < 0 + 3331; _dummy_s0_t++)
      {
       int _802 = 3331 - _dummy_s0_t;
       int _803 = 3330 - _dummy_s0_t;
       float _805 = _D_shreg[_dummy__4_s0_s_ii][_dummy__5_s0_s_jj][_803];
       _D_shreg[_dummy__4_s0_s_ii][_dummy__5_s0_s_jj][_802] = _805;
       (void)_805;
      } // for _dummy_s0_t
      float _807 = _D_temp_shreg[_dummy__4_s0_s_ii][_dummy__5_s0_s_jj];
      _D_shreg[_dummy__4_s0_s_ii][_dummy__5_s0_s_jj][0] = _807;
      (void)_807;
      float _809 = _I_temp_shreg[_dummy__4_s0_s_ii][_dummy__5_s0_s_jj];
      _I_shreg[_dummy__4_s0_s_ii][_dummy__5_s0_s_jj][0] = _809;
      (void)_809;
      float _811 = _M_temp_shreg[_dummy__4_s0_s_ii][_dummy__5_s0_s_jj];
      _M_shreg[_dummy__4_s0_s_ii][_dummy__5_s0_s_jj][0] = _811;
      (void)_811;
      float _813 = _Sum_temp_shreg[_dummy__4_s0_s_ii][_dummy__5_s0_s_jj];
      _Sum_shreg[_dummy__4_s0_s_ii][_dummy__5_s0_s_jj][0] = _813;
      (void)_813;
     } // for _dummy__4_s0_s_ii
    } // for _dummy__5_s0_s_jj
   } // for _Hap_s0_i_j_rr_t_hh
  } // for _Hap_s0_h
 } // for _Hap_s0_r
} // kernel kernel_Out
// Address spaces for kernel_unloader
#define __address_space__unloader_mem_channel __global
__kernel void kernel_unloader(
 const int _H_extent_0,
 const int _R_extent_1,
 __address_space__unloader_mem_channel float *restrict _unloader_mem_channel)
{
 _Out_channel_array_t _Out_channel_array;
 int _addr_temp;
 _addr_temp = 0;
 int _814 = _R_extent_1 / 14;
 for (int _unloader_s0_r = 0; _unloader_s0_r < 0 + _814; _unloader_s0_r++)
 {
  int _815 = _H_extent_0 / 14;
  for (int _unloader_s0_h = 0; _unloader_s0_h < 0 + _815; _unloader_s0_h++)
  {
   for (int _unloader_s0_rr_hh = 0; _unloader_s0_rr_hh < 0 + 196; _unloader_s0_rr_hh++)
   {
    _Out_channel_array_t __816 = read_channel_intel(_Out_channel);
    _Out_channel_array = __816;
    (void)__816;
    int _817 = _addr_temp;
    _unloader_mem_channel[_817] = _Out_channel_array.s;
    int _818 = _addr_temp;
    int _819 = _818 + 1;
    _addr_temp = _819;
   } // for _unloader_s0_rr_hh
  } // for _unloader_s0_h
 } // for _unloader_s0_r
} // kernel kernel_unloader
#undef __address_space__unloader_mem_channel

