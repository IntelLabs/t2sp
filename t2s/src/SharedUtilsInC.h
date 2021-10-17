#ifndef SHARED_UTILS_IN_C
#define SHARED_UTILS_IN_C

/* This file contains common utilities shared by the JIT runtime, AOT runtime, and roofline drawing. */

// Return the bitstream file name with full path.
// Caller: free the space after usage.
extern "C" char *bistream_file_name_with_absolute_path();

// Return the directory of the bitstream file.
// Caller: free the space after usage.
extern "C" char *bitstream_directory();

// Return the directory where Quartus outputs FPGA synthesis results like acl_quartus_report.txt.
// Caller: free the space after usage.
extern "C" char *quartus_output_directory();

// Allocate space and concatenate the directory and the file name there.
// Caller: free the space after usage.
extern "C" char *concat_directory_and_file(const char *dir, const char *file);

#endif
