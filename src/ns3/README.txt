This directory contains ns3 simulators, which make use of the ns3 library.  The
following steps must be taken to install the prerequisites for these simulators:

1) In a directory outside of the Skynet directory, clone ns3 repository at
   https://gitlab.com/nsnam/ns-3-dev.git
2) In a directory outside of the Skynet directory, clone the waf repository at
   https://gitlab.com/ita1024/waf.git and checkout the waf-2.0.14 tag
3) From the $SKYNET_DIR/utils/ns3 directory, run the build_ns3.sh script (i.e.
   ./build_ns3.sh <absolute_path_to_ns3_dir> <absolute_path_to_waf_dir>)
   NOTE: THIS SCRIPT WILL MODIFY THE NS3_DIR!  This because a newer version of
   waf is required to address an issue with md5 and FIPS.

Once ns3 is successfully installed, you should be able to compile and run the
simulators using

1) make <simulator_name>
2) ./run_ns3_sim.sh <simulator_name>

Currently, the following simulator choices are:

1) udp_echo_debug: a simulator that creates a UDP server and a UDP client that
     send an echo pulse between them
