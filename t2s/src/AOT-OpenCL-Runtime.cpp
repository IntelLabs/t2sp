/*******************************************************************************
* Copyright 2021 Intel Corporation
*
* Licensed under the BSD-2-Clause Plus Patent License (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* https://opensource.org/licenses/BSDplusPatent
*
* Unless required by applicable law or agreed to in writing,
* software distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions
* and limitations under the License.
*
*
* SPDX-License-Identifier: BSD-2-Clause-Patent
*******************************************************************************/
#include "AOT-OpenCL-Runtime.h"
#include "SharedUtilsInC.h"

#define WEAK __attribute__((weak))
#define ACL_ALIGNMENT 64

extern int MAX_DEVICES;
extern int NUM_QUEUES_TO_CREATE;
extern int NUM_KERNELS_TO_CREATE;
extern cl_int status;
extern cl_context context;
extern cl_command_queue cmdQueue[]; // extra queue for reading buffer D
extern cl_device_id devices[];
extern int current_kernel;
extern cl_kernel kernel[];
extern const char *kernel_name[];

using namespace aocl_utils;

void cleanup() {
}

void* acl_aligned_malloc (size_t size) {
    void *result = NULL;
    if (posix_memalign(&result, ACL_ALIGNMENT, size) != 0)
        printf("acl_aligned_malloc() failed.\n");
    return result;
}

void acl_aligned_free (void *ptr) {
    free (ptr);
}

#ifdef __cplusplus
extern "C" {
#endif

WEAK void *halide_malloc(void *user_context, size_t sz) {
    void *result = NULL;
    posix_memalign(&result, ACL_ALIGNMENT, sz);
    return result;
}

WEAK void halide_free(void *user_context, void *ptr) {
    free(ptr);
}

WEAK void halide_error(void *user_context, const char *msg) {
    printf("%s\n", msg);
}

WEAK const struct halide_device_interface_t *halide_opencl_device_interface() {
    return NULL;
}

WEAK int halide_device_malloc(void *user_context, struct halide_buffer_t *buf,
                                    const halide_device_interface_t *interface) {
    size_t size = buf->size_in_bytes();
    assert(size != 0);
    // Check size is within limit. Not calling buf->size_in_bytes() since when the size is over the range of size_t,
    // that function would return 0 instead of the true size.
    uint64_t lowest_index = 0;
    uint64_t highest_index = 0;
    for (int i = 0; i < buf->dimensions; i++) {
        if (buf->dim[i].stride < 0) {
            lowest_index += (uint64_t)(buf->dim[i].stride) * (buf->dim[i].extent - 1);
        }
        if (buf->dim[i].stride > 0) {
            highest_index += (uint64_t)(buf->dim[i].stride) * (buf->dim[i].extent - 1);
        }
    }
    uint64_t total_bytes = (highest_index + 1  - lowest_index) * buf->type.bytes();
    if (total_bytes > (static_cast<uint64_t>(1) << 32) - 1) {
        std::cout << "CL: halide_opencl_device_malloc failed: "
                  << total_bytes << " bytes are requested to allocate on the device. The size exceeds 2^32 - 1.\n";
        assert(false);
    }

    if (buf->device) {
        return 0;
    }

    for (int i = 0; i < buf->dimensions; i++) {
        assert(buf->dim[i].stride >= 0);
    }

    device_handle *dev_handle = (device_handle *)malloc(sizeof(device_handle));
    if (dev_handle == NULL) {
        return CL_OUT_OF_HOST_MEMORY;
    }

    cl_mem dev_ptr = clCreateBuffer(context, CL_MEM_READ_WRITE, size, NULL, &status);
    CHECK(status);
    dev_handle->mem = dev_ptr;
    dev_handle->offset = 0;
    buf->device = (uint64_t)dev_handle;

    return 0;
}

WEAK int32_t halide_device_and_host_malloc(void *user_context, struct halide_buffer_t *buf,
                                       const halide_device_interface_t *device_interface) {
    size_t size = buf->size_in_bytes();
    assert(size != 0);
    buf->host = (uint8_t *)halide_malloc(user_context, size);
    if (buf->host == NULL) {
        return CL_OUT_OF_HOST_MEMORY;
    }

    if (context == NULL) {
        cl_uint numPlatforms = 0;
        cl_platform_id platform;

        const char *name = getenv("INTEL_FPGA_OCL_PLATFORM_NAME");
        platform = findPlatform(name);
        if(platform == NULL) {
            DPRINTF("ERROR: Unable to find Intel(R) FPGA OpenCL platform\n");
            return -1;
        }

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

        DPRINTF("Total number of devices: %d\n", numDevices);

        context = clCreateContext(
            NULL,
            1,
            devices,
            NULL,
            NULL,
            &status);
        CHECK(status);

        // Create a command queue using clCreateCommandQueue(),
        // and associate it with the device you want to execute on
        for (int i = 0; i < NUM_QUEUES_TO_CREATE; i++) {
            //fDPRINTF(stdout,"cmdQueue i = %d\n", i);
            cmdQueue[i] = clCreateCommandQueue(
                context,
                devices[0],
                CL_QUEUE_PROFILING_ENABLE,
                &status);
            CHECK(status);
        }

        //fDPRINTF(stdout,"cmdQueue i = %d, a queue for reading the C buffer\n", i);
        cmdQueue[NUM_QUEUES_TO_CREATE] = clCreateCommandQueue(
            context,
            devices[0],
            CL_QUEUE_PROFILING_ENABLE,
            &status);
        CHECK(status);

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

        for (int j = 0; j < NUM_KERNELS_TO_CREATE; j++) {
            DPRINTF("Creating kernel[%d]: %s\n", j, kernel_name[j]);
            kernel[j] = clCreateKernel(program, (const char *)kernel_name[j], &status);
            CHECK(status);
        }
        DPRINTF("All kernels created\n");
    }

    return halide_device_malloc(user_context, buf, device_interface);
}

/** Free host and device memory associated with a buffer_t. */
WEAK int32_t halide_device_and_host_free(void *user_context, void *obj) {
    struct halide_buffer_t *buf = (struct halide_buffer_t *)obj;
    cl_mem dev_ptr = ((device_handle *)buf->device)->mem;
    assert(((device_handle *)buf->device)->offset == 0);
    cl_int result = clReleaseMemObject((cl_mem)dev_ptr);
    free((device_handle *)buf->device);
    buf->device = 0;

    if (buf->host) {
        halide_free(user_context, buf->host);
        buf->host = NULL;
    }
    buf->set_host_dirty(false);
    buf->set_device_dirty(false);
    return result;
}

WEAK void halide_device_and_host_free_as_destructor(void *user_context, void *obj) {
}

// Return execution time in nanoseconds, as well as the start and end time in nanoseconds
double compute_kernel_execution_time(cl_event &event, double &start_d, double &end_d) {
    cl_ulong start, end;

    clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);
    clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);

    start_d = (double)start;
    end_d = (double)end;
    return (double)(end-start);
}

WEAK int32_t halide_opencl_wait_for_kernels_finish(void *user_context) {
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
    DPRINTF("\n");
    DPRINTF(" *** FPGA execution started!\n");
    for (int i = 0; i < NUM_KERNELS_TO_CREATE; i++) {
        status = clFlush(cmdQueue[i]);
        CHECK(status);
    }

    for (int i = 0; i < NUM_QUEUES_TO_CREATE; i++) {
        DPRINTF("cmd queue: %d\n", i);
        fflush(stdout);
        status = clFinish(cmdQueue[i]);
        CHECK(status);
    }
    DPRINTF(" *** FPGA execution finished!\n");
    DPRINTF("\n");

    double k_start_time[NUM_KERNELS_TO_CREATE];
    double k_end_time[NUM_KERNELS_TO_CREATE];
    double k_exec_time[NUM_KERNELS_TO_CREATE];
    for (int i = 0; i < NUM_KERNELS_TO_CREATE; i++) {
        k_exec_time[i] = compute_kernel_execution_time(kernel_exec_event[i], k_start_time[i], k_end_time[i]);
    }

    double k_earliest_start_time = k_start_time[0];
    double k_latest_end_time = k_end_time[0];
    for (int i = 1; i < NUM_KERNELS_TO_CREATE; i++) {
        if (k_start_time[i] < k_earliest_start_time) {
            k_earliest_start_time = k_start_time[i];
        }
        if (k_end_time[i] > k_latest_end_time) {
            k_latest_end_time = k_end_time[i];
        }
    }

    double k_overall_exec_time = k_latest_end_time - k_earliest_start_time;

    char *bitstream_dir = bitstream_directory();
    char *exec_time_file = concat_directory_and_file(bitstream_dir, "exec_time.txt");
    FILE *fp = fopen(exec_time_file, "w");
    if (fp == NULL) {
        DPRINTF("Failed to open %s for writing.\n", exec_time_file);
        free(bitstream_dir);
        free(exec_time_file);
        return -1;
    }
    fprintf(fp, "%f\n", k_overall_exec_time);
    for (int i = 0; i < NUM_KERNELS_TO_CREATE; i++) {
        fprintf(fp, "%s %lf\n", kernel_name[i], k_exec_time[i]);
    }
    fclose(fp);
    free(bitstream_dir);
    free(exec_time_file);
    return 0;
}

WEAK void halide_device_host_nop_free(void *user_context, void *obj) {
}

WEAK int halide_opencl_buffer_copy(void *user_context, struct halide_buffer_t *src,
                                   struct halide_buffer_t *dst, bool to_host) {
    bool from_host = (src->device == 0) ||
                     (src->host_dirty() && src->host != NULL);
    if (!from_host && to_host) {
        std::cout << "Command queue " << current_kernel << ": copying " << src->size_in_bytes() << " bytes data from device to host. ";
        status = clEnqueueReadBuffer(cmdQueue[current_kernel], ((device_handle *)src->device)->mem,
                                     CL_TRUE, 0, src->size_in_bytes(), (void *)(dst->host),
                                     0, NULL, NULL);
        std::cout << "Done.\n";
    } else if (from_host && !to_host) {
        std::cout << "Command queue " << current_kernel << ": copying " << src->size_in_bytes() << " bytes data from host to device. ";
        status = clEnqueueWriteBuffer(cmdQueue[current_kernel], ((device_handle *)dst->device)->mem,
                                      CL_TRUE, 0, src->size_in_bytes(), (void *)(src->host),
                                      0, NULL, NULL);
        std::cout << "Done.\n";
    } else if (!from_host && !to_host) {
        std::cout << "Command queue " << current_kernel << ": copying " << src->size_in_bytes() << " bytes data from device to device. ";
        status = clEnqueueCopyBuffer(cmdQueue[current_kernel], ((device_handle *)src->device)->mem, ((device_handle *)dst->device)->mem,
                                     0, 0,
                                     src->size_in_bytes(), 0, NULL, NULL);
        std::cout << "Done.\n";
    } else if (dst->host != src->host) {
        std::cout << "Copying " << src->size_in_bytes() << " bytes data from host to host. ";
            memcpy((void *)(dst->host), (void *)(src->host), src->size_in_bytes());
        std::cout << "Done.\n";
    } else {
        std::cout << "halide_opencl_buffer_copy: host to host copy with source address equal to destination address. Do nothing. \n";
    }
    return 0;
}

WEAK int halide_copy_to_device(void *user_context, struct halide_buffer_t *buf,
                               const struct halide_device_interface_t *device_interface) {
    return halide_opencl_buffer_copy(user_context, buf, buf, false);
}

WEAK int halide_copy_to_host(void *user_context, struct halide_buffer_t* buf) {
    return halide_opencl_buffer_copy(user_context, buf, buf, true);
}

WEAK int halide_error_bounds_inference_call_failed(void *user_context, const char *extern_stage_name, int result) {
    std::cout
        << "Bounds inference call to external stage " << extern_stage_name
        << " returned non-zero value: " << result;
    return result;
}

WEAK int halide_error_extern_stage_failed(void *user_context, const char *extern_stage_name, int result) {
    std::cout
        << "Call to external stage " << extern_stage_name
        << " returned non-zero value: " << result;
    return result;
}

WEAK int halide_error_explicit_bounds_too_small(void *user_context, const char *func_name, const char *var_name,
                                                int min_bound, int max_bound, int min_required, int max_required) {
    std::cout
        << "Bounds given for " << var_name << " in " << func_name
        << " (from " << min_bound << " to " << max_bound
        << ") do not cover required region (from " << min_required
        << " to " << max_required << ")";
    return halide_error_code_explicit_bounds_too_small;
}

WEAK int halide_error_bad_type(void *user_context, const char *func_name,
                               uint32_t type_given_bits, uint32_t correct_type_bits) {
    halide_type_t correct_type, type_given;
    memcpy(&correct_type, &correct_type_bits, sizeof(uint32_t));
    memcpy(&type_given, &type_given_bits, sizeof(uint32_t));
    std::cout
        << func_name << " has type " << correct_type.code
        << " but type of the buffer passed in is " << type_given.code;
    return halide_error_code_bad_type;
}

WEAK int halide_error_bad_dimensions(void *user_context, const char *func_name,
                                     int32_t dimensions_given, int32_t correct_dimensions) {
    std::cout
        << func_name << " requires a buffer of exactly " << correct_dimensions
        << " dimensions, but the buffer passed in has " << dimensions_given << " dimensions";
    return halide_error_code_bad_dimensions;
}

WEAK int halide_error_access_out_of_bounds(void *user_context, const char *func_name,
                                           int dimension, int min_touched, int max_touched,
                                           int min_valid, int max_valid) {
    if (min_touched < min_valid) {
        std::cout
            << func_name << " is accessed at " << min_touched
            << ", which is before the min (" << min_valid
            << ") in dimension " << dimension;
    } else if (max_touched > max_valid) {
        std::cout
            << func_name << " is accessed at " << max_touched
            << ", which is beyond the max (" << max_valid
            << ") in dimension " << dimension;
    }
    return halide_error_code_access_out_of_bounds;
}

WEAK int halide_error_buffer_allocation_too_large(void *user_context, const char *buffer_name, uint64_t allocation_size, uint64_t max_size) {
    std::cout
        << "Total allocation for buffer " << buffer_name
        << " is " << allocation_size
        << ", which exceeds the maximum size of " << max_size;
    return halide_error_code_buffer_allocation_too_large;
}

WEAK int halide_error_buffer_extents_negative(void *user_context, const char *buffer_name, int dimension, int extent) {
    std::cout
        << "The extents for buffer " << buffer_name
        << " dimension " << dimension
        << " is negative (" << extent << ")";
    return halide_error_code_buffer_extents_negative;
}

WEAK int halide_error_buffer_extents_too_large(void *user_context, const char *buffer_name, int64_t actual_size, int64_t max_size) {
    std::cout
        << "Product of extents for buffer " << buffer_name
        << " is " << actual_size
        << ", which exceeds the maximum size of " << max_size;
    return halide_error_code_buffer_extents_too_large;
}

WEAK int halide_error_constraints_make_required_region_smaller(void *user_context, const char *buffer_name,
                                                               int dimension,
                                                               int constrained_min, int constrained_extent,
                                                               int required_min, int required_extent) {
    int required_max = required_min + required_extent - 1;
    int constrained_max = constrained_min + constrained_extent - 1;
    std::cout
        << "Applying the constraints on " << buffer_name
        << " to the required region made it smaller in dimension " << dimension << ". "
        << "Required size: " << required_min << " to " << required_max << ". "
        << "Constrained size: " << constrained_min << " to " << constrained_max << ".";
    return halide_error_code_constraints_make_required_region_smaller;
}

WEAK int halide_error_constraint_violated(void *user_context, const char *var, int val,
                                          const char *constrained_var, int constrained_val) {
    std::cout
        << "Constraint violated: " << var << " (" << val
        << ") == " << constrained_var << " (" << constrained_val << ")";
    return halide_error_code_constraint_violated;
}

WEAK int halide_error_param_too_small_i64(void *user_context, const char *param_name,
                                          int64_t val, int64_t min_val) {
    std::cout
        << "Parameter " << param_name
        << " is " << val
        << " but must be at least " << min_val;
    return halide_error_code_param_too_small;
}

WEAK int halide_error_param_too_small_u64(void *user_context, const char *param_name,
                                          uint64_t val, uint64_t min_val) {
    std::cout
        << "Parameter " << param_name
        << " is " << val
        << " but must be at least " << min_val;
    return halide_error_code_param_too_small;
}

WEAK int halide_error_param_too_small_f64(void *user_context, const char *param_name,
                                          double val, double min_val) {
    std::cout
        << "Parameter " << param_name
        << " is " << val
        << " but must be at least " << min_val;
    return halide_error_code_param_too_small;
}

WEAK int halide_error_param_too_large_i64(void *user_context, const char *param_name,
                                          int64_t val, int64_t max_val) {
    std::cout
        << "Parameter " << param_name
        << " is " << val
        << " but must be at most " << max_val;
    return halide_error_code_param_too_large;
}

WEAK int halide_error_param_too_large_u64(void *user_context, const char *param_name,
                                          uint64_t val, uint64_t max_val) {
    std::cout
        << "Parameter " << param_name
        << " is " << val
        << " but must be at most " << max_val;
    return halide_error_code_param_too_large;
}

WEAK int halide_error_param_too_large_f64(void *user_context, const char *param_name,
                                          double val, double max_val) {
    std::cout
        << "Parameter " << param_name
        << " is " << val
        << " but must be at most " << max_val;
    return halide_error_code_param_too_large;
}

WEAK int halide_error_out_of_memory(void *user_context) {
    // The error message builder uses malloc, so we can't use it here.
    halide_error(user_context, "Out of memory (halide_malloc returned NULL)");
    return halide_error_code_out_of_memory;
}

WEAK int halide_error_buffer_argument_is_null(void *user_context, const char *buffer_name) {
    std::cout
        << "Buffer argument " << buffer_name << " is NULL";
    return halide_error_code_buffer_argument_is_null;
}

WEAK int halide_error_debug_to_file_failed(void *user_context, const char *func,
                                           const char *filename, int error_code) {
    std::cout
        << "Failed to dump function " << func
        << " to file " << filename
        << " with error " << error_code;
    return halide_error_code_debug_to_file_failed;
}

WEAK int halide_error_failed_to_upgrade_buffer_t(void *user_context,
                                                 const char *name,
                                                 const char *reason) {
    std::cout
        << "Failed to upgrade buffer_t to halide_buffer_t for " << name << ": " << reason;
    return halide_error_code_failed_to_upgrade_buffer_t;
}

WEAK int halide_error_failed_to_downgrade_buffer_t(void *user_context,
                                                 const char *name,
                                                 const char *reason) {
    std::cout
        << "Failed to downgrade halide_buffer_t to buffer_t for " << name << ": " << reason;
    return halide_error_code_failed_to_downgrade_buffer_t;
}

WEAK int halide_error_unaligned_host_ptr(void *user_context, const char *func,
                                         int alignment) {
    std::cout
        << "The host pointer of " << func
        << " is not aligned to a " << alignment
        << " bytes boundary.";
    return halide_error_code_unaligned_host_ptr;
}

WEAK int halide_error_host_is_null(void *user_context, const char *func) {
    std::cout
        << "The host pointer of " << func
        << " is null, but the pipeline will access it on the host.";
    return halide_error_code_host_is_null;
}

WEAK int halide_error_bad_fold(void *user_context, const char *func_name, const char *var_name,
                               const char *loop_name) {
    std::cout
        << "The folded storage dimension " << var_name << " of " << func_name
        << " was accessed out of order by loop " << loop_name << ".";
    return halide_error_code_bad_fold;
}

WEAK int halide_error_bad_extern_fold(void *user_context, const char *func_name,
                                      int dim, int min, int extent, int valid_min, int fold_factor) {
    if (min < valid_min || min + extent > valid_min + fold_factor) {
        std::cout
            << "Cannot fold dimension " << dim << " of " << func_name
            << " because an extern stage accesses [" << min << ", " << (min + extent - 1) << "],"
            << " which is outside the range currently valid: ["
            << valid_min << ", " << (valid_min + fold_factor - 1) << "].";
    } else {
        std::cout
            << "Cannot fold dimension " << dim << " of " << func_name
            << " because an extern stage accesses [" << min << ", " << (min + extent - 1) << "],"
            << " which wraps around the boundary of the fold, "
            << "which occurs at multiples of " << fold_factor << ".";
    }
    return halide_error_code_bad_extern_fold;
}

WEAK int halide_error_fold_factor_too_small(void *user_context, const char *func_name, const char *var_name,
                                            int fold_factor, const char *loop_name, int required_extent) {
    std::cout
        << "The fold factor (" << fold_factor
        << ") of dimension " << var_name << " of " << func_name
        << " is too small to store the required region accessed by loop "
        << loop_name << " (" << required_extent << ").";
    return halide_error_code_fold_factor_too_small;
}

WEAK int halide_error_requirement_failed(void *user_context, const char *condition, const char *message) {
    std::cout
        << "Requirement Failed: (" << condition << ") " << message;
    return halide_error_code_requirement_failed;
}

WEAK int halide_error_specialize_fail(void *user_context, const char *message) {
    std::cout
        << "A schedule specialized with specialize_fail() was chosen: " << message;
    return halide_error_code_specialize_fail;
}

WEAK int halide_error_no_device_interface(void *user_context) {
    std::cout << "Buffer has a non-zero device but no device interface.\n";
    return halide_error_code_no_device_interface;
}

WEAK int halide_error_device_interface_no_device(void *user_context) {
    std::cout << "Buffer has a non-null devie_interface but device is 0.\n";
    return halide_error_code_device_interface_no_device;
}

WEAK int halide_error_host_and_device_dirty(void *user_context) {
    std::cout << "Buffer has both host and device dirty bits set.\n";
    return halide_error_code_host_and_device_dirty;
}

WEAK int halide_error_buffer_is_null(void *user_context, const char *routine) {
    std::cout << "Buffer pointer passed to " << routine << " is null.\n";
    return halide_error_code_buffer_is_null;
}

WEAK int halide_error_integer_division_by_zero(void *user_context) {
    std::cout << "Integer division or modulo by zero.\n";
    return halide_error_code_integer_division_by_zero;
}

#ifdef __cplusplus
}  // extern "C"
#endif
