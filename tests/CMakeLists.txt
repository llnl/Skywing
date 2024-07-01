# Handle the submodule
if (NOT SKYWING_EXTERNAL_CATCH2)
  list(APPEND CMAKE_MODULE_PATH
    "${PROJECT_SOURCE_DIR}/subprojects/catch2/Catch2/contrib")
  include(Catch)
endif ()

# NOTE (trb 2023/12/04) -- This should all move into a single catch
# binary, but there are some technical hurdles to overcome.
#
# The primary issue is that these are all integration tests that rely
# on connecting to (real) local ports. As such, they must be protected
# by a TIMEOUT, lest they hang indefinitely. Additionally, they may
# fail to connect sporadically; there's little or no consideration
# given to multiple connection attempts or recovery upon failure.
#
# In order to improve the probability that a contiguous block of ports
# will be available to each test, the `START_PORT` env var is
# incremented in the test script. This might be moved into a global
# fixture that Catch manages.
#
# Anyway, this is a bit long-winded, but it builds both a single
# binary and a separate binary for each individual test case using
# only a single object file for each source file (rather than building
# <test-name>.cpp.o twice).

set(TEST_HDRS
  include/utils.hpp
)
set(TEST_SRCS
  skywing_core/assorted.cpp
  skywing_core/broadcast.cpp
  # skywing_core/broken_subscribes.cpp # Commented out in meson.build
  skywing_core/disconnect.cpp
  skywing_core/heartbeat.cpp
  skywing_core/ip_subscribe.cpp
  skywing_core/publish_data_wrapper.cpp
  skywing_core/publish_multiple_values.cpp
  skywing_core/repeat_connection.cpp
  skywing_core/self_subscribe.cpp
  skywing_core/devices/socket_communicator.cpp
  skywing_core/multiple_jobs_test.cpp
  skywing_core/tag.cpp
  skywing_core/subscription.cpp

  skywing_mid/associative_vector.cpp
  skywing_mid/asynchronous_iterative.cpp
  skywing_mid/count_test.cpp
  skywing_mid/max_test.cpp
  skywing_mid/pubsub.cpp
  skywing_mid/stoppolicy_send.cpp
  skywing_mid/synchronous_iterative.cpp
)

add_executable(catch-tests
  ${TEST_HDRS}
)
target_link_libraries(catch-tests
  PRIVATE
  Catch2::Catch2WithMain
)

target_compile_features(catch-tests PUBLIC cxx_std_17)

set(_test_list)
foreach (src IN LISTS TEST_SRCS)
  get_filename_component(TEST_NAME "${src}" NAME_WE)

  add_executable(${TEST_NAME})
  add_library(${TEST_NAME}-lib OBJECT "${src}")

  # Object library
  target_include_directories(${TEST_NAME}-lib
    PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/include"
  )
  target_link_libraries(${TEST_NAME}-lib
    PUBLIC
    Catch2::Catch2WithMain
    skywing_mid
  )

  # Single-test executable
  target_link_libraries(${TEST_NAME}
    PRIVATE
    ${TEST_NAME}-lib
    Catch2::Catch2WithMain
  )

  # C++17
  target_compile_features(${TEST_NAME}-lib PUBLIC cxx_std_17)
  target_compile_features(${TEST_NAME} PUBLIC cxx_std_17)

  # Add to the multiple-test driver version as well.
  target_link_libraries(catch-tests
    PRIVATE
    ${TEST_NAME}-lib
  )

  get_filename_component(dir_name "${src}" DIRECTORY)
  string(REPLACE "skywing_" "" comp_name "${dir_name}")
  string(APPEND _test_list
    "${comp_name}/${TEST_NAME} $<TARGET_FILE:${TEST_NAME}>\n")
endforeach ()

# Write out the test list for the tester script
file(GENERATE
  OUTPUT test_runner.txt
  CONTENT "${_test_list}")

# Setup CTest tests, one per test case.
catch_discover_tests(catch-tests)
