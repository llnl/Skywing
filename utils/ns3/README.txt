This directory will contain an installed version of the ns3 discrete event
simulator.  Follow these steps to first populate this directory:

  1) In a directory outside of the Skynet directory, clone ns3 repository at 
     https://github.com/GMLC-TDC/ns-3-dev-git
  2) Check the contrib/helics subdirectory of the ns3 repo.  If it is empty,
     complete the additional step
     - clone the HELICS repository at https://github.com/GMLC-TDC/HELICS-src
       into the contrib/helics directory
       (ie git clone https://github.com/GMLC-TDC/HELICS-src contrib/helics)
  3) In a directory outside of the Skynet directory, clone waf repository at 
     https://gitlab.com/ita1024/waf
  4) From the $SKYNET_DIR/utils/ns3 directory, run the build_ns3.sh script: 
     (ie ./build_ns3.sh <path_to_ns3_dir> <path_to_waf_dir>)
     NOTE: THIS SCRIPT WILL MODIFY THE NS3_DIR!  This because a newer version of
     waf is required to address an issue with md5 and FIPS.
