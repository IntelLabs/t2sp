# PairHMM

## Performance (single precision)

| Device | Frequency | Throughput | Logic utilization | DSP blocks | RAM blocks | Efficiency | Matrix Size | Device compiler |
| ------ | --------- | ---------- | ----------------- | ---------- | ---------- | -----------| ----------- | --------------- |
| Intel Arria 10 GX 1150 | 229 MHz | 41.8 GCups | 238,543 / 427,200 ( 56 % )| 1,414 / 1,518 ( 93 % ) | 1,092 / 2,713 ( 40 % ) | 95% PE efficiency | R(224,128) * H(224,384) | aoc 19.4.0 (on s001-n139) |
| Intel Stratix 10 SX 2800 | 268 MHz | 47.9 GCups | 531,954 / 933,120 ( 57 % ) | 1,402 / 5,760 ( 24 % ) | 1,518 / 11,721 ( 13 % ) | 93% DSP efficiency | R(224,128) * H(224,384) | aoc 19.2.0 (on s001-n005) |
| Intel GEN9.5 GPU | 1200 MHz | 4.25 GCups | - | - | - | -  | R(64,8) * H(64,8) | CM SDK 20200119 |
| Intel GEN12 GPU | 1650 MHz | 14.8 GCups | - | - | - | -  | R(128,8) * H(128,8) | CM SDK 20211028 |

Note:  when [measuring the performance](../README.md#Performance-metrics),

- The length of Haps and Reads are fixed. We are working towards enabling variable length.


## [Understand the design](../README.md#how-to-understand-a-design)

## [Test the design](../../../../README.md#Performance-tests)
