find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
find_package(pybind11 REQUIRED)

python_add_library(
  skywing_cpp_interface
  MODULE
  WITH_SOABI
  skywing_interface.cpp
)
target_link_libraries(skywing_cpp_interface
  PUBLIC
  skywing_core
  pybind11::headers
)

install(TARGETS skywing_cpp_interface
  LIBRARY DESTINATION .
)
