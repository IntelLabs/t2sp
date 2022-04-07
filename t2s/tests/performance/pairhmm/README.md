# PairHMM

## Performance (single precision)

| Device | Frequency | Throughput | Logic utilization | DSP blocks | RAM blocks | PE Efficiency | Tensor Sizes | Device compiler |
| ------ | --------- | ------ | --------- | ---- | ----- | -------------- |----- | -------------- |
| Intel Arria 10 GX 1150 FPGA | 229 MHz | 42.5 GCups | 238,543 / 427,200 ( 56 % ) | 1,414 / 1,518 ( 93 % ) | 1,092 / 2,713 ( 40 % ) | 97%  | Read(224 * 128) * Hap(224 * 384) | aoc 19.4.0 |
| Intel GEN9.5 GPU | 1200 MHz | 4.25 GCups | - | - | - | -  | Read(64 * 8) * Hap(64 * 8) | CM Dev Package 20200119 |
| Intel GEN12 GPU | 1650 MHz | 14.8 GCups | - | - | - | -  | Read(128 * 8) * Hap(128 * 8) | CM SDK 20211028 |

Note:  when [measuring the performance](../README.md#Performance-metrics),

- The length of Haps and Reads are fixed. We are working towards enabling variable length.


## [Understand the design](../README.md#how-to-understand-a-design)

## [Test the design](../../../../README.md#Performance-tests)
