add_executable(collective_count collective_count.cpp)
target_link_libraries(collective_count PRIVATE skywing_core)

# NOTE (trb 2023/12/05) -- See hello_world for details
set(collective_count_exe $<TARGET_FILE:collective_count>)
configure_file(run.sh.in run.generator_expression.sh.in @ONLY)
file(GENERATE
  OUTPUT run.sh
  INPUT "${CMAKE_CURRENT_BINARY_DIR}/run.generator_expression.sh.in")
