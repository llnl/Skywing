add_executable(barbell_jacobi barbell_jacobi.cpp)
target_link_libraries(barbell_jacobi skywing_math_interface)

# NOTE (trb 2023/12/05) -- See hello_world for details
set(barbell_jacobi_exe $<TARGET_FILE:barbell_jacobi>)
configure_file(run.sh.in run.generator_expression.sh.in @ONLY)
file(GENERATE
  OUTPUT run.sh
  INPUT "${CMAKE_CURRENT_BINARY_DIR}/run.generator_expression.sh.in")
