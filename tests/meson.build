core_tests = {
  # Base path, test names
  'skynet_core': [
    'broadcast_complex',
    'broadcast',
    'disconnect',
    'heartbeat',
    'publish_data_wrapper',
    'simple_reduce',
    'broken_subscribes',
    'broken_reduce',
    'publish_multiple_values'
    # 'basic_master_config'
  ],
  'skynet_core/devices': [
    'socket_communicator'
  ],

  'skynet_upper': [
    'synchronous_iterative'
  ]
}

foreach directory, test_names : core_tests
  foreach test_name : test_names
    exe = executable(
      test_name,
      [directory + '/' + test_name + '.cpp', 'catch_main.cpp'],
      dependencies : [catch2_dep, skynet_core_dep],
      include_directories : include_directories('include')
    )
    # Don't run in parallel so that port collisions don't have to be worried about
    test(test_name, exe, is_parallel : false)
  endforeach
endforeach
