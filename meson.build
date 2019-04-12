project('skynet', 'cpp',
  version: '0.0.1',
  default_options : ['cpp_std=c++14'])

catch2_dep = subproject('catch2').get_variable('catch2_dep')
thread_dep = dependency('threads')
# ns3_dep = dependency('ns3')

skynet_inc = include_directories('include')
skynet_internal_dep = declare_dependency(
  include_directories : skynet_inc,
  dependencies : [thread_dep])#, ns3_dep])

subdir('src/skynet')

skynet_dep = declare_dependency(
  dependencies : skynet_internal_dep,
  link_with : skynet_lib)

subdir('src/tests')
