add_executable(mean_or_sum mean_or_sum.cpp)
target_link_libraries(mean_or_sum PRIVATE skywing_core)

# NOTE (trb 2023/12/05) -- See hello_world for details
set(mean_or_sum_exe $<TARGET_FILE:mean_or_sum>)
configure_file(run.sh.in run.generator_expression.sh.in @ONLY)
file(GENERATE
  OUTPUT run.sh
  INPUT "${CMAKE_CURRENT_BINARY_DIR}/run.generator_expression.sh.in")
