# Capsule convolution

Capsule convolution is defined as follows [1]:

![capsule-original-equation](figures/capsule-equation.png)

where `s` is the stride, operation `·` is matrix multiplication, and `V`, `P`, and `W` are all 4-dimensional arrays of 4x4 matrices.  In our design, we assume  stride `s=2`.  

## Performance (single precision)

| Device | Frequency | Throughput | Logic utilization | DSPs | BRAMs | DSP Efficiency |
| ------ | --------- | ------ | --------- | ---- | ----- | -------------- |
| Intel Arria 10 GX 1150 FPGA | 210 MHz | 534 GFLOPS | 214,384 / 427,200 ( 53 % ) | 1,295 / 1,518 ( 85 % ) | 1,866 / 2,713 ( 69 % ) | 98%   |
| Intel GEN9.5 GPU | 1200 MHz | 414 GFLOPS | - | - | - | 90%   |

Note:  when [measuring the performance](../README.md#Performance-metrics),

- Given the definition of capsule convolution above, #operations =  2 * (4 * 4 * 4) * (product of the extents of the 4 dimensions of tensor `V` in the equation) * (product of the extents of `kx`, `ky` and `ci` in the equation), where the factor 2 accounts for two operations: multiply and add.

## Design

Below is the GPU design. The FPGA design is similar and skipped here.

![Design](figures/capsule-design.png)

## [Test the design](../../../../README.md#Performance-tests)

## References

1. Paul Barham and Michael Isard. Machine learning systems are stuck in a rut. In Proceedings of the Workshop on Hot Topics in Operating Systems, pages 177–183, 2019.  
