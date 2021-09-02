all:
	rm -rf a a.out $(HOME)/tmp/a.* $(HOME)/tmp/a
	g++ $(file) -g -I ../util  -I ../../../../Halide/include -L ../../../../Halide/bin -lHalide -lpthread -ldl -std=c++11 -DSIZE=10  -DVERBOSE_DEBUG -DPLACE0=Place::Host -DPLACE1=Place::Device -DDESIGN=$(design)
	env CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA=1 AOC_OPTION="-march=emulator -board=$(FPGA_BOARD)" ./a.out
