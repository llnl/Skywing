add_executable(async_jacobi async_jacobi.cpp)
target_link_libraries(async_jacobi PRIVATE skywing_core)

# NOTE (trb 2023/12/05) -- See hello_world for details
set(async_jacobi_exe $<TARGET_FILE:async_jacobi>)
configure_file(run.sh.in run.generator_expression.sh.in @ONLY)
file(GENERATE
  OUTPUT run.sh
  INPUT "${CMAKE_CURRENT_BINARY_DIR}/run.generator_expression.sh.in")
