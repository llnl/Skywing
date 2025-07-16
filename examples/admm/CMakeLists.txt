add_executable(admm admm.cpp)
target_link_libraries(admm PRIVATE skywing_math_interface)

# NOTE (trb 2023/12/05) -- See hello_world for details
set(admm_exe $<TARGET_FILE:admm>)
configure_file(run.sh.in run.generator_expression.sh.in @ONLY)
file(GENERATE
  OUTPUT run.sh
  INPUT "${CMAKE_CURRENT_BINARY_DIR}/run.generator_expression.sh.in")

configure_file(14_bus_problem_data/matrix.txt 14_bus_problem_data/matrix.txt @ONLY)
configure_file(14_bus_problem_data/rhs.txt 14_bus_problem_data/rhs.txt @ONLY)
configure_file(14_bus_problem_data/exact_soln.txt 14_bus_problem_data/exact_soln.txt @ONLY)
configure_file(14_bus_problem_data/col_partition.txt 14_bus_problem_data/col_partition.txt @ONLY)
configure_file(14_bus_problem_data/row_partition.txt 14_bus_problem_data/row_partition.txt @ONLY)
configure_file(14_bus_problem_data/communication_pattern.txt 14_bus_problem_data/communication_pattern.txt @ONLY)