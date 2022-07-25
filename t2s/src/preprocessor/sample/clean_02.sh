# Change to clang include directory
CWD="$PWD"
cd ./sample_02/
echo "CWD: $PWD"

# Run
echo "cleaning..."
rm -rf a.out;
rm -rf test.fpga_emu;
rm -rf *.sycl.h;
rm -rf *_genx.cpp
rm -rf *.spec.cpp
rm -rf *.run.cpp
# Return back to the directory
cd ${CWD}
echo "CWD: $PWD"

