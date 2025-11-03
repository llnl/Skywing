cmake_minimum_required(VERSION 3.23.0)
project(Skywing_CMake_Example
  VERSION "0.0.1"
  DESCRIPTION "Example App built on Skywing framework"
  LANGUAGES CXX)

# This handles all dependencies of Skywing, too.
find_package(Skywing REQUIRED)

set(example_names ex1 ex2 ex3 ex4)
foreach (name ${example_names})
  # Build the executable for this example
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name} PRIVATE skywing::skywing)

  # Generate a run script with a hard-coded executable name
  file(GENERATE
    OUTPUT run_${name}.sh
    INPUT run_${name}.sh.in
    FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
endforeach()

file(GENERATE
  OUTPUT plot_results_ex4.py
  INPUT plot_results_ex4.py
  FILE_PERMISSIONS OWNER_READ OWNER_WRITE)
