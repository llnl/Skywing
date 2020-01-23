# ['test_name', 'path/to/test/source']
core_tests = [
  # skynet_core/
  ['broadcast_complex', 'broadcast_complex.cpp'],
  ['broadcast', 'broadcast.cpp'],
  ['disconnect', 'disconnect.cpp'],
  ['heartbeat', 'heartbeat.cpp'],
  ['master_conn', 'master_conn.cpp'],
  ['publish_data_wrapper', 'publish_data_wrapper.cpp'],
  ['simple_reduce', 'simple_reduce.cpp'],
  ['broken_subscribes', 'broken_subscribes.cpp'],
  ['broken_reduce', 'broken_reduce.cpp'],
  # ['basic_master_config', 'basic_master_config.cpp'],

  # skynet_core/devices/
  ['socket_communicator', 'devices/socket_communicator.cpp']
]

foreach t : core_tests
  exe = executable(
    t[0], # test name
    ['skynet_core/' + t[1], 'catch_main.cpp'], # test executable
    dependencies : [catch2_dep, skynet_core_dep],
    include_directories : include_directories('include')
  )
  # Don't run in parallel so that port collisions don't have to be worried about
  test(t[0], exe, is_parallel : false)
endforeach

upper_tests = [
  #skynet_upper/
  ['synchronous_iterative', 'synchronous_iterative.cpp']
]
foreach t : upper_tests
  exe = executable(
    t[0],
    ['skynet_upper/' + t[1], 'catch_main.cpp'],
    dependencies : [catch2_dep, skynet_upper_dep],
    include_directories : include_directories('include')
  )
  test(t[0], exe, is_parallel : false)
endforeach
