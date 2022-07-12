
TARGET := t2spreprocessor

LLVM_SRC_PATH := ${T2S_PATH}/downloads/llvm9.0
LLVM_BUILD_PATH := ${LLVM_SRC_PATH}/build

# LLVM_INSTALL_LIB := -L${T2S_PATH}/install/
LLVM_INSTALL_LIB := -L${LLVM_SRC_PATH}/lib

# HEADERS := -isystem $(LLVM_SRC_PATH)/include
HEADERS := -L$(T2S_PATH)/install/lib 
# HEADERS := -isystem ${T2S_PATH}/install/gcc-7.5.0/include/c++/7.5.0/


BASE_DIR=$(LLVM_SRC_PATH)

LLVM_CONFIG=$(T2S_PATH)/install/bin/llvm-config
LLVM_CXX_LD_FLAGS := `$(LLVM_CONFIG) --cxxflags --ldflags`
LLVM_LIBS=`$(LLVM_CONFIG) --libs --system-libs`
CLANG_INCLUDES= -I $(BASE_DIR)/tools/clang/include/ -I $(T2S_PATH)/install 
CLANG_INCLUDES += -I ${T2S_PATH}/install/lib/clang/9.0.1/include/
OTHER_FLAGS= -I ${T2S_PATH}/install/gcc-7.5.0/lib/gcc/x86_64-pc-linux-gnu/8.4.0/include/ -I ${T2S_PATH}/install/lib/clang/9.0.1/include/

CLANG_LIBS := \
	-Wl,--start-group \
	-lclang \
	-lclangIndex \
	-lclangCodeGen \
	-lclangToolingCore \
	-lclangFrontend \
	-lclangRewriteFrontend \
	-lclangSema \
	-lclangSerialization \
	-lclangParse \
	-lclangASTMatchers \
	-lclangAST \
	-lclangARCMigrate \
	-lclangAnalysis \
	-lclangEdit \
	-lclangRewrite \
	-lclangFrontendTool \
	-lclangDriver \
	-lclangDynamicASTMatchers \
	-lclangFormat \
	-lclangStaticAnalyzerCore \
	-lclangTooling \
	-lclangStaticAnalyzerCheckers \
	-lclangStaticAnalyzerFrontend \
	-lclangBasic \
	-lclangLex \
	-lLLVMSupport \
	-Wl,--end-group



$(TARGET): $(TARGET).cpp
	clang++ -std=c++17 -fno-exceptions -fno-rtti -O3 -Os $^ $(LLVM_CXX_LD_FLAGS) $(CLANG_INCLUDES) $(CLANG_LIBS) $(LLVM_LIBS) -o $@ $(OTHER_FLAGS) 

# g++ -std=c++17 -fno-exceptions -fno-rtti -O3 -Os $^ $(LLVM_CXX_LD_FLAGS) $(CLANG_INCLUDES) $(CLANG_LIBS) $(LLVM_LIBS) -o $@ $(OTHER_FLAGS) 


all: $(TARGET)

.phony: clean
.phony: run

clean:
	rm -rf $(TARGET) $(TARGET).o ./post.*









