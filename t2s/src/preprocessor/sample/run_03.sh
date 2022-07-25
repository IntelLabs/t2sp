# Change to clang include directory
CWD="$PWD"
cd ./sample_03/
echo "CWD: $PWD"

# Run
if [ "$1" == "GPU" ]
then
echo "Running preprocessor..."
../../src/t2spreprocessor ./oneapi.gpu.cpp
fi

if [ "$1" == "FPGA" ]
then
echo "currently FPGA capsule example has not been added to this directory"
fi

# Return back to the directory
cd ${CWD}
# echo "CWD: $PWD"

