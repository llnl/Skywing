add_executable(admm_ls_driver admm_ls_driver.cpp)
target_link_libraries(admm_ls_driver skywing_math_interface)

file(COPY
  data/
  DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data)
file(COPY
  shared_data/
  DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/shared_data)

# NOTE (trb 2023/12/05) -- Hack until I can change run.sh.in to use
# generator expressions directly. The "configure_file" rewrites
# run.sh.in at configure time, replacing "@admm_ls_driver@" with the
# literal generator expression. The file(GENERATE) call rewrites
# _that_ file (at generation time) with the full path to the binary.
set(admm_ls_driver_exe $<TARGET_FILE:admm_ls_driver>)
configure_file(run.sh.in run.generator_expression.sh.in @ONLY)
file(GENERATE
  OUTPUT run.sh
  INPUT "${CMAKE_CURRENT_BINARY_DIR}/run.generator_expression.sh.in")
