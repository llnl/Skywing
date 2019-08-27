# ['test_name', 'path/to/test/source']
tests = [
  # data/
  ['keyvaluereader_test', 'data/Skynet_KeyValueReader_test.cpp'],

  # devices/
  ['serialization_test', 'devices/Skynet_Serialization_test.cpp'],
  ['socketbuildcommunications_test', 'devices/Skynet_SocketBuildCommunications_test.cpp'],
  # ['socketcommunicator_test', 'devices/Skynet_SocketCommunicator_test.cpp'],
  ['socketgateway_test', 'devices/Skynet_SocketGateway_test.cpp'],

  # heartbeat/
  ['deviceidregistry_test', 'heartbeat/Skynet_DeviceIdRegistry_test.cpp'],
  ['heart_test', 'heartbeat/Skynet_Heart_test.cpp']
]

foreach t : tests
  exe = executable(
    t[0], # test name
    [t[1], 'catch_main.cpp'], # test executable
    dependencies : [catch2_dep, skynet_dep])
  test(t[0], exe)
endforeach
