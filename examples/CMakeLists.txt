add_subdirectory(asynchronous_iterative)
add_subdirectory(bigfloat)
add_subdirectory(collective_count)
add_subdirectory(hello_world)
add_subdirectory(mean_or_sum)
add_subdirectory(power_method)
add_subdirectory(push_sum_basic)

add_subdirectory(math_interface_examples)
add_subdirectory(unix_hello_world)


if (SKYWING_BUILD_LC_EXAMPLES)
  add_subdirectory(lc_hello_world)
  add_subdirectory(lc_push_sum)
endif ()

if (SKYWING_USE_EIGEN)
  add_subdirectory(admm)
endif ()
