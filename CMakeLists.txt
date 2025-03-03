cmake_minimum_required(VERSION 3.23.0)
project(Skywing VERSION 0.2 DESCRIPTION "A high-reliability, real-time, decentralized platform for collaborative autonomy." HOMEPAGE_URL "https://github.com/LLNL/Skywing" LANGUAGES CXX)

# CMake modules
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# Options
option(SKYWING_BUILD_TESTS "Build tests" OFF)
option(SKYWING_BUILD_EXAMPLES "Build the example programs" OFF)
option(SKYWING_BUILD_LC_EXAMPLES "Build LC example programs" OFF)
option(SKYWING_DEVELOPER_BUILD "Build with developer flags enabled" OFF)
option(SKYWING_ENABLE_MEMCHECK "Build with address sanitizer (if compiler-supported)" ${SKYWING_DEVELOPER_BUILD})
option(SKYWING_WARNINGS_AS_ERRORS "Build with compiler warnings promoted to errors" ${SKYWING_DEVELOPER_BUILD})
option(SKYWING_EXTERNAL_CATCH2 "Search for and use an external Catch2" OFF)
option(SKYWING_EXTERNAL_SPDLOG "Search for and use an external spdlog" OFF)
option(SKYWING_USE_EIGEN_MATH_EXAMPLES "Include Eigen library for skywing_math_interface" OFF)

set(SKYWING_LOG_LEVEL "off" CACHE STRING "Logging level to enable")
string(TOUPPER "${SKYWING_LOG_LEVEL}" SKYWING_LOG_LEVEL_UC)
set(_VALID_LOG_LEVELS "OFF" "CRITICAL" "ERROR" "WARN" "INFO" "DEBUG" "TRACE")
if (NOT SKYWING_LOG_LEVEL_UC IN_LIST _VALID_LOG_LEVELS)
    message(FATAL_ERROR "Specified log level: \"${SKYWING_LOG_LEVEL}\".\nValid choices: \"${_VALID_LOG_LEVELS}\"")
endif ()

# C++: Require at least C++20, and disallow compiler extensions.
if (NOT CMAKE_CXX_STANDARD OR CMAKE_CXX_STANDARD EQUAL 98 OR CMAKE_CXX_STANDARD LESS 20)
    set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ Standard to use." FORCE)
endif ()
set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "Force the C++ standard." FORCE)
set(CMAKE_CXX_EXTENSIONS OFF CACHE BOOL "Do not allow compiler extensions." FORCE)

# Handle developer options
if (SKYWING_DEVELOPER_BUILD)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
        set(CMAKE_VERIFY_INTERFACE_HEADER_SETS ON CACHE BOOL "" FORCE)
    endif ()
    if (CMAKE_CXX_COMPILER_ID MATCHES ".*[Cclang]" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Intel.*")
        add_compile_options(-Wall -Wextra)
        if (CMAKE_CXX_COMPILER_ID MATCHES "Intel.*")
            add_compile_options(-pedantic)
            add_compile_options(-fp-model=strict)
        else ()
            add_compile_options(-Wpedantic)
        endif ()
        if (SKYWING_ENABLE_MEMCHECK)
            add_compile_options(-fno-omit-frame-pointer -fsanitize=address)
            add_link_options(-fno-omit-frame-pointer -fsanitize=address)
        endif ()
        if (SKYWING_WARNINGS_AS_ERRORS)
            #add_compile_options(-Werror)
            add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-Werror>)

        endif ()
    endif ()
endif ()

# Possible submodule dependencies
if (SKYWING_BUILD_TESTS AND SKYWING_EXTERNAL_CATCH2)
    find_package(Catch2 3.0.0 REQUIRED CONFIG)
    message(STATUS "Using external Catch2: ${Catch2_DIR}")
    message(STATUS "Using Catch2 Version: ${Catch2_VERSION}")
elseif (SKYWING_BUILD_TESTS)
    add_subdirectory(subprojects/catch2/Catch2)
    message(STATUS "Using Catch2 from submodule.")
endif ()

if (SKYWING_EXTERNAL_SPDLOG)
    find_package(spdlog 1.0 REQUIRED CONFIG)
else ()
    set(SPDLOG_INSTALL ON CACHE BOOL "Install spdlog" FORCE)
    add_subdirectory(subprojects/spdlog)
endif ()

# External dependencies
find_package(Threads REQUIRED)
find_package(CapnProto 1.0 REQUIRED CONFIG)

# Conditionally find Eigen
if (SKYWING_USE_EIGEN_MATH_EXAMPLES)
    find_package(Eigen3 REQUIRED CONFIG)
    if(Eigen3_FOUND)
        message(STATUS "Found Eigen3: ${EIGEN3_INCLUDE_DIR}")
    else()
        message(FATAL_ERROR "Eigen3 not found. Please install Eigen3.")
    endif()
endif ()

# Build libraries
add_library(skywing_core)
target_sources(skywing_core PUBLIC FILE_SET core_api TYPE HEADERS BASE_DIRS skywing)
target_include_directories(skywing_core PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/skywing>)
target_link_libraries(skywing_core PUBLIC Threads::Threads CapnProto::capnp spdlog::spdlog)
target_compile_definitions(skywing_core PUBLIC SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_${SKYWING_LOG_LEVEL_UC})

if (NOT SKYWING_EXTERNAL_SPDLOG)
    target_include_directories(skywing_core SYSTEM PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/subprojects/spdlog/include>)
endif ()

add_library(skywing_mid INTERFACE)
target_sources(skywing_mid PUBLIC FILE_SET mid_api TYPE HEADERS BASE_DIRS skywing)
target_link_libraries(skywing_mid INTERFACE skywing_core)

add_library(skywing_math_interface INTERFACE)
target_sources(skywing_math_interface PUBLIC FILE_SET math_api TYPE HEADERS BASE_DIRS skywing)
target_link_libraries(skywing_math_interface INTERFACE skywing_mid)

# Conditionally link Eigen to skywing_math_interface
if (SKYWING_USE_EIGEN_MATH_EXAMPLES)
    target_link_libraries(skywing_math_interface INTERFACE Eigen3::Eigen)
endif ()


# Setup the C++ standard. Should be at least C++20.
if (CMAKE_CXX_STANDARD)
    target_compile_features(skywing_core INTERFACE cxx_std_${CMAKE_CXX_STANDARD})
    target_compile_features(skywing_mid INTERFACE cxx_std_${CMAKE_CXX_STANDARD})
    target_compile_features(skywing_math_interface INTERFACE cxx_std_${CMAKE_CXX_STANDARD})
else ()
    target_compile_features(skywing_core INTERFACE cxx_std_20)
    target_compile_features(skywing_mid INTERFACE cxx_std_20)
    target_compile_features(skywing_math_interface INTERFACE cxx_std_20)
endif ()

# Add sources
add_subdirectory(generated_files)
add_subdirectory(skywing/skywing_core)
add_subdirectory(skywing/skywing_mid)
add_subdirectory(skywing/skywing_math_interface)

# Helper targets for consistent view of linking
add_library(skywing::core ALIAS skywing_core)
add_library(skywing::mid ALIAS skywing_mid)
add_library(skywing::math_interface ALIAS skywing_math_interface)
add_library(skywing::skywing IMPORTED INTERFACE)
target_link_libraries(skywing::skywing INTERFACE skywing::mid)
target_link_libraries(skywing::skywing INTERFACE skywing::core)
target_link_libraries(skywing::skywing INTERFACE skywing::math_interface)

set_target_properties(skywing_core PROPERTIES EXPORT_NAME core)
set_target_properties(skywing_mid PROPERTIES EXPORT_NAME mid)
set_target_properties(skywing_math_interface PROPERTIES EXPORT_NAME math_interface)

# Examples
if (SKYWING_BUILD_EXAMPLES OR SKYWING_BUILD_LC_EXAMPLES)
    add_subdirectory(examples)
endif ()

# Testing
if (SKYWING_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif ()

# Install target
if (CMAKE_INSTALL_PREFIX)
    message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
endif ()

set(INCLUDE_INSTALL_DIRS ${CMAKE_INSTALL_INCLUDEDIR})
set(LIB_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR})
set(CMAKE_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/skywing)

write_basic_package_version_file(
    "${CMAKE_BINARY_DIR}/SkywingConfigVersion.cmake"
    COMPATIBILITY SameMinorVersion
)

configure_package_config_file(
    "cmake/SkywingConfig.cmake.in"
    "${CMAKE_BINARY_DIR}/SkywingConfig.install.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_DIR}
    PATH_VARS INCLUDE_INSTALL_DIRS LIB_INSTALL_DIR
)

install(TARGETS skywing_core skywing_mid EXPORT SkywingTargets
    FILE_SET capnp DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/skywing"
    FILE_SET core_api DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/skywing"
    FILE_SET mid_api DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/skywing"
    FILE_SET math_api DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/skywing"
)

install(FILES "${PROJECT_BINARY_DIR}/SkywingConfig.install.cmake" RENAME "SkywingConfig.cmake" DESTINATION ${CMAKE_INSTALL_DIR})
install(FILES "${PROJECT_BINARY_DIR}/SkywingConfigVersion.cmake" DESTINATION ${CMAKE_INSTALL_DIR})
install(EXPORT SkywingTargets DESTINATION ${CMAKE_INSTALL_DIR} NAMESPACE skywing::)
