# ['test_name', 'path/to/test/source']
tests = [
  # ./
  ['broadcast', 'broadcast.cpp'],
  ['broadcast_complex', 'broadcast_complex.cpp'],
  ['master_conn', 'master_conn.cpp'],
  ['tags', 'tags.cpp'],

  # devices/
  ['socket_communicator', 'devices/socket_communicator.cpp'],

  # utility/
  ['future', 'utility/future.cpp'],
  ['optional', 'utility/optional.cpp']
]

foreach t : tests
  exe = executable(
    t[0], # test name
    [t[1], 'catch_main.cpp'], # test executable
    dependencies : [catch2_dep, skynet_dep, cppzmq_dep, zmq_dep])
  test(t[0], exe)
endforeach
