#!/bin/bash

# BASH_SOURCE is this script
if [ $0 != $BASH_SOURCE ]; then
    echo "Error: The script should be directly run, not sourced"
    return
fi

if [ "$HOSTNAME" != "login-2" ]; then
    echo "Error: The script should be run on the head node of DevCloud, i.e. login-2."
    exit
fi

# The path to this script.
PATH_TO_SCRIPT="$( cd "$(dirname "$BASH_SOURCE" )" >/dev/null 2>&1 ; pwd -P )"

# Devcloud environment setting
if [ -f /data/intel_fpga/devcloudLoginToolSetup.sh ]; then
    source /data/intel_fpga/devcloudLoginToolSetup.sh
fi

cur_dir=$PWD
cd $PATH_TO_SCRIPT

source ./parse-options.sh devcloud $@
if [ "$wrong_options" != "0" ]; then
    echo "Error: wrong options encountered"
    exit
fi

# It seems we cannot directly pass the options to test.sh with devcloud_login. 
# So generate a shell script to call test.sh instead.
echo "cd $PATH_TO_SCRIPT && ./test.sh devcloud $workload $target $size $platform" > job.sh
chmod a+x job.sh

time_budget="24:00:00"
if [ "$platform" == "emulator" ]; then
    # We emulate only tiny size and the time should be in minutes
    time_budget="00:10:00"
fi

if [ "$target" == "a10" ]; then
    devcloud_login -b A10PAC 1.2.1 walltime=$time_budget ./job.sh
elif [ "$target" == "s10" ]; then
    devcloud_login -b S10PAC walltime=$time_budget ./job.sh
elif [ "$target" == "gen9" ]; then
    qsub -l nodes=1:gen9:ppn=2 -d . ./job.sh
else
    echo "Error: support for target $target is not released yet"
fi

cd $cur_dir
