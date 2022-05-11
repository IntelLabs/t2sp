#!/bin/bash

function show_usage {
    echo "Usage:"
    echo "  ./install-tool.sh m4|gmp|mpfr|mpc|cmake|gcc|llvm-clang|python-packages|cm|git-lfs"
}

# No matter the script is sourced or directly run, BASH_SOURCE is always this script, and $1 is the
# argument to the script
T2S_PATH="$( cd "$(dirname "$BASH_SOURCE" )" >/dev/null 2>&1 ; pwd -P )" # The path to this script
if [ "$1" != "m4"  -a  "$1" != "gmp" -a  "$1" != "mpfr" -a  "$1" != "mpc" -a  "$1" != "cmake" -a  "$1" != "gcc" -a "$1" != "llvm-clang" -a "$1" != "python-packages" -a "$1" != "cm" -a "$1" != "git-lfs" ]; then
    show_usage
    if [ $0 == $BASH_SOURCE ]; then
        # The script is directly run
        exit
    else
        return
    fi
else
    component="$1"
fi

function install_cmake {
    eval major="$1"
    eval minor="$2"
    echo Installing cmake ...
    mkdir -p cmake-$minor && cd cmake-$minor
    wget -c https://cmake.org/files/v$major/cmake-$minor.tar.gz
    tar -zxvf cmake-$minor.tar.gz > /dev/null
    cd cmake-$minor
    mkdir -p build && cd build
    ../configure --prefix=$T2S_PATH/install > /dev/null
    make -j`nproc` > /dev/null
    make install > /dev/null
    cd ..
    cd ..
    cd ..
}

function install_m4 {
    eval version="$1"
    echo Installing m4 ...
    wget -c http://ftp.gnu.org/gnu/m4/m4-$version.tar.xz
    tar xvf m4-$version.tar.xz > /dev/null
    cd m4-$version
    ./configure --prefix=$T2S_PATH/install > /dev/null
    make -j`nproc` > /dev/null
    make install > /dev/null
    cd ..
}

function install_gmp {
    eval version="$1"
    echo Installing gmp ...
    wget -c https://ftp.gnu.org/gnu/gmp/gmp-$version.tar.xz
    tar xvf gmp-$version.tar.xz > /dev/null
    cd gmp-$version
    ./configure --prefix=$T2S_PATH/install > /dev/null
    make -j`nproc` > /dev/null
    make install > /dev/null
    cd ..
}

function install_mpfr {
    eval version="$1"
    echo Installing mpfr ...
    wget -c https://www.mpfr.org/mpfr-current/mpfr-$version.tar.gz
    tar xvzf mpfr-$version.tar.gz > /dev/null
    cd mpfr-$version
    ./configure --prefix=$T2S_PATH/install --with-gmp=$T2S_PATH/install  > /dev/null
    make -j`nproc` > /dev/null
    make install > /dev/null
    cd ..
}

function install_mpc {
    eval version="$1"
    echo Installing mpc ...
    wget -c https://ftp.gnu.org/gnu/mpc/mpc-$version.tar.gz
    tar xvzf mpc-$version.tar.gz > /dev/null
    cd mpc-$version
    ./configure --prefix=$T2S_PATH/install --with-gmp=$T2S_PATH/install  --with-mpfr=$T2S_PATH/install > /dev/null
    make -j`nproc` > /dev/null
    make install > /dev/null
    cd ..
}

function install_gcc {
    eval version="$1"
    echo Installing gcc ...
    wget -c http://www.netgull.com/gcc/releases/gcc-$version/gcc-$version.tar.gz
    tar xvzf gcc-$version.tar.gz > /dev/null
    mkdir -p gcc-$version-build && cd gcc-$version-build
    export LD_LIBRARY_PATH=$T2S_PATH/install/lib:$T2S_PATH/install/lib64:$LD_LIBRARY_PATH
    ../gcc-$version/configure --enable-languages=c,c++ --disable-multilib --disable-libsanitizer  --prefix=$T2S_PATH/install/gcc-$version --with-gmp=$T2S_PATH/install --with-mpfr=$T2S_PATH/install --with-mpc=$T2S_PATH/install > /dev/null
    make -j`nproc` > /dev/null
    make install > /dev/null
    cd ..
}

function install_llvm_clang {
    eval release="$1"
    eval version="$2"
    eval gcc_version="$3"
    echo Installing llvm and clang ...
    git clone -b release_$release https://github.com/llvm-mirror/llvm.git llvm$version
    git clone -b release_$release https://github.com/llvm-mirror/clang.git llvm$version/tools/clang
    cd llvm$version
    mkdir -p build && cd build
    export PATH=$T2S_PATH/install/bin:$PATH
    export LD_LIBRARY_PATH=$T2S_PATH/install/lib:$T2S_PATH/install/lib64:$LD_LIBRARY_PATH
    CXX=$T2S_PATH/install/gcc-$gcc_version/bin/g++ CC=$T2S_PATH/install/gcc-$gcc_version/bin/gcc cmake -DCMAKE_CXX_LINK_FLAGS="-Wl,-rpath,$T2S_PATH/install/gcc-$gcc_version/lib64 -L$T2S_PATH/install/gcc-$gcc_version/lib64" \
        -DLLVM_ENABLE_TERMINFO=OFF -DLLVM_TARGETS_TO_BUILD="X86" -DLLVM_ENABLE_ASSERTIONS=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$T2S_PATH/install .. > /dev/null
    make -j`nproc` > /dev/null
    make install > /dev/null
    cd ..
    cd ..
}

function install_python-packages {
    pip install numpy
    pip install matplotlib
}

function install_cm_20211028 {
    wget -c https://01.org/sites/default/files/downloads/cmsdk20211028.zip
    unzip -d $T2S_PATH/install cmsdk20211028.zip
}

function install_cm_20200119 {
    wget -c https://github.com/intel/cm-compiler/releases/download/Master/Linux_C_for_Metal_Development_Package_20200119.zip
    unzip Linux_C_for_Metal_Development_Package_20200119.zip

    cd Linux_C_for_Metal_Development_Package_20200119
    chmod +x compiler/bin/cmc

    cd drivers/media_driver/release
    mkdir extract
    dpkg -X intel-media-u18.04-release.deb extract/
    cd -

    cd drivers/IGC
    mkdir extract
    dpkg -X intel-igc.deb extract/
    cd -

    cd ..
    cp -rf Linux_C_for_Metal_Development_Package_20200119 $T2S_PATH/install
}

function install_git_lfs {
    eval version="$1"
    wget -c https://github.com/git-lfs/git-lfs/releases/download/v3.1.4/git-lfs-linux-amd64-v$version.tar.gz
    mkdir git-lfs
    tar -xvf git-lfs-linux-amd64-v$version.tar.gz -C git-lfs > /dev/null
    cd git-lfs
    sed -i "4c prefix=${T2S_PATH}/install" install.sh
    ./install.sh
    cd ..
}

# Below we install newer version of gcc and llvm-clang and their dependencies
mkdir -p $T2S_PATH/install $T2S_PATH/install/bin
export PATH=$T2S_PATH/install/bin:$PATH

cd $T2S_PATH
mkdir -p downloads
cd downloads

if [ "$component" == "m4" ]; then
    install_m4         "1.4.18"
fi
if [ "$component" == "gmp" ]; then
    install_gmp        "6.2.1"
fi
if [ "$component" == "mpfr" ]; then
    install_mpfr       "4.1.0"
fi
if [ "$component" == "mpc" ]; then
    install_mpc        "1.2.1"
fi
if [ "$component" == "cmake" ]; then
    install_cmake      "3.11"  "3.11.1"
fi
if [ "$component" == "gcc" ]; then
    install_gcc        "7.5.0"
fi
if [ "$component" == "llvm-clang" ]; then
    install_llvm_clang "90"    "9.0"    "7.5.0"
fi
if [ "$component" == "python-packages" ]; then
    install_python-packages
fi
if [ "$component" == "cm" ]; then
    # install_cm_20211028
    install_cm_20200119
fi
if [ "$component" == "git-lfs" ]; then
    install_git_lfs 3.1.4
fi

cd ..
