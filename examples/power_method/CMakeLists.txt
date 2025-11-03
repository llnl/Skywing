add_executable(power_method power_method.cpp)
target_link_libraries(power_method PRIVATE skywing_core)

# NOTE (trb 2023/12/05) -- See hello_world for details
set(power_method_exe $<TARGET_FILE:power_method>)
configure_file(run.sh.in run.generator_expression.sh.in @ONLY)
file(GENERATE
  OUTPUT run.sh
  INPUT "${CMAKE_CURRENT_BINARY_DIR}/run.generator_expression.sh.in")
