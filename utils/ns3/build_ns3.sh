#!/bin/bash
#  
#  This script will use the latest waf to build and install the ns3 simulator
# ------------------------------------------------------------------------------

# check usage
if [ $# -lt 2 ]; then
  echo "usage: ./build_ns3.sh <path_to_ns3_dir> <path_to_waf_dir>"
  exit -1
fi

# obtain command line arguments
NS3_DIR=$1
WAF_DIR=$2

# check if NS3_DIR and WAF_DIR are set correctly
if ! [ -f ${NS3_DIR}/waf ]; then
  echo "${NS3_DIR} does not contain waf script"
  exit -1
fi
if ! [ -d ${WAF_DIR}/waflib ]; then
  echo "${WAF_DIR} does not contain waflib subdirectory"
  exit -1
fi

# overwrite native ns3 waf script
cp waf ${NS3_DIR}/waf

# set install directory
INSTALL_DIR=$PWD

# remove any failed build directory and old install directory contents
rm -rf ${NS3_DIR}/build ${INSTALL_DIR}/bin ${INSTALL_DIR}/include ${INSTALL_DIR}/lib64

# set WAFDIR variable used by waf
export WAFDIR=${WAF_DIR}

# configure ns3 with debug profile
cd ${NS3_DIR}
./waf configure \
  --disable-werror \
  --enable-tests \
  --build-profile=debug \
  --out=build/debug \
  --prefix=${INSTALL_DIR}

# check waf configure return code
rc=$?
if [[ $rc != 0 ]]; then
    echo "ERROR: waf configure exited with return code $rc"
    exit $rc
fi

# build ns3 with debug profile
./waf build

# check waf return code
rc=$?
if [[ $rc != 0 ]]; then
    echo "ERROR: waf exited with return code $rc"
    exit $rc
fi

# test debug profile build
./test.py --constrain=core

# check test return code
rc=$?
if [[ $rc != 0 ]]; then
    echo "ERROR: test exited with return code $rc"
    exit $rc
fi

# install ns3 with debug profile
./waf install

# check install return code
rc=$?
if [[ $rc != 0 ]]; then
    echo "ERROR: test exited with return code $rc"
    exit $rc
fi

# configure ns3 with optimized profile
./waf configure \
  --disable-werror \
  --enable-tests \
  --build-profile=optimized \
  --out=build/optimized \
  --prefix=${INSTALL_DIR}

# check waf configure return code
rc=$?
if [[ $rc != 0 ]]; then
    echo "ERROR: waf configure exited with return code $rc"
    exit $rc
fi

# build ns3 with optimized profile
./waf build

# check waf return code
rc=$?
if [[ $rc != 0 ]]; then
    echo "ERROR: waf exited with return code $rc"
    exit $rc
fi

# test optimized profile build
./test.py --constrain=core

# check test return code
rc=$?
if [[ $rc != 0 ]]; then
    echo "ERROR: test exited with return code $rc"
    exit $rc
fi

# install ns3 with optimized profile
./waf install

# check install return code
rc=$?
if [[ $rc != 0 ]]; then
    echo "ERROR: test exited with return code $rc"
    exit $rc
else
    rm -rf build
    rm different.pcap
fi

