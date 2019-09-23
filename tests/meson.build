# ['test_name', 'path/to/test/source']
tests = [
  # ./
  ['broadcast', 'broadcast.cpp'],
  ['broadcast_complex', 'broadcast_complex.cpp'],
  ['disconnect', 'disconnect.cpp'],
  ['master_conn', 'master_conn.cpp'],

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
  test(t[0], exe)
endforeach
