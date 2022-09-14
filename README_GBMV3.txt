# to run gbmv3

# devcloud_login a10

cd $HOME/t2sp/t2s/tests/performance/gbmv3
rm -rf a.aoc* a/
aoc $COMMON_AOC_OPTION_FOR_EXECUTION a.cl -o a.aocx
source $AOCL_BOARD_PACKAGE_ROOT/linux64/libexec/sign_aocx.sh -H openssl_manager -i a.aocx -r NULL -k NULL -o a_unsigned.aocx
mv a_unsigned.aocx a.aocx
aocl program acl0 a.aocx

# comment lines 67 and 68 from test-common-funcs.sh (cleanup / generate_fpga_kernel)

# exit to login-2

cd $HOME/t2sp/t2s/tests/performance
./devcloud-job.sh gbmv3 a10 large hw