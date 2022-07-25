
# Change to clang include directory
CWD="$PWD"
cd ${PWD}/sample_01/
# echo "CWD: $PWD"
if [ "$1" == "FPGA" ];
then
echo "Building T2S...";
echo "CMD: g++ gemm.spec.cpp -I ${T2S_PATH}/t2s/src/ -I ${T2S_PATH}/t2s/tests/correctness/util -I ${T2S_PATH}/Halide/include -L ${T2S_PATH}/Halide/bin -I ${T2S_PATH}/t2s/src/oneapi-src -lz -lpthread -ldl -std=c++11 -lHalide -DTINY -DFPGA_EMULATOR -DFPGA;";
g++ gemm.spec.cpp \
    -I ${T2S_PATH}/t2s/src/ \
    -I ${T2S_PATH}/t2s/tests/correctness/util \
    -I ${T2S_PATH}/Halide/include \
    -I ${T2S_PATH}/t2s/src/oneapi-src \
    -L ${T2S_PATH}/Halide/bin \
    -lz -lpthread -ldl -std=c++11 -lHalide -DTINY -DFPGA_EMULATOR -DFPGA;

echo "Executing T2S...";
./a.out;

echo "Building OneAPI...";
echo "CMD: dpcpp gemm.run.cpp -I ${T2S_PATH}/Halide/include -I ${T2S_PATH}/t2s/src/oneapi-src -L ${T2S_PATH}/Halide/bin -lHalide -lz -lpthread -ldl -fintelfpga -fsycl -fsycl-device-code-split=off -DTINY -DFPGA_EMULATOR -DFPGA -o ./test.fpga_emu;";
dpcpp gemm.run.cpp \
    -I ${T2S_PATH}/Halide/include \
    -I ${T2S_PATH}/t2s/tests/correctness/util \
    -I ${T2S_PATH}/t2s/src/oneapi-src \
    -L ${T2S_PATH}/Halide/bin \
    -lHalide -lz -lpthread -ldl \
    -fintelfpga -fsycl -fsycl-device-code-split=off \
    -DTINY -DFPGA_EMULATOR -DFPGA \
    -o ./test.fpga_emu;

echo "Executing final binary...";
./test.fpga_emu;

# Return back to the directory
# cd ${CWD}
echo "CWD: $PWD"
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
echo "clang++ -fsycl -I $T2S_PATH/install/llvm-test-suite/SYCL/ESIMD/ gemm_genx.cpp"
clang++ -fsycl -I $T2S_PATH/install/llvm-test-suite/SYCL/ESIMD/ gemm_genx.cpp;
#execution
echo "Executing final binary..."
./a.out
echo "done"
# Return back to the directory
# cd ${CWD}
echo "CWD: $PWD"
fi