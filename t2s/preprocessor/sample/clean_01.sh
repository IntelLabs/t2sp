# Change to clang include directory
CWD="$PWD"
cd ./sample_01/
echo "CWD: $PWD"

# Run
echo "cleaning..."
rm a.out;
rm test.fpga_emu;
rm post.*;
rm gemm.generated_oneapi_header.h;

# Return back to the directory
cd ${CWD}
echo "CWD: $PWD"

