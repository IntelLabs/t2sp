# Change to clang include directory
CWD="$PWD"
cd ./sample_01/
# echo "CWD: $PWD"

# Run
if [ "$1" == "GPU" ]
then
echo "Running preprocessor..."
../../src/t2spreprocessor ./oneapi.gpu.cpp
fi

if [ "$1" == "FPGA" ]
then
echo "Running preprocessor..."
../../src/t2spreprocessor ./gemm.cpp
fi

# Return back to the directory
cd ${CWD}
# echo "CWD: $PWD"

