add_executable(async_iterative asynchronous_iterative.cpp)
target_link_libraries(async_iterative PRIVATE skywing_core)

# NOTE (trb 2023/12/05) -- See hello_world for details
set(exe $<TARGET_FILE:async_iterative>)
configure_file(run.sh.in run.generator_expression.sh.in @ONLY)
file(GENERATE
  OUTPUT run.sh
  INPUT "${CMAKE_CURRENT_BINARY_DIR}/run.generator_expression.sh.in")

# NOTE (trb 2023/12/05) -- This could probably just be file(COPY) but
# this should be equivalent.
configure_file(config.cfg config.cfg @ONLY)
