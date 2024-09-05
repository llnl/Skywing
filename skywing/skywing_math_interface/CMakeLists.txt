# Headers enumerated in a FILE_SET.
target_sources(skywing_math_interface
PUBLIC
FILE_SET math_api
TYPE HEADERS
FILES
machine_setup.hpp
fake_io.hpp
linear_system_driver.hpp
)
