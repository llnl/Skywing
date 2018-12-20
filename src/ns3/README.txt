This directory contains ns3 simulators, which make use of the ns3 library.  A
script "build_ns3.sh" is provided in utils/ns3 to both build and install ns3.
The comments in that script detail how to obtain ns3 (and it's build system waf)
and to run the script itself.

Once ns3 is successfully installed, you should be able to compile and run the
simulators using

1) make <simulator_name>
2) ./run_ns3_sim.sh <simulator_name>

Currently, the following simulator choices are:

1) udp_echo_debug: a simulator that creates a UDP server and a UDP client that
     send an echo pulse between them
2) skynet_startup_debug: a simulator that creates two empty Skynet_Hearts, whose
     begin_heartbeat() method simply outputs a message to the terminal.
