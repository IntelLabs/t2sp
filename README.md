T2SP (Temporal To Spatial Programming, previously called T2S) enables software programmers to build systolic arrays for dense tensor computes with portable performance across spatial architectures (like FPGAs) and vector architectures (like GPUs) in a constructive  and productive way.

T2SP is available under a permissive license, the [BSD+Patent license](./LICENSE.md). 

Currently, we support only Intel FPGAs and GPUs. We assume your device is local to you, or within Intel DevCloud, and the operating system is Linux. Other platforms might also work, but are not tested. 

# [DevCloud] Open an account

 + Register at [DevCloud](https://software.intel.com/content/www/us/en/develop/tools/devcloud/fpga.html). This will enable access to both FPGAs and GPUs in the cloud.

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

+ Local machine with an FPGA: Install the Intel FPGA SDK for OpenCL, 19.1 or above:

  + Download [Intel FPGA SDK for OpenCL](http://dl.altera.com/opencl/)

  + Install

    ```
    tar -xvf AOCL-pro-*-linux.tar 
    ./setup_pro.sh
    ```
  
+ Local machine with an FPGA or a GPU: install all the other tools needed:

  ```
  cd $HOME/t2sp
  ./install-tools.sh
  cd -
  ```

+ DevCloud: install all the other tools needed (You should be at the head node for this step):
  ```
  cd $HOME/t2sp
  
  # Choose one of the commands below 
  
  # Do this if you will build T2S for A10 FPGA
  qsub -q batch@v-qsvr-fpga -l nodes=arria10:ppn=2 ./install-tools.sh

  # Or do this if you will build T2S for S10 FPGA

  qsub -q batch@v-qsvr-fpga -l nodes=darby:ppn=2 ./install-tools.sh

  # Or do this if you will build T2S for GEN9 GPU
  qsub -l nodes=1:gen9:ppn=2 ./install-tools.sh

  # Or do this if you will build T2S for GEN12 GPU
  qsub -l nodes=1:iris_xe_max:ppn=2 ./install-tools.sh

  cd -
  ````
  This may take 1-2 hours on a DevCloud machine. 

Note: 

+ We assume your system has python >= 2.7 already installed.
+ The above `install-tools.sh` command installs llvm-clang >= 9.0, gcc >= 7.5.0, and python's numpy and matplotlib package. The command installs all of them and their dependencies we know to make the system self-contained, which saves us from any  dependence problem. If your system has some of the tools already installed, you could edit `install-tools.sh` to disable the installations of these tools.

# Modify the environment setting (once)

```
cd $HOME/t2sp
```

Here you would see a `setenv.sh`. 

+ If you have your own gcc, llvm or clang and did not use the above `install-tools.sh` command to install them, in `setenv.sh`, modify the following path variables appropriately:

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

  Note: here is an example how to find out the board package and board (Assume Intel FPGA SDK for OpenCL 19.1 was installed under directory `$HOME/intelFPGA_pro`):

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

Local: open a bash shell

DevCloud: from the head node, log into a **compute node**:

+ FPGA: 
  ```
    devcloud_login
  ```
    Choose         
    ```
    6) Enter Specific Node Number
    ```
    Enter the name of a node with Arria 10 Release 1.2.1, or with Stratix 10.

+ GPU: to request a compute node with GEN9.5  (Intel UHD Graphics P630) or GEN12 ( Intel Iris Xe MAX Graphics),
  
    ```
    qsub -I -l nodes=1:gen9:ppn=2  
    ```
    
    or 
    
    ```
    qsub -I -l nodes=1:iris_xe_max:ppn=2
    ```

For all the steps below, we assume you are either on a local machine, or on a compute node of DevCloud.

# Set up the environment (whenever a terminal is open)

with one of the following commands, according to if you are working on DevCloud or locally, to use an FPGA or a GPU:

```
source ./setenv.sh devcloud fpga|gpu 
source ./setenv.sh local    fpga|gpu 
```

# Build T2SP (whenever you change the source code)

```
cd Halide
make -j
cd -
```

+ For debugging the T2SP compiler with source code information, ```make -j OPTIMIZE="-O0 -g"``` instead.
+ To debug runtime, ```make -j OPTIMIZE_RUNTIME="-O0 -g" ``` instead.
+ To enable both of them, ```make -j OPTIMIZE="-O0 -g" OPTIMIZE_RUNTIME="-O0 -g" ``` instead.

# Regression tests

```
cd t2s/tests/correctness
./test.sh
```
Each sub-directory there contains a success.txt and failure.txt, which have the command lines for compiling and running every test. These tests are small examples one can play with.

To remove all the temporary files generated during the regression testing:

```
./test.sh clean
```

# Performance tests

```
cd t2s/tests/performance
```
Current release contains SGEMM, 2-D convolution and Capsule convolution on Arria 10 FPGA and GEN9.5 GPU. For every kernel, we write a **single** specification for the different hardwares. This reflects our concept of "**write a kernel once, and run with high performance across spatial and vector architectures**".

Summary of throughput:

|     | A10 | GEN9.5 |
| --- | :-: | :-:    |
| SGEMM |  532 GFLOPS, 95% DSP efficiency  |  423 GFLOPS, 92% machine peak |
| 2-D convolution | 515 GFLOPS, 96% DSP efficiency | 415 GFLOPS, 90% machine peak |
| Capsule convolution | 534 GFLOPS, 98% DSP efficiency | 416 GFLOPS, 90% machine peak |

To reproduce the performance, follow one of the ways below:
+ [On the DevCloud head node], submit job(s):
  
  ```
  # Test all workloads, tiny and large inputs:
  ./devcloud-jobs.sh (a10|gen9)
    
  # Or test individually:
  ./devcloud-job.sh (gemm|conv|capsule) (a10|gen9) (tiny|large) (hw|emulator)
  ```
+ [On a DevCloud compute node], test directly:
  
  ```
  # Test all workloads, tiny and large inputs:
  ./tests.sh devcloud (a10|gen9)
    
  # Or test individually:
  ./test.sh devcloud (gemm|conv|capsule) (a10|gen9) (tiny|large) (hw|emulator)
  ```
+ [On a local machine], test directly:
  
  ```
  # Test all workloads, all hardwares, tiny and large inputs:
  ./tests.sh local (a10|gen9)
    
  # Or test individually:
  ./test.sh local (gemm|conv|capsule) (a10|gen9) (tiny|large) (hw|emulator)
  ```

Note:

     + The emulator option is applicable only to FPGAs and tiny size.
     + Synthesis of each FPGA design will take hours. So on DevCloud, we recommend submitting job(s) for testing on FPGAs.

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

A 10-minute video `intro.mp4`, located at the root of the repository, introduces the basic concept of T2SP.

We have a set of [tutorials](https://github.com/intel/FPGA-Devcloud/tree/master/main/QuickStartGuides/T2S) at Intel DevCloud. A compiler binary is also there, and all dependencies have been installed, so you may start using the programming environment immediately.

# Next release
We aim to release the following in December, 2021:
+ Throughput numbers of SGEMM, 2-D convolution, and Capsule convolution on GEN 12 GPU.
+ Initial throughput numbers for Stratix 10 FPGA. 

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