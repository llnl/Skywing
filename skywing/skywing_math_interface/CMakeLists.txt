# Headers enumerated in a FILE_SET.
target_sources(skywing_math_interface
    PUBLIC
    FILE_SET math_api
    TYPE HEADERS
    FILES
    machine_setup.hpp
    eigen_helpers/eigen_helpers.hpp
    linear_system_driver.hpp
)

target_link_libraries(skywing_math_interface INTERFACE skywing_mid)
