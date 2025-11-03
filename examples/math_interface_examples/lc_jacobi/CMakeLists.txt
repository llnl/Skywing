add_executable(lc_jacobi lc_jacobi.cpp)
target_link_libraries(lc_jacobi skywing_math_interface)
configure_file(run.py run.py @ONLY)
configure_file(generate_config_info.py generate_config_info.py @ONLY)
