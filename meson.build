project('skynet', 'cpp',
  version: '0.0.1',
  default_options : [
    'cpp_std=c++17',
    'warning_level=3',
    'werror=true'
  ]
)

use_ns3 = get_option('use_ns3')
message('using ns3: ' + use_ns3.to_string())

catch2_dep = subproject('catch2').get_variable('catch2_dep')

if use_ns3
  ns3_dep = subproject('ns3').get_variable('ns3_dep')
endif

zmq_dep = dependency('libzmq', fallback : ['libzmq', 'libzmq_dep'])
cppzmq_dep = dependency('cppzmq', fallback : ['cppzmq', 'cppzmq_dep'])
thread_dep = dependency('threads')

subdir('generated_includes/generated')

skynet_inc = include_directories('include', 'generated_includes')
skynet_internal_dep = declare_dependency(
  include_directories : skynet_inc,
  dependencies : [thread_dep],
  link_with : skynet_serialize,
  sources : gen_message_header_information
)

subdir('src/skynet')

skynet_dep = declare_dependency(
  dependencies : skynet_internal_dep,
  link_with : skynet_lib
)

if use_ns3
  subdir('src/ns3')
endif

subdir('tests')
