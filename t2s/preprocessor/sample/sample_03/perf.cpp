/*******************************************************************************
 * Copyright 2021 Intel Corporation
 *
 * Licensed under the BSD-2-Clause Plus Patent License (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * https://opensource.org/licenses/BSDplusPatent
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 *******************************************************************************/
#define GPU 1
#ifndef GPU
#include "HalideBuffer.h"
#endif
#ifndef GPU
#include "util.h"
#endif
// Constant parameters (inner loop bounds) of the design
#include "capsule_const-parameter.h"

#ifdef FPGA
#ifndef GPU
#include "HalideBuffer.h"
#endif
#include "capsule.generated_oneapi_header.h"
#endif
#ifdef GPU
#include "esimd_test_utils.hpp"
#include <CL/sycl.hpp>
#include <iostream>
#include <sycl/ext/intel/esimd.hpp>
#endif

#define N 4
#define TOTAL_N N *NN
#define SIZE_P_0 TOTAL_CI *MK *MX *NN
/*
#define OX              7
#define OY              7
#define KY              3
#define KX              3
#define MY              4
#define MX              4
#define MK              4
#define TOTAL_IX        (OX * 2 + KX - 2)
#define TOTAL_IY        (OY * 2 + KY - 2)
#define TOTAL_CO        (COOO * COO * CO)
#define TOTAL_CI        (CII * CI)
*/
//SIZE_P_1=15 * 15 * 4 = 900
#define SIZE_P_1 TOTAL_IY *TOTAL_IX *N
#define SIZE_W_0 TOTAL_CO *MY
#define SIZE_W_1 TOTAL_CI *KY *KX *MK
#define SIZE_V_0 TOTAL_CO *MY *MX *NN
#define SIZE_V_1 OY *OX *N
#ifndef GPU
using namespace Halide;
#endif
double get_event_time(event e) {
  double start = e.get_profiling_info<info::event_profiling::command_start>();
  double end = e.get_profiling_info<info::event_profiling::command_end>();
  return end - start;
}
int main() {
  float *p = new float[SIZE_P_0 * SIZE_P_1];
  float *w = new float[SIZE_W_0 * SIZE_W_1];
  float *v = new float[SIZE_V_0 * SIZE_V_1];
  for (unsigned m = 0; m < SIZE_P_0 * SIZE_P_1; ++m)
    p[m] = random() % 5;
  for (unsigned m = 0; m < SIZE_W_0 * SIZE_W_1; ++m)
    w[m] = random() % 5;
  for (unsigned m = 0; m < SIZE_V_0 * SIZE_V_1; ++m)
    v[m] = random() % 5;

  size_t p_dim[2] = {SIZE_P_0, SIZE_P_1};
  size_t w_dim[2] = {SIZE_W_0, SIZE_W_1};
  size_t v_dim[2] = {SIZE_V_0, SIZE_V_1};

#ifdef FPGA
#ifndef GPU
  Halide::Runtime::Buffer<float, (sizeof(p_dim) / sizeof(size_t))> P_h(
      p, std::vector<size_t>(std::begin(p_dim), std::end(p_dim)));
#endif
#endif
#ifdef GPU
  int _P_extent_0 = p_dim[0];
  int _P_extent_1 = p_dim[1];
  sycl::image<2> img_P(p, image_channel_order::rgba, image_channel_type::fp32,
                       range<2>{p_dim[0] / 4, p_dim[1]});
#endif
#ifdef FPGA
#ifndef GPU
  Halide::Runtime::Buffer<float, (sizeof(w_dim) / sizeof(size_t))> W_h(
      w, std::vector<size_t>(std::begin(w_dim), std::end(w_dim)));
#endif
#endif
#ifdef GPU
  int _W_extent_0 = w_dim[0];
  int _W_extent_1 = w_dim[1];
  sycl::image<2> img_W(w, image_channel_order::rgba, image_channel_type::fp32,
                       range<2>{w_dim[0] / 4, w_dim[1]});
#endif
#ifdef FPGA
#ifndef GPU
  Halide::Runtime::Buffer<float, (sizeof(v_dim) / sizeof(size_t))> V_h(
      v, std::vector<size_t>(std::begin(v_dim), std::end(v_dim)));
#endif
#endif
#ifdef GPU
  int _V_extent_0 = v_dim[0];
  int _V_extent_1 = v_dim[1];
  sycl::image<2> img_V(v, image_channel_order::rgba, image_channel_type::fp32,
                       range<2>{v_dim[0] / 4, v_dim[1]});
#endif
#ifdef FPGA
#if defined(FPGA_EMULATOR)
  std::cout << "USING FPGA EMULATOR\n";
  sycl::ext::intel::fpga_emulator_selector
      device_selector; // (NOTE) for emulation
#else
  std::cout << "USING REAL FPGA\n";
  sycl::ext::intel::fpga_selector
      device_selector; // (NOTE) for full compile and hardware profiling
#endif
  std::cout << "Start Run\n";
  double exec_time = 0;
  exec_time = capsule(device_selector, P_h.raw_buffer(), W_h.raw_buffer(),
                      V_h.raw_buffer());
  std::cout << "Run completed!\n";
  std::cout << "kernel exec time: " << exec_time << "\n";
#endif

#ifdef GPU
  queue q(esimd_test::ESIMDSelector{}, esimd_test::createExceptionHandler(),
          property::queue::enable_profiling{});
  range<3> GlobalRange(4 * 4, 16 * 4, (_P_extent_1 / 225) * 1);
  range<3> LocalRange(4, 4, 1);
  const int nd_item_dimension = 3;
  double _time;
  double ops = 2 * (long)(TOTAL_N * TOTAL_CO) * (long)(MY * MX * OY * OX) * (long)(TOTAL_CI * MK * KY * KX);
  double min_time = RAND_MAX;
  double total_time = 0.0f;
  for (int i = 0; i < 100 ;i++){
  auto e = q.submit([&](handler &cgh) {
    auto _V = img_V.get_access<uint4, access::mode::write>(cgh);
    auto _P = img_P.get_access<uint4, access::mode::read>(cgh);
    auto _W = img_W.get_access<uint4, access::mode::read>(cgh);
    cgh.parallel_for<class Test>(
        nd_range{GlobalRange, LocalRange},
        [=](nd_item<nd_item_dimension> ndi) SYCL_ESIMD_KERNEL {
          using namespace sycl::ext::intel::esimd;
          int _A_s0_n___block_id_z = ndi.get_group(2);
          int _A_s0_nn___block_id_y = ndi.get_group(1);
          int _A_s0_co___block_id_x = ndi.get_group(0);
          int ___thread_id_y = ndi.get_local_id(1);
          int ___thread_id_x = ndi.get_local_id(0);
          simd<float, 392> _C;
          simd<float, 180> _P_im_buf;
          simd<float, 288> _W_im_buf;
          _C.select<392, 1>(0) = 0.000000;
          for (int _A_s0_mk = 0; _A_s0_mk < 0 + 4; _A_s0_mk++) {
            for (int _A_s0_ci = 0; _A_s0_ci < 0 + 8; _A_s0_ci++) {
              int _270 = (((_A_s0_mk * 8) + _A_s0_ci) * 36);
              int _271 = (((___thread_id_x * 4) + _A_s0_co___block_id_x) * 8);
              _W_im_buf.select<288, 1>(0)
                  .bit_cast_view<float, 36, 8>()
                  .select<8, 1, 8, 1>(0, 0) =
                  media_block_load<float, 8, 8>(_W, (_271 * 4), (_270 + 0));
              _W_im_buf.select<288, 1>(0)
                  .bit_cast_view<float, 36, 8>()
                  .select<8, 1, 8, 1>(8, 0) =
                  media_block_load<float, 8, 8>(_W, (_271 * 4), (_270 + 8));
              _W_im_buf.select<288, 1>(0)
                  .bit_cast_view<float, 36, 8>()
                  .select<8, 1, 8, 1>(16, 0) =
                  media_block_load<float, 8, 8>(_W, (_271 * 4), (_270 + 16));
              _W_im_buf.select<288, 1>(0)
                  .bit_cast_view<float, 36, 8>()
                  .select<8, 1, 8, 1>(24, 0) =
                  media_block_load<float, 8, 8>(_W, (_271 * 4), (_270 + 24));
              _W_im_buf.select<288, 1>(0)
                  .bit_cast_view<float, 36, 8>()
                  .select<4, 1, 8, 1>(32, 0) =
                  media_block_load<float, 4, 8>(_W, (_271 * 4), (_270 + 32));
#pragma unroll
              for (int _A_s0_yy_xx = 0; _A_s0_yy_xx < 0 + 7; _A_s0_yy_xx++) {
                int _272 = ((_A_s0_n___block_id_z * 225) + (_A_s0_yy_xx * 30));
                int _273 =
                    (((_A_s0_nn___block_id_y * 128) +
                      ((___thread_id_y * 32) + ((_A_s0_mk * 8) + _A_s0_ci))) *
                     4);
                _P_im_buf.select<180, 1>(0)
                    .bit_cast_view<float, 45, 4>()
                    .select<16, 1, 4, 1>(0, 0) =
                    media_block_load<float, 16, 4>(_P, (_273 * 4), (_272 + 0));
                _P_im_buf.select<180, 1>(0)
                    .bit_cast_view<float, 45, 4>()
                    .select<16, 1, 4, 1>(16, 0) =
                    media_block_load<float, 16, 4>(_P, (_273 * 4), (_272 + 16));
                _P_im_buf.select<180, 1>(0)
                    .bit_cast_view<float, 45, 4>()
                    .select<13, 1, 4, 1>(32, 0) =
                    media_block_load<float, 13, 4>(_P, (_273 * 4), (_272 + 32));
#pragma unroll
                for (int _A_s0_yyy_xxx = 0; _A_s0_yyy_xxx < 0 + 7;
                     _A_s0_yyy_xxx++) {
#pragma unroll
                  for (int _A_s0_kx = 0; _A_s0_kx < 0 + 3; _A_s0_kx++) {
#pragma unroll
                    for (int _A_s0_ky = 0; _A_s0_ky < 0 + 3; _A_s0_ky++) {
#pragma unroll
                      for (int _A_s0_cii = 0; _A_s0_cii < 0 + 4; _A_s0_cii++) {
                        _C.select<8, 1>(
                            (((_A_s0_yy_xx * 7) + _A_s0_yyy_xxx) * 8)) =
                            (_C.select<8, 1>(
                                 (((_A_s0_yy_xx * 7) + _A_s0_yyy_xxx) * 8)) +
                             (_W_im_buf.select<8, 1>(
                                  (((_A_s0_kx * 12) +
                                    ((_A_s0_ky * 4) + _A_s0_cii)) *
                                   8)) *
                              _P_im_buf[((((_A_s0_kx * 15) +
                                           ((_A_s0_yyy_xxx * 2) + _A_s0_ky)) *
                                          4) +
                                         _A_s0_cii)]));
                      } // for _A_s0_cii
                    }   // for _A_s0_ky
                  }     // for _A_s0_kx
                }       // for _A_s0_yyy_xxx
              }         // for _A_s0_yy_xx
            }           // for _A_s0_ci
          }             // for _A_s0_mk
          int _274 = (_A_s0_n___block_id_z * 49);
          int _275 = (((_A_s0_nn___block_id_y * 64) +
                       ((___thread_id_y * 16) +
                        ((___thread_id_x * 4) + _A_s0_co___block_id_x))) *
                      8);
          media_block_store<float, 8, 8>(_V, (_275 * 4), (_274 + 0),
                                         _C.select<392, 1>(0)
                                             .bit_cast_view<float, 49, 8>()
                                             .select<8, 1, 8, 1>(0, 0));
          media_block_store<float, 8, 8>(_V, (_275 * 4), (_274 + 8),
                                         _C.select<392, 1>(0)
                                             .bit_cast_view<float, 49, 8>()
                                             .select<8, 1, 8, 1>(8, 0));
          media_block_store<float, 8, 8>(_V, (_275 * 4), (_274 + 16),
                                         _C.select<392, 1>(0)
                                             .bit_cast_view<float, 49, 8>()
                                             .select<8, 1, 8, 1>(16, 0));
          media_block_store<float, 8, 8>(_V, (_275 * 4), (_274 + 24),
                                         _C.select<392, 1>(0)
                                             .bit_cast_view<float, 49, 8>()
                                             .select<8, 1, 8, 1>(24, 0));
          media_block_store<float, 8, 8>(_V, (_275 * 4), (_274 + 32),
                                         _C.select<392, 1>(0)
                                             .bit_cast_view<float, 49, 8>()
                                             .select<8, 1, 8, 1>(32, 0));
          media_block_store<float, 8, 8>(_V, (_275 * 4), (_274 + 40),
                                         _C.select<392, 1>(0)
                                             .bit_cast_view<float, 49, 8>()
                                             .select<8, 1, 8, 1>(40, 0));
          media_block_store<float, 1, 8>(_V, (_275 * 4), (_274 + 48),
                                         _C.select<392, 1>(0)
                                             .bit_cast_view<float, 49, 8>()
                                             .select<1, 1, 8, 1>(48, 0));
        } // kernel kernel_A
    );
  });
#endif
  e.wait();
  _time = get_event_time(e);
  if (_time < min_time) {
    min_time = _time;
  }
  total_time += _time;
}
std::cout << "Size of tensor P: " << TOTAL_N << ", " << TOTAL_CI << ", " << TOTAL_IX << ", " << TOTAL_IY
                                     << ", " << MX << ", " << MK << "\n";
std::cout << "Size of tensor W: " << TOTAL_CI << ", " << TOTAL_CO << ", " << KX << ", " << KY
                                     << ", " << MK << ", " << MY << "\n";
std::cout << "Average Throughput: " << ops / (total_time / 100) << "\n";
std::cout << "Max Throughput: " << ops / min_time << "\n";
}
