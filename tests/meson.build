core_tests = {
  # Base path, test names
  'core': [
    'assorted',
    # 'basic_master_config',
    'broadcast_complex',
    'broadcast',
    'broken_reduce',
    'broken_subscribes',
    'disconnect',
    'heartbeat',
    'publish_data_wrapper',
    'publish_multiple_values',
    'self_subscribe',
    'simple_reduce',
  ],
  'core/devices': [
    'socket_communicator'
  ],

  'upper': [
    'asynchronous_iterative',
    'synchronous_iterative'
  ]
}

foreach directory, test_names : core_tests
  foreach test_name : test_names
    exe = executable(
      test_name,
      ['skynet_' + directory + '/' + test_name + '.cpp', 'catch_main.cpp'],
      dependencies : [catch2_dep, skynet_core_dep],
      include_directories : include_directories('include')
    )
    # Don't run in parallel so that port collisions don't have to be worried about
    test(directory + '/' + test_name, exe, is_parallel : false)
  endforeach
endforeach
