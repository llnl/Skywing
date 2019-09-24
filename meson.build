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

# Is there a better way to say that this depends on the serialization source file?
skynet_serialize= library('skynet_serialize',
  [
    'src/skynet/internal/utility/serialize.cpp'
  ],
  include_directories : include_directories('include')
)

make_network_message_size = executable(
  'make_network_message_size',
  'scripts/generate_message_network_size.cpp',
  dependencies : [
    declare_dependency(
      include_directories : include_directories('include')
    )
  ],
  link_with : skynet_serialize
)

# Having an output to a sub-directory errors; not sure how / if it's
# worth writing to a sub-directory
gen_message_header_information = custom_target(
  'gen_message_header_information',
  input : [],
  output : ['message_header_information.hpp'],
  command : [make_network_message_size]
)

skynet_inc = include_directories('include')
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
