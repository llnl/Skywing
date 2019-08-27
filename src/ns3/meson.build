skyns3_inc = include_directories('skyns3')

executable('udp_echo', 'udp_echo.cpp',
  dependencies : ns3_dep)

executable('skynet_startup',
  [
    'skynet_startup.cpp',
    'skyns3/Ns3_Heart.cpp',
    'skyns3/Ns3_SocketCommunicator.cpp'
  ],
  include_directories : skyns3_inc,
  dependencies : [ns3_dep, skynet_dep])
