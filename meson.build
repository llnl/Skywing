project('skynet', 'cpp',
  version: '0.0.1',
  default_options : [
    'cpp_std=c++17',
    'warning_level=3',
    'werror=true'
  ]
)

add_project_arguments(
  '-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_' + get_option('logging_level').to_upper(),
  language : 'cpp'
)

use_ns3 = get_option('use_ns3')
message('using ns3: ' + use_ns3.to_string())

catch2_dep = subproject('catch2').get_variable('catch2_dep')

if use_ns3
  ns3_dep = subproject('ns3').get_variable('ns3_dep')
endif

thread_dep = dependency('threads')
spdlog_dep = subproject('spdlog').get_variable('spdlog_dep')

capnproto_dep = dependency('capnp')

subdir('generated_files/generated')

skynet_inc = include_directories('include', 'generated_files')
skynet_internal_dep = declare_dependency(
  include_directories : skynet_inc,
  dependencies : [thread_dep, capnproto_dep, spdlog_dep],
  sources : [generated_sources]
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
subdir('examples')
