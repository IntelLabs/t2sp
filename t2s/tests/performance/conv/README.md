# 2-D convolution

2-D convolution is defined as follows [1]:

![2dconv-original-equation](figures/conv-equation.png) 

where `s` is the stride, operation `·` is scalar multiplication, and `O`, `I`, and `K` are all 4-dimensional arrays of scalars.  In this design, we assume  stride `s=1`.  

## Performance (single precision)

| Device | Frequency | Throughput | Logic utilization | DSPs | BRAMs | Efficiency | Tensor Sizes | Device compiler |
| ------ | --------- | ------ | --------- | ---- | ----- | -------------- | ----- | -------------- |
| Intel Arria 10 GX 1150 FPGA | 206 MHz | 515 GFLOPS | 257,558 / 427,200 ( 60 % ) | 1,299 / 1,518 ( 86 % ) | 2,011 / 2,713 ( 74 % ) | 96%   | I(64,256,1x60+3,1x60+3) * K(256,256,3,3) | aoc 19.4.0 |
| Intel GEN9.5 GPU | 1200 MHz | 437 GFLOPS | - | - | - | 95%   | I(64,256,1x64+3,1x64+3) * K(256,256,3,3) | CM Dev Package 20200119 |

Note:

- The DSP efficiency of an FPGA equals measured throughput/theoretical peak throughput with the synthesized clock frequency.
  - Measured throughput in GFLOPS = #operations / execution time  in nanoseconds.
  - Given the definition of 2-D convolution  above, #operations =  2 * (size of tensor `O` in the equation) * (multiplication of the extents of `kx`, `ky` and `ci` in the equation), where the factor 2 accounts for two operations: multiply and add.
  - Theoretical peak throughput = frequency  * 2 (multiply and add)  * 1518 (#DSPs).
- The machine peak of GEN9.5 for single-precision computes is calculated as 1200Mhz (the GPU's clock frequency) * 2 (multiply and add) * 2 (FPUs) * 4 (SIMD-4) * 24 (EUs) = 460.8 GFlOPS.  Refer to [GEN architecture document](https://www.intel.com/content/dam/develop/external/us/en/documents/the-compute-architecture-of-intel-processor-graphics-gen9-v1d0.pdf) for more details.

## Design

![Design](figures/conv-design.png)

In this design, the original 7 loops are manually tiled and ordered; `CII`, `YYY` and `COOO` are static constants and are the extents of loop `cii`, `yy`y, and `cooo`, respectively. 

On a GPU, some loops are made block loops,  and some loops are made thread loops, to create parallel threads. On an FPGA, there  is only one thread, since multi-threading is not efficient on FPGAs. To drain results out of the device, we also add extra drain loops for each thread. Any loop that is not specially annotated is a sequential loop. 

Data move as the loops run. For tensor`I` and `K` ,  we create abstract memories for them, `DI` and `DK`, that are resident in device DRAM, and serve as global user-managed caches. Above them, we create another level of abstract memories, `SI` and `SK`, that are resident in device SRAM, and serve as per-block user-managed caches.

Each thread contains a systolic array as its compute engine. This systolic array is created with UREs and a space-time transform. After the systolic array finishes execution, its results are drained through two levels of abstract memories, `RO2` and `RO1`,  that are resident in registers, and into the last abstract memory, `DO`, which is in the device's DRAM.

Note that an abstract memory outputs a tensor each time. For example, `DI` outputs a vector of size `CII` each time. So an abstract memory is named streaming tensor (stensor). 

## [How to test the design](../../../../README.md#Performance-tests)

## References

1. Paul Barham and Michael Isard. Machine learning systems are stuck in a rut. In Proceedings of the Workshop on Hot Topics in Operating Systems, pages 177–183, 2019.  
