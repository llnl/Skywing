# Get Catch2
FetchContent_Declare(
  Catch2
  SYSTEM
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG fa43b77429ba76c462b1898d6cd2f2d7a9416b14 # v3.7.1
  GIT_SHALLOW 1
  FIND_PACKAGE_ARGS 3.0.0 CONFIG
)
FetchContent_MakeAvailable(Catch2)
report_deps(Catch2)

include(Catch)

# NOTE (trb 2023/12/04) -- This should all move into a single catch
# binary, but there are some technical hurdles to overcome.
# The primary issue is that these are all integration tests that rely
# on connecting to (real) local ports. As such, they must be protected
# by a TIMEOUT, lest they hang indefinitely. Additionally, they may
# fail to connect sporadically; there's little or no consideration
# given to multiple connection attempts or recovery upon failure.
# In order to improve the probability that a contiguous block of ports
# will be available to each test, the `START_PORT` env var is
# incremented in the test script. This might be moved into a global
# fixture that Catch manages.
# Anyway, this is a bit long-winded, but it builds both a single
# binary and a separate binary for each individual test case using
# only a single object file for each source file (rather than building
# <test-name>.cpp.o twice).

set(TEST_HDRS include/utils.hpp)

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
  skywing_core/buffers.cpp
  skywing_core/agent_disconnect.cpp
  skywing_core/agent_reconnect.cpp

  skywing_mid/associative_vector.cpp
  skywing_mid/idempotent_processor_test.cpp
  skywing_mid/asynchronous_iterative.cpp
  skywing_mid/count_test.cpp
  skywing_mid/ex4.cpp
  skywing_mid/max_test.cpp
  skywing_mid/pubsub.cpp
  skywing_mid/iterationpolicy_send.cpp
  skywing_mid/synchronous_iterative.cpp
  skywing_mid/data_handler_test.cpp

  skywing_math_interface/jacobi_test.cpp
  skywing_math_interface/jacobi_root_node_test.cpp
)

file(COPY
  skywing_math_interface/data/
  DESTINATION ${CMAKE_BINARY_DIR}/data)

# Conditionally add tests for Eigen processors if SKYWING_USE_EIGEN is ON
if (SKYWING_USE_EIGEN)
  list(APPEND TEST_SRCS skywing_mid/admm_test.cpp)
  list(APPEND TEST_SRCS skywing_math_interface/cola_test.cpp)
endif()

# Add the executable for catch-tests
add_executable(catch-tests ${TEST_HDRS})

# Link libraries to catch-tests
target_link_libraries(catch-tests
  PRIVATE
  Catch2::Catch2WithMain)

# Set C++ standard
target_compile_features(catch-tests PUBLIC cxx_std_17)

set(_test_list)

foreach (src IN LISTS TEST_SRCS)
  get_filename_component(TEST_NAME "${src}" NAME_WE)
  add_executable(${TEST_NAME})
  add_library(${TEST_NAME}-lib OBJECT "${src}")

  # Object library target_include_directories
  target_include_directories(${TEST_NAME}-lib
    PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/include"
  )
  target_link_libraries(${TEST_NAME}-lib
    PUBLIC
    Catch2::Catch2WithMain
    skywing_mid
  )

  # Set location of input linear system data files
  target_compile_definitions(${TEST_NAME}-lib
    PRIVATE
    DATA_DIR_DEST="${CMAKE_BINARY_DIR}/data"
  )

  # Conditionally link to skywing_math_interface for Eigen dependency
  if (SKYWING_USE_EIGEN)
    target_link_libraries(${TEST_NAME}-lib
      PUBLIC
      skywing_math_interface
    )
  endif()

  # Single-test executable
  target_link_libraries(${TEST_NAME}
    PRIVATE
    ${TEST_NAME}-lib
    Catch2::Catch2WithMain)

  # C++17
  target_compile_features(${TEST_NAME}-lib
    PUBLIC
    cxx_std_17
  )
  target_compile_features(${TEST_NAME}
    PUBLIC
    cxx_std_17
  )

  # Add to the multiple-test driver version as well.
  target_link_libraries(catch-tests
    PRIVATE
    ${TEST_NAME}-lib
  )

  get_filename_component(dir_name "${src}" DIRECTORY)
  string(REPLACE "skywing_" "" comp_name "${dir_name}")
  string(APPEND _test_list "${comp_name}/${TEST_NAME} $<TARGET_FILE:${TEST_NAME}>\n")
endforeach()

# Write out the test list for the tester script
file(GENERATE OUTPUT test_runner.txt CONTENT "${_test_list}")

# Setup CTest tests, one per test case.
catch_discover_tests(catch-tests)
