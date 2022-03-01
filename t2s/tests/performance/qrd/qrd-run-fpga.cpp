// Copyright (C) 2013-2019 Altera Corporation, San Jose, California, USA. All rights reserved.
// Permission is hereby granted, free of charge, to any person obtaining a copy of this
// software and associated documentation files (the "Software"), to deal in the Software
// without restriction, including without limitation the rights to use, copy, modify, merge,
// publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to
// whom the Software is furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.
//
// This agreement shall be governed in all respects by the laws of the State of California and
// by the laws of the United States of America.


// This file is modified from /glob/development-tools/versions/fpgasupportstack/a10/1.2.1/intelFPGA_pro/hld/examples_aoc/matrix_mult/host/src/main.cpp
#include "AOCLUtils/aocl_utils.h"
#include "CL/opencl.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <math.h>
#include <sstream>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/time.h>
#include <time.h>

#define I 128
#define J 128
#define K 128
#define BATCH_SIZE 50

using namespace aocl_utils;

#define TYPE float

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define DPRINTF(...)     \
    printf(__VA_ARGS__); \
    fflush(stdout);

#define NUM_QUEUES_TO_CREATE    4
#define NUM_KERNELS_TO_CREATE   4

#define CHECK(status)                                       \
    if (status != CL_SUCCESS) {                             \
        printf("error %d in line %d.\n", status, __LINE__); \
        exit(1);                                            \
    }

#define ACL_ALIGNMENT 64
void *acl_aligned_malloc(size_t size) {
    void *result = NULL;
    posix_memalign(&result, ACL_ALIGNMENT, size);
    return result;
}

const char *kernel_name[] = {
    "kernel_ALoader",
    "kernel_R",
    "kernel_QUnloader",
    "kernel_RUnloader"    
};

double compute_kernel_execution_time(cl_event &event, double &start_d, double &end_d) {
    cl_ulong start, end;

    clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);
    clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);

    start_d = (double)1.0e-9 * start;
    end_d = (double)1.0e-9 * end;
    //return (double)(end-start);
    return (double)1.0e-9 * (end - start); // nanoseconds to seconds
}


int main() {
    float *A, *Q, *R;
    const int batch_size = BATCH_SIZE;
    long int num_elem_A = (long int)J*K*BATCH_SIZE;
    long int num_elem_Q = (long int)K*I*BATCH_SIZE;
    long int num_elem_R = (long int)J*(I+1)*BATCH_SIZE;
    if ((A = (float *)acl_aligned_malloc(num_elem_A * sizeof(float))) == NULL) {
        perror("Failed malloc of matrix A");
    }
    if ((Q = (float *)acl_aligned_malloc(num_elem_Q * sizeof(float))) == NULL) {
        perror("Failed malloc of matrix A");
    }
    if ((R = (float *)acl_aligned_malloc(J*I*BATCH_SIZE * sizeof(float))) == NULL) {
        perror("Failed malloc of matrix C");
    }
    constexpr size_t kRandomSeed = 1138;
    constexpr size_t kRandomMin = 1;
    constexpr size_t kRandomMax = 10;
    srand(kRandomSeed); 
    for (size_t b = 0; b < BATCH_SIZE; b++) {
        for (size_t k = 0; k < K; k++) {
            for (size_t j = 0; j < J; j++) {
                int random_val = rand();
                float random_float = random_val % (kRandomMax - kRandomMin) + kRandomMin;
                A[(j + k*J + b*J*K)] = random_float;
                // random_val = rand();
                // random_float = random_val % (kRandomMax - kRandomMin) + kRandomMin;
                // A[(j + k*J + b*J*K)*2 + 1] = 0.0f;
            }
        }
    }

    float *serialized_A;
    if ((serialized_A = (float *)acl_aligned_malloc(num_elem_A * sizeof(float))) == NULL) {
        perror("Failed malloc of matrix serialized_A");
    }

    // Serialize A
    long int addr = 0;
    for (size_t b = 0; b < BATCH_SIZE; b++) {
        for (size_t j = 0; j < J; j++) {
            for (size_t k = 0; k < K; k++) {
                serialized_A[addr++] = A[(j + k*J + b*J*K)];
                // serialized_A[addr++] = A[(j + k*J + b*J*K)*2 + 1];
            }
        }
    }

    DPRINTF("\n===== Host-CPU setting up the OpenCL platform and device ======\n\n");

    // Use this to check the output of each API call
    cl_int status;

    //----------------------------------------------
    // Discover and initialize the platforms
    //----------------------------------------------
    cl_uint numPlatforms = 0;
    cl_platform_id platform;
    const char *name = getenv("INTEL_FPGA_OCL_PLATFORM_NAME");
    platform = findPlatform(name);
    if(platform == NULL) {
        DPRINTF("ERROR: Unable to find Intel(R) FPGA OpenCL platform\n");
        return -1;
    }

    //----------------------------------------------
    // Discover and initialize the devices
    //----------------------------------------------
    cl_uint numDevices = 0;
    cl_device_id *devices = NULL;
    // Device info
    char buffer[4096];
    unsigned int buf_uint;
    int device_found = 0;
    printf("Initializing IDs\n");
    status = clGetDeviceIDs(platform,
                        CL_DEVICE_TYPE_ALL,
                        0,
                        NULL,
                        &numDevices);

    if(status == CL_SUCCESS){
        clGetPlatformInfo(platform,
                          CL_PLATFORM_VENDOR,
                          4096,
                          buffer,
                          NULL);

        if(strstr(buffer, "Intel(R)") != NULL){
            device_found = 1;
        }
        printf("%s\n", buffer);

        if(device_found){
            // Allocate enough space for each device
            devices = (cl_device_id*)
            acl_aligned_malloc (numDevices * sizeof(cl_device_id));

            // Fill in devices with clGetDeviceIDs()
            status = clGetDeviceIDs(platform,
                                    CL_DEVICE_TYPE_ALL,
                                    numDevices,
                                    devices,
                                    NULL);
        }
    }

    if (!device_found) {
        DPRINTF("failed to find a OpenCL device\n");
        exit(-1);
    }

    DPRINTF("Total number of devices: %d", numDevices);
    for (int i = 0; i < numDevices; i++) {
        clGetDeviceInfo(devices[i],
                        CL_DEVICE_NAME,
                        4096,
                        buffer,
                        NULL);
        DPRINTF("\nDevice Name: %s\n", buffer);

        clGetDeviceInfo(devices[i],
                        CL_DEVICE_VENDOR,
                        4096,
                        buffer,
                        NULL);
        DPRINTF("Device Vendor: %s\n", buffer);

        clGetDeviceInfo(devices[i],
                        CL_DEVICE_MAX_COMPUTE_UNITS,
                        sizeof(buf_uint),
                        &buf_uint,
                        NULL);
        DPRINTF("Device Computing Units: %u\n", buf_uint);

        clGetDeviceInfo(devices[i],
                        CL_DEVICE_GLOBAL_MEM_SIZE,
                        sizeof(unsigned long),
                        &buffer,
                        NULL);
        //DPRINTF("Global Memory Size: %i\n", *((unsigned long*)buffer));

        clGetDeviceInfo(devices[i],
                        CL_DEVICE_MAX_MEM_ALLOC_SIZE,
                        sizeof(unsigned long),
                        &buffer,
                        NULL);
        //DPRINTF("Global Memory Allocation Size: %i\n\n", *((unsigned long*)buffer));
    }

    //----------------------------------------------
    // Create a context
    //----------------------------------------------

    DPRINTF("\n===== Host-CPU setting up the OpenCL command queues ======\n\n");

    cl_context context = NULL;

    // Create a context using clCreateContext() and
    // associate it with the device

    context = clCreateContext(
        NULL,
        1,
        devices,
        NULL,
        NULL,
        &status);
    CHECK(status);

    //----------------------------------------------
    // Create command queues
    //---------------------------------------------

    cl_command_queue cmdQueue[NUM_QUEUES_TO_CREATE + 1]; // extra queue for reading buffer D

    // Create a command queue using clCreateCommandQueue(),
    // and associate it with the device you want to execute on
    for (int i = 0; i < NUM_QUEUES_TO_CREATE; i++) {
        // fDPRINTF(stdout,"cmdQueue i = %d\n", i);
        cmdQueue[i] = clCreateCommandQueue(
            context,
            devices[0],
            CL_QUEUE_PROFILING_ENABLE,
            &status);
        CHECK(status);
    }

    // fDPRINTF(stdout,"cmdQueue i = %d, a queue for reading the C buffer\n", i);
    cmdQueue[NUM_QUEUES_TO_CREATE] = clCreateCommandQueue(
        context,
        devices[0],
        CL_QUEUE_PROFILING_ENABLE,
        &status);
    CHECK(status);

    //----------------------------------------------
    // Create device buffers
    //----------------------------------------------

    cl_mem input_A_buf;
    cl_mem output_Q_buf;
    cl_mem output_R_buf;

    DPRINTF("\n===== Host-CPU transferring matrix A to the FPGA device global memory (DDR4) via PCIe ======\n\n");
    input_A_buf = clCreateBuffer(
        context,
        //CL_MEM_READ_ONLY | CL_MEM_BANK_1_ALTERA,
        CL_MEM_READ_ONLY,
        num_elem_A * sizeof(cl_float),
        NULL,
        &status);
    CHECK(status);

    output_Q_buf = clCreateBuffer(
        context,
        //CL_MEM_READ_ONLY | CL_MEM_BANK_1_ALTERA,
        CL_MEM_WRITE_ONLY,
        num_elem_Q * sizeof(cl_float),
        NULL,
        &status);
    CHECK(status);

    output_R_buf = clCreateBuffer(
        context,
        //CL_MEM_WRITE_ONLY | CL_MEM_BANK_1_ALTERA,
        CL_MEM_WRITE_ONLY,
        num_elem_R * sizeof(cl_float),
        NULL,
        &status);
    CHECK(status);

    //----------------------------------------------
    // Write host data to device buffers
    //----------------------------------------------

    // blocking writes
    status = clEnqueueWriteBuffer(
        cmdQueue[0],
        input_A_buf,
        CL_TRUE,
        0,
        num_elem_A * sizeof(cl_float),
        serialized_A,
        0,
        NULL,
        NULL);
    CHECK(status);

    //----------------------------------------------
    // Create the program from binaries
    //----------------------------------------------
    DPRINTF("\n===== Host-CPU setting up OpenCL program and kernels ======\n\n");

    cl_program program;

    size_t binary_length;
    const unsigned char *binary;

    fflush(stdout);
    // create the program using binary already compiled offline using aoc (i.e. the .aocx file)
    char *aocx_file = getenv("BITSTREAM");
    FILE *fp = fopen(aocx_file, "rb");

    if (fp == NULL) {
        DPRINTF("Failed to open the AOCX file (fopen).\n");
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    binary_length = ftell(fp);
    binary = (unsigned char *)malloc(sizeof(unsigned char) * binary_length);
    assert(binary && "Malloc failed");
    rewind(fp);

    if (fread((void *)binary, binary_length, 1, fp) == 0) {
        DPRINTF("Failed to read from the AOCX file (fread).\n");
        return -1;
    }
    fclose(fp);

    DPRINTF("Create program with binary\n");
    // Create a program using clCreateProgramWithBinary()
    program = clCreateProgramWithBinary(
        context,
        1,
        devices,
        &binary_length,
        (const unsigned char **)&binary,
        &status,
        NULL);
    CHECK(status);

    //----------------------------------------------
    // Create the kernel
    //----------------------------------------------

    status = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
    if (status != CL_SUCCESS) {
        char log[128 * 1024] = {0};
        clGetProgramBuildInfo(program, devices[0], CL_PROGRAM_BUILD_LOG, 128 * 1024, log, NULL);
        DPRINTF("%s\n", log);
        CHECK(status);
    }

    cl_kernel kernel[NUM_KERNELS_TO_CREATE];

    for (int j = 0; j < NUM_KERNELS_TO_CREATE; j++) {
        DPRINTF("Creating kernel[%d]: %s\n", j, kernel_name[j]);
        kernel[j] = clCreateKernel(program, (const char *)kernel_name[j], &status);
        CHECK(status);
    }
    DPRINTF("All kernels created\n");

    // ALoader
    status = clSetKernelArg(
        kernel[0],
        0,
        sizeof(int),
        (void *)&batch_size);
    CHECK(status);
    status = clSetKernelArg(
        kernel[0],
        1,
        sizeof(cl_mem),
        (void *)&input_A_buf);
    CHECK(status);

    // QR
    status = clSetKernelArg(
        kernel[1],
        0,
        sizeof(int),
        (void *)&batch_size);
    CHECK(status);

    // QUnloader
    status = clSetKernelArg(
        kernel[2],
        0,
        sizeof(int),
        (void *)&batch_size);
    CHECK(status);
    status = clSetKernelArg(
        kernel[2],
        1,
        sizeof(cl_mem),
        (void *)&output_Q_buf);
    CHECK(status);
    // RUnloader
    status = clSetKernelArg(
        kernel[3],
        0,
        sizeof(int),
        (void *)&batch_size);
    CHECK(status);
    status = clSetKernelArg(
        kernel[3],
        1,
        sizeof(cl_mem),
        (void *)&output_R_buf);
    CHECK(status);


    //----------------------------------------------
    // Configure the work-item structure (using only tasks atm)
    //----------------------------------------------

    // Define the number of threads that will be created
    // as well as the number of work groups
    size_t globalWorkSize[1];
    size_t localWorkSize[1];

    //----------------------------------------------
    // Enqueue the kernel for execution
    //----------------------------------------------

    // all kernels are always tasks
    globalWorkSize[0] = 1;
    localWorkSize[0] = 1;

    cl_event kernel_exec_event[NUM_KERNELS_TO_CREATE];

    DPRINTF("\n===== Host-CPU enqeuing the OpenCL kernels to the FPGA device ======\n\n");
    for (int i = 0; i < NUM_KERNELS_TO_CREATE; i++) {
        // Alternatively, can use clEnqueueTaskKernel
        DPRINTF("clEnqueueNDRangeKernel[%d]: %s!\n", i, kernel_name[i]);
        status = clEnqueueNDRangeKernel(
            cmdQueue[i],
            kernel[i],
            1,
            NULL,
            globalWorkSize,
            localWorkSize,
            0,
            NULL,
            &kernel_exec_event[i]);
        CHECK(status);
    }
    DPRINTF(" *** FPGA execution started!\n");

    for (int i = 0; i < NUM_KERNELS_TO_CREATE; i++) {
        status = clFlush(cmdQueue[i]);
        CHECK(status);
    }
    DPRINTF(" *** FPGA execution flushed!\n");

    for (int i = 0; i < NUM_QUEUES_TO_CREATE; i++) {
        fflush(stdout);
        status = clFinish(cmdQueue[i]);
        DPRINTF("cmd queue finish: %d\n", i);
        CHECK(status);
    }
    DPRINTF(" *** FPGA execution finished!\n");
    DPRINTF("\n\n");

    double k_start_time[NUM_KERNELS_TO_CREATE];
    double k_end_time[NUM_KERNELS_TO_CREATE];
    double k_exec_time[NUM_KERNELS_TO_CREATE];
    double max_time = 0;
    for (int i = 0; i < NUM_KERNELS_TO_CREATE; i++) {
        k_exec_time[i] = compute_kernel_execution_time(kernel_exec_event[i], k_start_time[i], k_end_time[i]);
        if (k_exec_time[i] > max_time) {
            max_time = k_exec_time[i];
        }
    }
    DPRINTF("Time taken: %lf sec\n\n", max_time);

    printf("\n===== Reporting measured throughput ======\n\n");
    double k_earliest_start_time = k_start_time[0];
    double k_latest_end_time = k_end_time[0];

    for (int i = 1; i < NUM_KERNELS_TO_CREATE; i++) {
        if (k_start_time[i] < k_earliest_start_time)
            k_earliest_start_time = k_start_time[i];

        if (k_end_time[i] > k_latest_end_time)
            k_latest_end_time = k_end_time[i];
    }

    k_latest_end_time = k_end_time[NUM_KERNELS_TO_CREATE - 1];

    for (int i = 0; i < NUM_KERNELS_TO_CREATE; i++) {
        printf("  Kernel execution time on FPGA: %s, \n   \t\t\t\t\t\t\t\t\texec time = %.5f s, start=%.5f s, end=%.5f s\n", kernel_name[i], k_exec_time[i], k_start_time[i], k_end_time[i]);
    }

    double k_overall_exec_time = k_latest_end_time - k_earliest_start_time;

    printf("\n");
    printf("  Loader kernels start time\t\t= %.5f s\n", k_earliest_start_time);
    printf("  Unloader kernels end time\t\t= %.5f s\n", k_latest_end_time);
    printf("  FPGA QR exec time\t\t= %.5f s\n", k_overall_exec_time);

    printf("\n");

    printf("  Throughput: %.5f matrices/s for matrices of size %d*%d\n", (double)BATCH_SIZE / k_overall_exec_time, K, J);

    DPRINTF("\n===== Host-CPU transferring result matrix C from the FPGA device global memory (DDR4) via PCIe ======\n\n");

    // Read the results back from the device, blocking read
    float *serialized_Q, *serialized_R;
    if ((serialized_Q = (float *)acl_aligned_malloc(num_elem_Q * sizeof(float))) == NULL) {
        perror("Failed malloc of matrix serialized_Z");
    }
    if ((serialized_R = (float *)acl_aligned_malloc(num_elem_R * sizeof(float))) == NULL) {
        perror("Failed malloc of matrix serialized_Z");
    }

    clEnqueueReadBuffer(
        //cmdQueue[KID_DRAIN_MAT_C],
        cmdQueue[NUM_KERNELS_TO_CREATE], // using a special queue for reading buffer R
        output_R_buf,
        CL_TRUE,
        0,
        num_elem_R * sizeof(cl_float),
        serialized_R,
        0,
        NULL,
        NULL);
    CHECK(status);

    clEnqueueReadBuffer(
        //cmdQueue[KID_DRAIN_MAT_C],
        cmdQueue[NUM_KERNELS_TO_CREATE], // using a special queue for reading buffer Q
        output_Q_buf,
        CL_TRUE,
        0,
        num_elem_Q * sizeof(cl_float),
        serialized_Q,
        0,
        NULL,
        NULL);
    CHECK(status);

    // Deserialize Q
    addr = 0;
    for (size_t b = 0; b < BATCH_SIZE; b++) {
        for (size_t i = 0; i < I; i++) {
            for (size_t k = 0; k < K; k++) {
                Q[(k + i*K + b*I*K)] = serialized_Q[addr++];
                // Q[(k + i*K + b*I*K)*2 + 1] = serialized_Q[addr++];
            }
        }
    }

    // Deserialize R
    addr = 0;
    for (size_t b = 0; b < BATCH_SIZE; b++) {
        for (size_t i = 0; i < I; i++) {
            for (size_t j = 0; j < J; j++) {
                // printf("%5.2f", serialized_R[addr]);
                if (j >= i) {
                    R[(j + i*J + b*I*J)] = serialized_R[addr++];
                    // R[(j + i*J + b*I*J)*2 + 1] = serialized_R[addr++];
                } else {
                    R[(j + i*J + b*I*J)] = 0.0f;
                    // R[(j + i*J + b*I*J)*2 + 1] = 0.0f;
                }
                
            }
        }
    }


    bool passed = 1;

    std::list<size_t> to_check;
    // We will check at least matrix 0
    to_check.push_back(0);
    // Spot check the last and the middle one
    if (BATCH_SIZE > 2) to_check.push_back(BATCH_SIZE / 2);
    if (BATCH_SIZE > 1) to_check.push_back(BATCH_SIZE - 1);

    for (size_t b : to_check) {
        printf("\n*** Verifying results on input matrix %ld\n", b);
        printf("*** Matrix Q: \n");
        for (int k = 0; k < K; k++) {
            for (int i = 0; i < I; i++) {
                printf("%5.2f ", Q[(k + i*K + b*K*I)]);
            }
            printf("\n");
        }

        printf("*** Matrix R: \n");
        for (int i = 0; i < I; i++) {
            for (int j = 0; j < J; j++) {
                printf("%5.2f ", R[(j + i*J + b*I*J)]);
            }
            printf("\n");
        }

        // check if Q * R can reproduce the inputs
        printf("*** Q * R [Input]\n");
        for (int k = 0; k < K; k++) {
            for (int j = 0; j < J; j++) {
                float golden = 0.0f;
                for (int i = 0; i < I; i++) {
                    golden += Q[(k + i*K + b*K*I)] * R[(j + i*J + b*I*J)];
                }
                bool correct = fabs(A[(j + k*J + b*J*K)] - golden) < 0.005;
                passed = passed && correct;
                printf("%5.2f [%5.2f%s] ", golden,  A[(j + k*J + b*J*K)], correct ? "" : " !!");
            }
            printf("\n");
        }
    }

    if (passed) {
        printf("[PASSED]\n");
    } else {
        printf("[FAILED]\n");
    }
}

// Free the resources allocated during initialization
void cleanup() {
    /*  for(unsigned i = 0; i < num_devices; ++i) {
    if(kernel && kernel[i]) {
      clReleaseKernel(kernel[i]);
    }
    if(queue && queue[i]) {
      clReleaseCommandQueue(queue[i]);
    }
#if USE_SVM_API == 0
    if(input_a_buf && input_a_buf[i]) {
      clReleaseMemObject(input_a_buf[i]);
    }
    if(input_b_buf && input_b_buf[i]) {
      clReleaseMemObject(input_b_buf[i]);
    }
    if(output_buf && output_buf[i]) {
      clReleaseMemObject(output_buf[i]);
    }
#else
    if(input_a[i].get())
      input_a[i].reset();
    if(input_b[i].get())
      input_b[i].reset();
    if(output[i].get())
      output[i].reset();
#endif // USE_SVM_API == 0
  }

  if(program) {
    clReleaseProgram(program);
  }
  if(context) {
    clReleaseContext(context);
  }*/
}
