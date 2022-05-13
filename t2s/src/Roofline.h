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
#ifndef T2S_ROOFLINE_H
#define T2S_ROOFLINE_H

#define STR_SIZE 100

int DSPs();
double FMax();
double ExecTime(const char* kernel_name = 0);
void roofline(double mem_bandwidth, double compute_roof, double number_ops, double number_bytes, double exec_time);

// Used for FPGA report generated through DPC++ OneAPI
int DSPs_oneapi();
double FMax_oneapi();

#endif
