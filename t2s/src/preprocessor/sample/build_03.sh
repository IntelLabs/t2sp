
# Change to clang include directory
CWD="$PWD"
cd ${PWD}/sample_03/
# echo "CWD: $PWD"
if [ "$1" == "FPGA" ];
then
echo "Conv FPGA example has not been added to this directory yet";
elif [ "$1" == "GPU" ];
then
echo "Building T2S for Intel GPU...";
echo "CMD: g++ oneapi.gpu.spec.cpp -I ${T2S_PATH}/t2s/src/ -I ${T2S_PATH}/t2s/tests/correctness/util -I ${T2S_PATH}/Halide/include -L ${T2S_PATH}/Halide/bin -lz -lpthread -ldl -std=c++11 -lHalide -DTINY -DGPU;";
g++ oneapi.gpu.spec.cpp \
    -I ${T2S_PATH}/t2s/src/ \
    -I ${T2S_PATH}/t2s/tests/correctness/util \
    -I ${T2S_PATH}/Halide/include \
    -L ${T2S_PATH}/Halide/bin \
    -lz -lpthread -ldl -std=c++11 -lHalide -DTINY -DGPU;

echo "Executing T2S...";
./a.out;
#format the file
clang-format -style=LLVM -i *_genx.cpp 
#building oneapi esimd
echo "Building oneAPI...";
echo "clang++ -fsycl -I $T2S_PATH/install/llvm-test-suite/SYCL/ESIMD/ capsule_genx.cpp"
clang++ -fsycl -I $T2S_PATH/install/llvm-test-suite/SYCL/ESIMD/ capsule_genx.cpp;
#execution
echo "Executing final binary..."
./a.out
echo "done"
# Return back to the directory
# cd ${CWD}
echo "CWD: $PWD"
fi