add_executable(multiple_jobs multiple_jobs.cpp)
target_link_libraries(multiple_jobs PRIVATE skywing_core)

# NOTE (trb 2023/12/05) -- See hello_world for details
set(multiple_jobs_exe $<TARGET_FILE:multiple_jobs>)
configure_file(run.sh.in run.generator_expression.sh.in @ONLY)
file(GENERATE
  OUTPUT run.sh
  INPUT "${CMAKE_CURRENT_BINARY_DIR}/run.generator_expression.sh.in")
