add_executable(jacobi jacobi.cpp)
target_link_libraries(jacobi skywing_math_interface)

configure_file(config.cfg config.cfg COPYONLY)


# NOTE (trb 2023/12/05) -- Hack until I can change run.sh.in to use
# generator expressions directly. The "configure_file" rewrites
# run.sh.in at configure time, replacing "@hello_exe@" with the
# literal generator expression. The file(GENERATE) call rewrites
# _that_ file (at generation time) with the full path to the binary.
set(jacobi_exe $<TARGET_FILE:jacobi>)
configure_file(run.sh.in run.generator_expression.sh.in @ONLY)
file(GENERATE
  OUTPUT run.sh
  INPUT "${CMAKE_CURRENT_BINARY_DIR}/run.generator_expression.sh.in")