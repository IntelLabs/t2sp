# Capsule convolution

Capsule convolution is defined as follows [1]:

![capsule-original-equation](figures/capsule-equation.png)

where `s` is the stride, operation `·` is matrix multiplication, and `V`, `P`, and `W` are all 4-dimensional arrays of 4x4 matrices.  In our design, we assume  stride `s=2`.  

## Performance (single precision)

| Device | Frequency | Throughput | Logic utilization | DSPs | BRAMs | DSP Efficiency |
| ------ | --------- | ------ | --------- | ---- | ----- | -------------- |
| Intel Arria 10 GX 1150 FPGA | 210 MHz | 534 GFLOPS | 214,384 / 427,200 ( 53 % ) | 1,295 / 1,518 ( 85 % ) | 1,866 / 2,713 ( 69 % ) | 98%   |
| Intel GEN9.5 GPU | 1200 MHz | 414 GFLOPS | - | - | - | 90%   |

Note:

- The DSP efficiency of an FPGA equals measured throughput/theoretical peak throughput with the synthesized clock frequency.
  - Measured throughput in GFLOPS = #operations / execution time in nanoseconds.
  - Given the definition of capsule convolution above, #operations =  2 * (4 * 4 * 4) * (product of the extents of the 4 dimensions of tensor `V` in the equation) * (product of the extents of `kx`, `ky` and `ci` in the equation), where the factor 2 accounts for two operations: multiply and add.
  - Theoretical peak throughput = frequency * 2 (multiply and add)  * 1518 (#DSPs).
- The machine peak of GEN9.5 for single-precision computes is calculated as 1200Mhz (the GPU's clock frequency) * 2 (multiply and add) * 2 (FPUs) * 4 (SIMD4) * 24 (EUs) = 460.8 GFlOPS.  Refer to [GEN architecture document](https://www.intel.com/content/dam/develop/external/us/en/documents/the-compute-architecture-of-intel-processor-graphics-gen9-v1d0.pdf) for more details.

## Design

Below is the GPU design. The FPGA design is similar and skipped here.

![Design](figures/capsule-design.png)

In this design, the original 7 loops are manually tiled and ordered; `CII`, etc. are static constants and are the extents of loop `cii`, etc. 

On a GPU, some loops are made block loops,  and some loops are made thread loops, to create parallel threads. On an FPGA, there  is only one thread, since multi-threading is not efficient on FPGAs. To drain results out of the device, we also add extra drain loops for each thread. Any loop that is not specially annotated is a sequential loop. 

Data move as the loops run. For tensor`P` and `W` ,  we create abstract memories for them, `DP` and `DW`, that are resident in device DRAM, and serve as global user-managed caches. Above them, we create another level of abstract memories, `SP` and `SW`, that are resident in device SRAM, and serve as per-block user-managed caches.

Each thread contains a systolic array as its compute engine. This systolic array is created with UREs and a space-time transform. After the systolic array finishes execution, its results are drained through two levels of abstract memories, `RV2` and `RV1`,  that are resident in registers, and into the last abstract memory, `DV`, which is in the device's DRAM.

Note that an abstract memory outputs a tensor each time. For example, `DP` outputs a vector of size `CII` each time. So an abstract memory is named streaming tensor (stensor). 

## [How to test the design](../../../../README.md#Performance-tests)

## References

1. Paul Barham and Michael Isard. Machine learning systems are stuck in a rut. In Proceedings of the Workshop on Hot Topics in Operating Systems, pages 177–183, 2019.  
