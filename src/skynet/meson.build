if target_machine.system() == 'darwin'
  platform_specific_sources = [
    'internal/devices/socket_wrappers_osx.cpp'
  ]
elif target_machine.system() == 'linux'
  platform_specific_sources = [
    'internal/devices/socket_wrappers_linux.cpp'
  ]
else
  error('Unsupported build target "' + target_machine.system() + '"')
endif

skynet_lib = static_library('skynet',
  [
    'internal/devices/socket_communicator.cpp',
    'internal/utility/network_conv.cpp',
    'internal/message_creators.cpp',
    'job.cpp',
  ] + platform_specific_sources,
  dependencies : [skynet_internal_dep],
  include_directories : ['../include']
)
