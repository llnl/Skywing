#!/bin/bash
#
#  This script will use the latest waf to build and install the ns3 simulator
# ------------------------------------------------------------------------------

# check usage
if [ $# -ne 4 ]; then
  echo "usage: ./build_ns3.sh <path_to_ns3_dir> <path_to_waf_dir> <path_to_install_dir> <build_profile>"
  exit -1
fi

# obtain command line arguments
NS3_DIR=$1
WAF_DIR=$2

# set install directory
INSTALL_DIR=$3

# set debug mode
BUILD_PROFILE=$4

echo "Installing ns3 ($1) with waf ($2) at ($3), with profile ($4)"

echo "Verifying ns3, waf"
if ! [ -d ${WAF_DIR}/waflib ]; then
  echo "${WAF_DIR} does not contain waflib subdirectory"
  exit -1
fi

echo "Replacing ns3 waf with waf-light"
unlink ${NS3_DIR}/waf
ln -s ${WAF_DIR}/waf-light ${NS3_DIR}/waf

# set WAFDIR variable used by waf
export WAFDIR=${WAF_DIR}

echo "Configuring ns3"
cd ${NS3_DIR}
./waf configure \
  --disable-werror \
  --enable-tests \
  --build-profile=${BUILD_PROFILE} \
  --prefix=${INSTALL_DIR}

# check waf configure return code
rc=$?
if [[ $rc != 0 ]]; then
    echo "ERROR: waf configure exited with return code $rc"
    exit $rc
fi

echo "Building ns3"
# build ns3 with debug profile
./waf build

# check waf return code
rc=$?
if [[ $rc != 0 ]]; then
    echo "ERROR: waf build exited with return code $rc"
    exit $rc
fi

# test debug profile build
./test.py --constrain=core

# check test return code
rc=$?
if [[ $rc != 0 ]]; then
    echo "WARNING: test.py exited with return code $rc"
fi

echo "Installing ns3"
# install ns3 with debug profile
./waf install

# check install return code
rc=$?
if [[ $rc != 0 ]]; then
    echo "ERROR: waf install exited with return code $rc"
    exit $rc
fi
