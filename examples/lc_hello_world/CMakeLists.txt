add_executable(lc_hello_world lc_hello_world.cpp)
target_link_libraries(lc_hello_world skywing_core)

# NOTE (trb 2023/12/05) -- See hello_world for details
set(lc_hello_exe "$<TARGET_FILE:lc_hello_world>")

configure_file(run.sh.in run.sh @ONLY)
configure_file(generate_config_info.py generate_config_info.py @ONLY)

configure_file(call_executable.sh.in
  call_executable.generator_expression.sh.in
  @ONLY)
file(GENERATE
  OUTPUT call_executable.sh
  INPUT
  "${CMAKE_CURRENT_BINARY_DIR}/call_executable.generator_expression.sh.in")

configure_file(sbatch_script.sh.in
  sbatch_script.generator_expression.sh.in
  @ONLY)
file(GENERATE
  OUTPUT sbatch_script.sh
  INPUT "${CMAKE_CURRENT_BINARY_DIR}/sbatch_script.generator_expression.sh.in")
