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

source ./parse-options.sh $@
if [ "$wrong_options" != "0" ]; then
    echo "Error: wrong options encountered"
    exit
fi

if [ "$target" == "a10" ]; then
    devcloud_login -b A10PAC 1.2.1 walltime=12:00:00 ./test.sh $@
elif [ "$target" == "s10" ]; then
    devcloud_login -b S10PAC walltime=12:00:00 ./test.sh $@
elif [ "$target" == "gen9" ]; then
    qsub -l nodes=1:gen9:ppn=2 -d . ./test.sh $@
else
    echo "Error: support for target $target is not released yet"
fi

cd $cur_dir