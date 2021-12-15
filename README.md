T2SP (Temporal To Spatial Programming, previously called T2S) enables software programmers to build systolic arrays for dense tensor computes with portable performance across spatial architectures (like FPGAs) and vector architectures (like GPUs) in a constructive  way.

T2SP is available under a permissive license, the [BSD+Patent license](./LICENSE.md). 

Currently, we support only Intel FPGAs and GPUs. We assume your device is local to you, or within Intel DevCloud, and the operating system is Linux (We have tried Ubuntu 18.04 and CentOS 7.9, but our system is not really tied to any specific Linux system or version). Other platforms might also work, although not tested. 

# [DevCloud] Open an account (once)

 + Register at the [Intel's FPGA DevCloud](https://software.intel.com/content/www/us/en/develop/tools/devcloud/fpga.html). This will enable access to both the FPGAs and the GPUs in the cloud. Currently, the cloud offers Arria 10  and Stratix 10 FPGAs, and GEN 9.5 (Intel UHD Graphics P630) and GEN 12 ( Intel Iris Xe MAX Graphics) GPUs.

 + Follow the instructions of an approval email to set up your connection to DevCloud.

 + Connect to DevCloud. Now you are at the **head node** named `login-2`.
   
 + Add the following to your .bashrc:
   
   ```
    if [ -f /data/intel_fpga/devcloudLoginToolSetup.sh ]; then
        source /data/intel_fpga/devcloudLoginToolSetup.sh
    fi
   ```
   Then
   ```
    source .bashrc
   ```

# Clone T2SP (once)

   ```
   git clone https://github.com/IntelLabs/t2sp 
   ```

# Install tools (once)

+ [DevCloud] From the **head node**, submit a job with one of the following commands, based on the type of device you will use: 

  ```
  # For Arria 10 FPGA
  qsub -q batch@v-qsvr-fpga -l nodes=arria10:ppn=2 -d $HOME/t2sp $HOME/t2sp/install-tools.sh
  
  # For Stratix 10 FPGA
  qsub -q batch@v-qsvr-fpga -l nodes=darby:ppn=2  -d $HOME/t2sp $HOME/t2sp/install-tools.sh
  
  # For GEN 9.5 GPU
  qsub -l nodes=1:gen9:ppn=2 -d $HOME/t2sp $HOME/t2sp/install-tools.sh 
  
  # For GEN 12 GPU
  qsub -l nodes=1:iris_xe_max:ppn=2 -d $HOME/t2sp $HOME/t2sp/install-tools.sh
  ````
  This may take 1-5 hours on DevCloud, depending on the specific machine allocated for the job. 
  
  A known issue: on a GEN 9.5 GPU machine, it is possible to see some errors during installing `m4`, but it turns out that package is not necessary for that machine, and we can ignore the error.  
  
+ [Local machine with an FPGA or a GPU]

  ```
  cd $HOME/t2sp
  ./install-tools.sh
  ```
  
+ [Local machine with an FPGA] Also download [Intel FPGA SDK for OpenCL](http://dl.altera.com/opencl/), and install with
  
  ```
  tar -xvf AOCL-pro-*-linux.tar 
  ./setup_pro.sh
  ```

Note: 

+ We assume your system has python >= 2.7 already installed.
+ The above `install-tools.sh` command installs llvm-clang >= 9.0, gcc >= 7.5.0, and python's numpy and matplotlib package. The command installs all of them and their dependencies we know to make the system self-contained. If your system has some of the tools already installed, you could edit `install-tools.sh` to disable the installations of these tools, then modify the environment setting as shown below.

# Modify the environment setting (once)

The environment setting file is in `$HOME/t2sp/setenv.sh`. 

+ If you have your own gcc, llvm or clang and thus did not use the above `install-tools.sh` command to install them, in `setenv.sh`, modify the following path variables appropriately:

```
  GCC_PATH=...
  export LLVM_CONFIG=...
  export CLANG=...
```

+ If you installed the Intel FPGA SDK for OpenCL for your local FPGA, check the following variables, and modify if needed:

  ```
  ALTERA_PATH=...
  AOCL_VERSION=...
  FPGA_BOARD_PACKAGE=...
  export FPGA_BOARD=...
  export LM_LICENSE_FILE=...
  ```

  Here is an example how to find out the board package and board (Assume Intel FPGA SDK for OpenCL 19.1 was installed under directory `$HOME/intelFPGA_pro`):

  ```
  $HOME/intelFPGA_pro/19.1/hld/bin/aoc -list-boards
     Board list:
       a10gx
         Board Package: $HOME/intelFPGA_pro/19.1/hld/board/a10_ref
    
       a10gx_hostpipe
         Board Package: $HOME/intelFPGA_pro/19.1/hld/board/a10_ref
  ```

  There are 1 board package and 2 boards in this case, and you should set `FPGA_BOARD_PACKAGE=a10_ref`, and either `export FPGA_BOARD=a10gx` or `export FPGA_BOARD=a10gx_hostpipe`.

# Open a terminal on a compute node

[DevCloud] from the head node, log into a **compute node**:

+ FPGA: 
  ```
    devcloud_login
  ```
    Choose         
    ```
    6) Enter Specific Node Number
    ```
    Enter the name of a node with Arria 10 Release 1.2.1, or with Stratix 10.

+ GPU: to request a compute node with GEN 9.5 or GEN 12,
  
    ```
    qsub -I -l nodes=1:gen9:ppn=2  
    ```
    
    or 
    
    ```
    qsub -I -l nodes=1:iris_xe_max:ppn=2
    ```

[Local] Open a bash shell

For all the steps below, we assume you are either on a compute node of DevCloud or on a local machine, except explicitly stated otherwise.

# Set up the environment (whenever a terminal is open)

```
cd $HOME/t2sp
source ./setenv.sh (devcloud|local) (fpga|gpu)
```
The options say if you are working on DevCloud or locally, and to use an FPGA or a GPU. 

# Build T2SP (whenever you change the source code)

```
cd $HOME/t2sp/Halide
make -j
```

# Regression tests

```
cd $HOME/t2sp/t2s/tests/correctness
./test.sh
```
After the testing, each sub-directory there will contain a success.txt and/or failure.txt, which have the command lines for compiling and running every test. These tests are small examples one can play with.

To remove all the temporary files generated during the regression testing:

```
./test.sh clean
```

# Performance tests
Current release contains SGEMM, 2-D convolution and Capsule convolution on Arria 10 FPGA and GEN 9.5 GPU. For every kernel, we write a **single** specification that gets mapped to the different kinds of hardware. This reflects our concept of "**write a kernel once, and run with high performance across spatial and vector architectures**".

Summary of throughput:

|     | A10 | GEN 9.5 |
| --- | :-: | :-:    |
| SGEMM |  533 GFLOPS, 95% DSP efficiency  |  412 GFLOPS, 90% machine peak |
| 2-D convolution | 524 GFLOPS, 98% DSP efficiency | 422 GFLOPS, 92% machine peak |
| Capsule convolution | 487 GFLOPS, 94% DSP efficiency | 398 GFLOPS, 87% machine peak |

To reproduce the performance,
```
cd $HOME/t2sp/t2s/tests/performance
```
then 
+ [DevCloud head node] Submit a job:

  ```
  # Test all kernels
  ./devcloud-jobs.sh (a10|gen9)
    
  # Or test 1 kernel
  ./devcloud-job.sh (gemm|conv|capsule) (a10|gen9) (tiny|large) (hw|emulator)
  ```
+ [A DevCloud compute node, or a local machine] Test directly:
  
  ```
  # Test all kernels
  ./tests.sh (devcloud|local) (a10|gen9)
    
  # Or test 1 kernel
  ./test.sh (devcloud|local) (gemm|conv|capsule) (a10|gen9) (tiny|large) (hw|emulator)
  ```

Note:
+ The emulator option is applicable only to FPGAs and tiny size.
+ Synthesis of an FPGA design will take hours. So on DevCloud, we recommend submitting a job for testing on FPGAs.
+ As for the results, look for the synthesis report of an FPGA design in `KERNEL/a/reports/report.html`. Here KERNEL is gemm, conv, etc. 
+ Look for the performance of an FPGA design in a roofline model that is automatically generated in `KERNEL/roofline.png`.
+ Look for the performance of a GPU design from the standard output.

# Features

The current release contains the following features:

+ Expressing systolic arrays
  
  UREs (uniform recurrence equations) and space-time transforms are supported for expressing systolic arrays in general. Currently, a space-time transform must be unimodular. 
  
+ Defining an abstract, performance portable memory hierarchy 

  A memory hierarchy is defined for each tensor by streaming the tensor across DRAM, SRAM, and registers. The memory hierarchy is then specialized by the compiler for specific hardware with portable performance. 

+ Isolation

  Split a compute into spatial pieces, so that each piece can be optimized individually.

+ Data optimizations

  Data gathering, scattering, double buffering, serialization and de-serialization

+ Loop optimizations

  Loop flattening, removal, unrolling, vectorization

# Tutorials

A 10-minute video `intro.mp4`, located at the root of the repository, introduces the basic concept of T2SP. There are also a set of [tutorials](https://github.com/intel/FPGA-Devcloud/tree/master/main/QuickStartGuides/T2S) at DevCloud. 

# Next release
We aim to release the following between December, 2021 and February, 2022:
+ Throughput numbers of SGEMM, 2-D convolution, and Capsule convolution on GEN 12 GPU.
+ Initial throughput numbers for Stratix 10 FPGA. 
+ Embedding of T2SP into OneAPI for kernel programming on FPGAs, GPUs and CPUs.

# Citation

If you use T2SP, please cite the following position paper:

```
@article{T2SP,
  author    = {Hongbo Rong},
  title     = {Programmatic Control of a Compiler for Generating High-performance Spatial Hardware},
  journal   = {CoRR},
  volume    = {abs/1711.07606},
  year      = {2017},
  url       = {http://arxiv.org/abs/1711.07606},
  archivePrefix = {arXiv},
  eprint    = {1711.07606},
  timestamp = {Mon, 13 Aug 2018 16:46:47 +0200},
  biburl    = {https://dblp.org/rec/journals/corr/abs-1711-07606.bib},
  bibsource = {dblp computer science bibliography, https://dblp.org},
  note      = {Open source available at https://github.com/IntelLabs/t2sp}
}
```

# Publications

+ **SuSy: a programming model for productive construction of high-performance systolic arrays on FPGAs**. 
Yi-Hsiang Lai, Hongbo Rong, Size Zheng, Weihao Zhang, Xiuping Cui, Yunshan Jia, Jie Wang, Brendan Sullivan, Zhiru Zhang, Yun Liang, Youhui Zhang, Jason Cong, Nithin George, Jose Alvarez, Christopher Hughes, and Pradeep Dubey. 2020.  ICCAD'20. [Link](https://ieeexplore.ieee.org/document/9256583) 

+ **T2S-Tensor: Productively Generating High-Performance Spatial Hardware for Dense Tensor Computations**. 
Nitish Srivastava, Hongbo Rong, Prithayan Barua, Guanyu Feng, Huanqi Cao, Zhiru Zhang, David Albonesi,Vivek Sarkar, Wenguang Chen, Paul Petersen, Geoff Lowney, Adam Herr, Christopher Hughes,Timothy Mattson, Pradeep Dubey. FCCM, 2019. [Link](https://ieeexplore.ieee.org/document/8735529)

# [Acknowledgement](./ACKNOWLEDGEMENT.md)
