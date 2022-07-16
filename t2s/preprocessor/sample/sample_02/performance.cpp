inline int cm_corr_buf_idx_f20(int i) {
  if (i < 8)
    return (i - 0);
  if (i < 264)
    return (i - 248);
  if (i < 520)
    return (i - 496);
  return i;
}
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

#ifdef FPGA
#ifndef GPU
#include "HalideBuffer.h"
#endif
#include "conv.generated_oneapi_header.h"
#endif
#ifdef GPU
#include "esimd_test_utils.hpp"
#include <CL/sycl.hpp>
#include <iostream>
#include <sycl/ext/intel/esimd.hpp>
#endif

// Constant parameters (inner loop bounds) of the design
#define KY 3
#define KX 3
#define CII 8
#define CI 32
#define COOO 8
#define COO 1
#define CO 32
#define YYY 32
#define XXX 1
#define YY 2
#define XX 2
#define Y 1
#define X 32
#define TOTAL_IX (XXX * XX * X + KX - 1)
#define TOTAL_IY (YYY * YY * Y + KY - 1)
#define TOTAL_OX (XXX * XX * X)
#define TOTAL_OY (YYY * YY * Y)
#define TOTAL_CO (COOO * COO * CO)
#define TOTAL_CI (CII * CI)
#define N 4
#define SIZE_I_0 TOTAL_CI *N
#define SIZE_I_1 TOTAL_IY *TOTAL_IX
#define SIZE_K_0 TOTAL_CO *KX
#define SIZE_K_1 TOTAL_CI *KY
#define SIZE_O_0 TOTAL_CO *N
#define SIZE_O_1 TOTAL_OY *TOTAL_OX
#ifndef GPU
using namespace Halide;
#endif
double get_event_time(event e) {
  double start = e.get_profiling_info<info::event_profiling::command_start>();
  double end = e.get_profiling_info<info::event_profiling::command_end>();
  return end - start;
}
int main() {
  float *i = new float[SIZE_I_0 * SIZE_I_1];
  float *k = new float[SIZE_K_0 * SIZE_K_1];
  float *o = new float[SIZE_O_0 * SIZE_O_1];
  for (unsigned m = 0; m < SIZE_I_0 * SIZE_I_1; ++m)
    i[m] = random() % 5;
  for (unsigned m = 0; m < SIZE_K_0 * SIZE_K_1; ++m)
    k[m] = random() % 5;
  for (unsigned m = 0; m < SIZE_O_0 * SIZE_O_1; ++m)
    o[m] = random() % 5;
  // Specifications to be preprocessed
  // std::vector<int> a_dim{TOTAL_K, TOTAL_I};
  // std::vector<int> b_dim{TOTAL_J, TOTAL_K};
  // std::vector<int> c_dim{JJJ, III, JJ, II, J, I};

  size_t i_dim[2] = {SIZE_I_0, SIZE_I_1};
  size_t k_dim[2] = {SIZE_K_0, SIZE_K_1};
  size_t o_dim[2] = {SIZE_O_0, SIZE_O_1};

#ifdef FPGA
#ifndef GPU
  Halide::Runtime::Buffer<float, (sizeof(i_dim) / sizeof(size_t))> I_h(
      i, std::vector<size_t>(std::begin(i_dim), std::end(i_dim)));
#endif
#endif
#ifdef GPU
  int _I_extent_0 = i_dim[0];
  int _I_extent_1 = i_dim[1];
  sycl::image<2> img_I(i, image_channel_order::rgba, image_channel_type::fp32,
                       range<2>{i_dim[0] / 4, i_dim[1]});
#endif
#ifdef FPGA
#ifndef GPU
  Halide::Runtime::Buffer<float, (sizeof(k_dim) / sizeof(size_t))> K_h(
      k, std::vector<size_t>(std::begin(k_dim), std::end(k_dim)));
#endif
#endif
#ifdef GPU
  int _K_extent_0 = k_dim[0];
  int _K_extent_1 = k_dim[1];
  sycl::image<2> img_K(k, image_channel_order::rgba, image_channel_type::fp32,
                       range<2>{k_dim[0] / 4, k_dim[1]});
#endif
#ifdef FPGA
#ifndef GPU
  Halide::Runtime::Buffer<float, (sizeof(o_dim) / sizeof(size_t))> O_h(
      o, std::vector<size_t>(std::begin(o_dim), std::end(o_dim)));
#endif
#endif
#ifdef GPU
  int _O_extent_0 = o_dim[0];
  int _O_extent_1 = o_dim[1];
  sycl::image<2> img_O(o, image_channel_order::rgba, image_channel_type::fp32,
                       range<2>{o_dim[0] / 4, o_dim[1]});
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
  exec_time = conv(device_selector, I_h.raw_buffer(), K_h.raw_buffer(),
                   O_h.raw_buffer());
  std::cout << "Run completed!\n";
  std::cout << "kernel exec time: " << exec_time << "\n";
#endif

#ifdef GPU
  queue q(esimd_test::ESIMDSelector{}, esimd_test::createExceptionHandler(),
          property::queue::enable_profiling{});
  range<3> GlobalRange(32 * 2, 32 * 2, (_I_extent_0 / 256) * 1);
  range<3> LocalRange(2, 2, 1);
  const int nd_item_dimension = 3;
  double _time;
  double ops = 2.0 * (long)(N * TOTAL_OX * TOTAL_OY * TOTAL_CO) * (long)(TOTAL_CI * KX * KY);
  double min_time = RAND_MAX;
  double total_time = 0.0f;
  for (int i = 0;i < 100 ; i++){
  auto e = q.submit([&](handler &cgh) {
    auto _I = img_I.get_access<uint4, access::mode::read>(cgh);
    auto _K = img_K.get_access<uint4, access::mode::read>(cgh);
    auto _O = img_O.get_access<uint4, access::mode::write>(cgh);
    cgh.parallel_for<class Test>(
        nd_range{GlobalRange, LocalRange},
        [=](nd_item<nd_item_dimension> ndi) SYCL_ESIMD_KERNEL {
          using namespace sycl::ext::intel::esimd;
          int _A_s0_n___block_id_z = ndi.get_group(2);
          int _A_s0_co___block_id_y = ndi.get_group(1);
          int _A_s0_x___block_id_x = ndi.get_group(0);
          int ___thread_id_y = ndi.get_local_id(1);
          int ___thread_id_x = ndi.get_local_id(0);
          simd<float, 256> _C;
          simd<float, 272> _I_im_buf;
          simd<float, 192> _K_im_buf;
          _C.select<256, 1>(0) = 0.000000;
          for (int _A_s0_ci = 0; _A_s0_ci < 0 + 32; _A_s0_ci++) {
            for (int _A_s0_kx = 0; _A_s0_kx < 0 + 3; _A_s0_kx++) {
              int _168 = (_A_s0_ci * 8);
              int _169 = (((_A_s0_kx * 32) + _A_s0_co___block_id_y) * 8);
              _K_im_buf.select<64, 1>(0)
                  .bit_cast_view<float, 8, 8>()
                  .select<8, 1, 8, 1>(0, 0) =
                  media_block_load<float, 8, 8>(_K, (_169 * 4), (_168 + 0));
              _K_im_buf.select<64, 1>(64)
                  .bit_cast_view<float, 8, 8>()
                  .select<8, 1, 8, 1>(0, 0) = media_block_load<float, 8, 8>(
                  _K, (_169 * 4), ((_168 + 256) + 0));
              _K_im_buf.select<64, 1>(128)
                  .bit_cast_view<float, 8, 8>()
                  .select<8, 1, 8, 1>(0, 0) = media_block_load<float, 8, 8>(
                  _K, (_169 * 4), ((_168 + 512) + 0));
              int _170 =
                  (((((_A_s0_x___block_id_x * 2) + ___thread_id_y) + _A_s0_kx) *
                    66) +
                   (___thread_id_x * 32));
              int _171 = (((_A_s0_n___block_id_z * 32) + _A_s0_ci) * 8);
              _I_im_buf.select<272, 1>(0)
                  .bit_cast_view<float, 34, 8>()
                  .select<8, 1, 8, 1>(0, 0) =
                  media_block_load<float, 8, 8>(_I, (_171 * 4), (_170 + 0));
              _I_im_buf.select<272, 1>(0)
                  .bit_cast_view<float, 34, 8>()
                  .select<8, 1, 8, 1>(8, 0) =
                  media_block_load<float, 8, 8>(_I, (_171 * 4), (_170 + 8));
              _I_im_buf.select<272, 1>(0)
                  .bit_cast_view<float, 34, 8>()
                  .select<8, 1, 8, 1>(16, 0) =
                  media_block_load<float, 8, 8>(_I, (_171 * 4), (_170 + 16));
              _I_im_buf.select<272, 1>(0)
                  .bit_cast_view<float, 34, 8>()
                  .select<8, 1, 8, 1>(24, 0) =
                  media_block_load<float, 8, 8>(_I, (_171 * 4), (_170 + 24));
              _I_im_buf.select<272, 1>(0)
                  .bit_cast_view<float, 34, 8>()
                  .select<2, 1, 8, 1>(32, 0) =
                  media_block_load<float, 2, 8>(_I, (_171 * 4), (_170 + 32));
#pragma unroll
              for (int _A_s0_ky = 0; _A_s0_ky < 0 + 3; _A_s0_ky++) {
#pragma unroll
                for (int _A_s0_yyy = 0; _A_s0_yyy < 0 + 32; _A_s0_yyy++) {
#pragma unroll
                  for (int _A_s0_cii = 0; _A_s0_cii < 0 + 8; _A_s0_cii++) {
                    int _172 =
                        cm_corr_buf_idx_f20(((_A_s0_ky * 256) + _A_s0_cii));
                    _C.select<8, 1>((_A_s0_yyy * 8)) =
                        (_C.select<8, 1>((_A_s0_yyy * 8)) +
                         (_K_im_buf.select<8, 1>((_172 * 8)) *
                          _I_im_buf[(((_A_s0_ky + _A_s0_yyy) * 8) +
                                     _A_s0_cii)]));
                  } // for _A_s0_cii
                }   // for _A_s0_yyy
              }     // for _A_s0_ky
            }       // for _A_s0_kx
          }         // for _A_s0_ci
          int _173 = (((((_A_s0_x___block_id_x * 2) + ___thread_id_y) * 2) +
                       ___thread_id_x) *
                      32);
          int _174 =
              (((_A_s0_n___block_id_z * 32) + _A_s0_co___block_id_y) * 8);
          media_block_store<float, 8, 8>(_O, (_174 * 4), (_173 + 0),
                                         _C.select<256, 1>(0)
                                             .bit_cast_view<float, 32, 8>()
                                             .select<8, 1, 8, 1>(0, 0));
          media_block_store<float, 8, 8>(_O, (_174 * 4), (_173 + 8),
                                         _C.select<256, 1>(0)
                                             .bit_cast_view<float, 32, 8>()
                                             .select<8, 1, 8, 1>(8, 0));
          media_block_store<float, 8, 8>(_O, (_174 * 4), (_173 + 16),
                                         _C.select<256, 1>(0)
                                             .bit_cast_view<float, 32, 8>()
                                             .select<8, 1, 8, 1>(16, 0));
          media_block_store<float, 8, 8>(_O, (_174 * 4), (_173 + 24),
                                         _C.select<256, 1>(0)
                                             .bit_cast_view<float, 32, 8>()
                                             .select<8, 1, 8, 1>(24, 0));
        } // kernel kernel_A
    );
  });
  e.wait();
  _time = get_event_time(e);
  if (_time < min_time) {
    min_time = _time;
  }
  total_time += _time;
  }
  std::cout << "Size of tensor I: " << N << ", " << TOTAL_CI << ", " << TOTAL_IX << ", " << TOTAL_IY << "\n";
  std::cout << "Size of tensor K: " << TOTAL_CI << ", " << TOTAL_CO << ", " << KX << ", " << KY << "\n";
  std::cout << "Average Throughput: " << ops / (total_time / 100) << "\n";
  std::cout << "Max Throughput: " << ops / min_time << "\n";
#endif
}
