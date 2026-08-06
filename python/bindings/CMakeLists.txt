find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
find_package(pybind11 REQUIRED)

python_add_library(
  skywing_bind_interface
  MODULE
  WITH_SOABI
  skywing_bind_interface.cpp
)
target_link_libraries(skywing_bind_interface
  PUBLIC
  skywing_core
  skywing_math_interface
  skywing_mid
  pybind11::headers
)

install(TARGETS skywing_bind_interface
  LIBRARY DESTINATION .
)
