# Change to clang include directory
CWD="$PWD"
cd ./sample_01/
# echo "CWD: $PWD"

# Run
echo "Running preprocessor..."
../../src/t2spreprocessor ./test.t2s.cpp

# Return back to the directory
cd ${CWD}
# echo "CWD: $PWD"

