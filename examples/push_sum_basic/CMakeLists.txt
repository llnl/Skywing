add_executable(push_sum_basic push_sum_basic.cpp)
target_link_libraries(push_sum_basic PRIVATE skywing_core)

# NOTE (trb 2023/12/05) -- See hello_world for details
set(push_sum_basic_exe $<TARGET_FILE:push_sum_basic>)
configure_file(run.sh.in run.generator_expression.sh.in @ONLY)
file(GENERATE
  OUTPUT run.sh
  INPUT "${CMAKE_CURRENT_BINARY_DIR}/run.generator_expression.sh.in")
