# ['test_name', 'path/to/test/source']
tests = [
  # ./
  ['tag_test', 'tags_test.cpp'],

  # devices/
  ['socket_communicator_test', 'devices/socket_communicator_test.cpp'],

  # utility/
  ['future_test', 'utility/future_test.cpp'],
  ['optional_test', 'utility/optional_test.cpp']
]

foreach t : tests
  exe = executable(
    t[0], # test name
    [t[1], 'catch_main.cpp'], # test executable
    dependencies : [catch2_dep, skynet_dep, cppzmq_dep, zmq_dep])
  test(t[0], exe)
endforeach
