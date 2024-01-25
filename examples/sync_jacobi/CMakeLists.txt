add_executable(sync_jacobi sync_jacobi.cpp)
target_link_libraries(sync_jacobi PRIVATE skywing_core)

# NOTE (trb 2023/12/05) -- See hello_world for details
set(sync_jacobi_exe $<TARGET_FILE:sync_jacobi>)
configure_file(run.sh.in run.generator_expression.sh.in @ONLY)
file(GENERATE
  OUTPUT run.sh
  INPUT "${CMAKE_CURRENT_BINARY_DIR}/run.generator_expression.sh.in")
