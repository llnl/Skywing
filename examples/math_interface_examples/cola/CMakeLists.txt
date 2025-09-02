add_executable(cola cola.cpp)
target_link_libraries(cola skywing_math_interface)

file(COPY
  data/
  DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data)

# NOTE (trb 2023/12/05) -- Hack until I can change run.sh.in to use
# generator expressions directly. The "configure_file" rewrites
# run.sh.in at configure time, replacing "@cola_exe@" with the
# literal generator expression. The file(GENERATE) call rewrites
# _that_ file (at generation time) with the full path to the binary.
set(cola_exe $<TARGET_FILE:cola>)
configure_file(run.sh.in run.generator_expression.sh.in @ONLY)
file(GENERATE
  OUTPUT run.sh
  INPUT "${CMAKE_CURRENT_BINARY_DIR}/run.generator_expression.sh.in")
