# Capsule convolution

| Device | Frequency | Throughput | Logic utilization | DSPs | BRAMs | DSP Efficiency |
| ------ | --------- | ------ | --------- | ---- | ----- | -------------- |
| Intel Arria 10 GX 1150 FPGA | 210 MHz | 534 GFLOPS | 214,384 / 427,200 ( 53 % ) | 1,295 / 1,518 ( 85 % ) | 1,866 / 2,713 ( 69 % ) | 98%   |
| Intel GEN9.5 GPU | 1200 MHz | 414 GFLOPS | - | - | - | 90%   |

The test can be reproduced by logging into a compute node on Intel FPGA DevCloud with an A10 card and 1.2.1 software stack, and following the instructions below.

## Key Implementation Details

This design contains several important optimizations:
- Generating a 10x8 systolic array with UREs and a space-time transform
- Generating double buffers for input data, with multiple banks connected as a daisy chain
- Vectorizing input data to utilize the bandwidth of the device's DRAM
- Generating two-level output buffers in registers

The compiler implements the above optimizations. In addition, the compiler automatically performs the following optimizations so that the generated code lends itself to the downstream EDA tools for further optimizations:
- Identifying and rewriting the inner-product pattern
- Moving  channel accesses at the boundaries of the systolic array outside the unrolled loops

## [How to test the design](../../../../README.md#Performance-tests)

### References

1. Paul Barham and Michael Isard. Machine learning systems are stuck in a rut. In Proceedings of the Workshop on Hot Topics in Operating Systems, pages 177–183, 2019.  
