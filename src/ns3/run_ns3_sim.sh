#!/bin/bash
#
#  Simple wrapper that sets LD_LIBRARY_PATH before running an ns3 script:
#    ./run_ns3_script.sh <ns3_executable>
#
#------------------------------------------------------------------------------
export DYLD_FALLBACK_LIBRARY_PATH=${SKYNET_DIR}/utils/ns3/lib

# check input
if [ $# -lt 1 ]; then
  echo "usage: ./run_ns3_script.sh <ns3_executable>"
  exit -1
fi

LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${SKYNET_DIR}/utils/ns3/lib64; ./$1
