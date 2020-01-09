# ['test_name', 'path/to/test/source']
tests = [
  # ./
  ['broadcast_complex', 'broadcast_complex.cpp'],
  ['broadcast', 'broadcast.cpp'],
  ['disconnect', 'disconnect.cpp'],
  ['heartbeat', 'heartbeat.cpp'],
  ['master_conn', 'master_conn.cpp'],
  ['publish_data_wrapper', 'publish_data_wrapper.cpp'],
  ['simple_reduce', 'simple_reduce.cpp'],
  ['broken_subscribes', 'broken_subscribes.cpp'],

  # devices/
  ['socket_communicator', 'devices/socket_communicator.cpp']
]

foreach t : tests
  exe = executable(
    t[0], # test name
    ['skynet/' + t[1], 'catch_main.cpp'], # test executable
    dependencies : [catch2_dep, skynet_dep, cppzmq_dep, zmq_dep],
    include_directories : include_directories('include')
  )
  # Don't run in parallel so that port collisions don't have to be worried about
  test(t[0], exe, is_parallel : false)
endforeach
