# Matrix Multiply

![Matrix multiply](figures/gemm-equation.png)

## Performance (single precision)

| Device | Frequency | Throughput | Logic utilization | DSP blocks | RAM blocks | Efficiency | Matrix Size | Device compiler |
| ------ | --------- | ---------- | ----------------- | ---------- | ---------- | -----------| ----------- | --------------- |
| Intel Arria 10 GX 1150 | 244 MHz | 620 GFLOPS | 207,854 / 427,200 ( 49 % ) | 1,305 / 1,518 ( 86 % ) | 2,075 / 2,713 ( 76 % ) | 97% DSP efficiency | A(10K,16K) * B(16K,8K) | aoc 19.4.0 (on s001-n137) |
| Intel Stratix 10 SX 2800 | 251 MHz | 1790 GFLOPS | 446,671 / 933,120 ( 48 % ) | 3,605 / 5,760 ( 63 % ) | 4,054 / 11,721 ( 35 % ) | 99% DSP efficiency | A(14K,16K) * B(16K,16K) | aoc 19.2.0 (on s005-n005) |
| Intel GEN9.5 GPU | 1200 MHz | 410 GFLOPS | - | - | - | 90% machine peak | A(2K,1K) * B(1K,2K) | CM Dev Package 20200119 |
| Intel GEN12 GPU | 1650 Mhz | 2165 GFLOPS | - | - | - | 85% machine peak | A(2K,2K) * B(2K,2K) | CM SDK 20211028 |

Note: when [measuring the performance](../README.md#Performance-metrics),

- Given the above definition of matrix multiply, #operations =  2 * (size of matrix `C`) * (extent of `k` in the equation), where the factor 2 accounts for two operations: multiply and add.
- We reported the maximum throughput for 100 runs on GEN9.5 GPU, as the number largely fluctuated. But we observed it is higher and more stable on our local machine, with the same product model.
- We use different cmc compilers for GEN9.5 and GEN12 GPU, as we find the newer one has limited support for GEN9.5 GPU. The performance is significantly decreased.

## Design

![Design](figures/gemm-design.png)

## [Understand the design](../README.md#how-to-understand-a-design)

## [Test the design](../../../../README.md#Performance-tests)
