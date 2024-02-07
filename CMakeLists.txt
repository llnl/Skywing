cmake_minimum_required(VERSION 3.23.0)

project(Skywing
  VERSION 0.2
  DESCRIPTION
  "A high-reliability, real-time, decentralized platform for collaborative autonomy."
  HOMEPAGE_URL "https://github.com/LLNL/Skywing"
  LANGUAGES CXX)

# CMake modules
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# Options
option(SKYWING_BUILD_TESTS "Build tests" OFF)
option(SKYWING_BUILD_EXAMPLES "Build the example programs" OFF)
option(SKYWING_BUILD_LC_EXAMPLES "Build LC example programs" OFF)

option(SKYWING_DEVELOPER_BUILD
  "Build with developer flags enabled"
  OFF)
option(SKYWING_ENABLE_MEMCHECK
  "Build with address sanitizer (if compiler-supported)"
  ${SKYWING_DEVELOPER_BUILD})
option(SKYWING_WARNINGS_AS_ERRORS
  "Build with compiler warnings promoted to errors"
  ${SKYWING_DEVELOPER_BUILD})

option(SKYWING_EXTERNAL_CATCH2 "Search for and use an external Catch2" OFF)
option(SKYWING_EXTERNAL_GSL
  "Search for and use an external Guidelines Support Library (GSL)"
  OFF)
option(SKYWING_EXTERNAL_SPDLOG "Search for and use an external spdlog" OFF)

set(SKYWING_LOG_LEVEL "off"
  CACHE STRING
  "Logging level to enable")
string(TOUPPER "${SKYWING_LOG_LEVEL}" SKYWING_LOG_LEVEL_UC)

set(_VALID_LOG_LEVELS "OFF" "CRITICAL" "ERROR" "WARN" "INFO" "DEBUG" "TRACE")
if (NOT SKYWING_LOG_LEVEL_UC IN_LIST _VALID_LOG_LEVELS)
  message(FATAL_ERROR
    "Specified log level: \"${SKYWING_LOG_LEVEL}\".\n"
    "Valid choices: \"${_VALID_LOG_LEVELS}\"")
endif ()

# C++: Require at least C++17, and disallow compiler extensions.
if (NOT CMAKE_CXX_STANDARD
    OR CMAKE_CXX_STANDARD EQUAL 98
    OR CMAKE_CXX_STANDARD LESS 17)
  set(CMAKE_CXX_STANDARD 17
    CACHE STRING "C++ Standard to use."
    FORCE)
endif ()
set(CMAKE_CXX_STANDARD_REQUIRED ON
  CACHE BOOL "Force the C++ standard."
  FORCE)
set(CMAKE_CXX_EXTENSIONS OFF
  CACHE BOOL "Do not allow compiler extensions."
  FORCE)

# Handle developer options
if (SKYWING_DEVELOPER_BUILD)

  # Get compile_commands.json, useful for IDEs and other developer tools
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

  # Build the "all_verify_interface_header_sets" to check that every
  # header is buildable with no external dependencies. (It's sort of
  # the other side of the same coin as IWYU.)
  if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
    set(CMAKE_VERIFY_INTERFACE_HEADER_SETS ON CACHE BOOL "" FORCE)
  endif ()

  # Add warnings flags. Generally these are pretty portable amongst
  # the GNU-like compilers.
  if (CMAKE_CXX_COMPILER_ID MATCHES ".*[Cclang]"
      OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
      OR CMAKE_CXX_COMPILER_ID MATCHES "Intel.*")
    add_compile_options(-Wall -Wextra)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Intel.*")
      add_compile_options(-pedantic)
      add_compile_options(-fp-model=strict) # NOTE: May impact performance!
    else ()
      add_compile_options(-Wpedantic)
    endif ()
    if (SKYWING_ENABLE_MEMCHECK)
      add_compile_options(
        -fno-omit-frame-pointer
        -fsanitize=address)
      add_link_options(
        -fno-omit-frame-pointer
        -fsanitize=address)
    endif ()

    # Promote warnings to errors. This should generally not be done
    # for user-facing builds but rather only for developers and CI.
    if (SKYWING_WARNINGS_AS_ERRORS)
      add_compile_options(-Werror)
    endif ()
  endif ()
endif (SKYWING_DEVELOPER_BUILD)

# Possible submodule dependencies
if (SKYWING_BUILD_TESTS AND SKYWING_EXTERNAL_CATCH2)
  find_package(Catch2 2.9.0 REQUIRED CONFIG)
elseif (SKYWING_BUILD_TESTS)
  add_subdirectory(subprojects/catch2/Catch2)
endif ()

if (SKYWING_EXTERNAL_SPDLOG)
  find_package(spdlog 1.0 REQUIRED CONFIG)
else ()
  # NOTE (trb 2024/01/03): Skywing uses spdlog in (public) headers, so
  # it must ensure that the spdlog artifacts get installed as well.
  # This will install spdlog to the same prefix as Skywing. This could
  # be changed if necessary.
  set(SPDLOG_INSTALL ON CACHE BOOL "Install spdlog" FORCE)
  add_subdirectory(subprojects/spdlog)
endif ()

# "Guidelines Support Library" (not GNU Scientific Library, as it has
# been for like 20 years, but who's counting).
if (SKYWING_EXTERNAL_GSL)
  find_package(gsl MODULE REQUIRED)
else ()
  add_subdirectory(subprojects/gsl)
endif ()

# External dependencies
find_package(Threads REQUIRED)
find_package(CapnProto 1.0 REQUIRED CONFIG)

# Build libraries
add_library(skywing_core)
target_sources(skywing_core
  PUBLIC
  FILE_SET core_api
  TYPE HEADERS
  BASE_DIRS skywing)
target_link_libraries(skywing_core
  PUBLIC
  Threads::Threads
  CapnProto::capnp
  spdlog::spdlog
  ${gsl_LIBS}
)
target_include_directories(skywing_core
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/skywing>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/subprojects/gsl/include>)
target_compile_definitions(skywing_core
  PUBLIC
  SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_${SKYWING_LOG_LEVEL_UC}
)

if (NOT SKYWING_EXTERNAL_SPDLOG)
  # We want to ignore warnings from spdlog, so explicitly mark its
  # include directory as SYSTEM. This maybe isn't ideal, but seems to
  # give the desired behavior.
  target_include_directories(skywing_core
    SYSTEM
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/subprojects/spdlog/include>)
endif ()

add_library(skywing_mid INTERFACE)
target_sources(skywing_mid
  PUBLIC
  FILE_SET mid_api
  TYPE HEADERS
  BASE_DIRS skywing)
target_link_libraries(skywing_mid
  INTERFACE
  skywing_core)

# Setup the C++ standard. Should be at least C++17.
if (CMAKE_CXX_STANDARD)
  target_compile_features(skywing_core INTERFACE cxx_std_${CMAKE_CXX_STANDARD})
  target_compile_features(skywing_mid INTERFACE cxx_std_${CMAKE_CXX_STANDARD})
else ()
  target_compile_features(skywing_core INTERFACE cxx_std_17)
  target_compile_features(skywing_mid INTERFACE cxx_std_17)
endif ()

# Add sources
add_subdirectory(generated_files)
add_subdirectory(skywing/skywing_core)
add_subdirectory(skywing/skywing_mid)

# Helper targets for consistent view of linking
add_library(skywing::core ALIAS skywing_core)
add_library(skywing::mid ALIAS skywing_mid)

add_library(skywing::skywing IMPORTED INTERFACE)
target_link_libraries(skywing::skywing INTERFACE skywing::mid)
target_link_libraries(skywing::skywing INTERFACE skywing::core)

set_target_properties(skywing_core PROPERTIES
  EXPORT_NAME core)
set_target_properties(skywing_mid PROPERTIES
  EXPORT_NAME mid)

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
  COMPATIBILITY SameMinorVersion)

configure_package_config_file(
  "cmake/SkywingConfig.cmake.in"
  "${CMAKE_BINARY_DIR}/SkywingConfig.install.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_DIR}
  PATH_VARS INCLUDE_INSTALL_DIRS LIB_INSTALL_DIR)

install(TARGETS skywing_core skywing_mid
  EXPORT SkywingTargets
  FILE_SET capnp DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/skywing"
  FILE_SET core_api DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/skywing"
  FILE_SET mid_api DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/skywing")
install(FILES
  "${PROJECT_BINARY_DIR}/SkywingConfig.install.cmake"
  RENAME "SkywingConfig.cmake"
  DESTINATION ${CMAKE_INSTALL_DIR})
install(FILES
  "${PROJECT_BINARY_DIR}/SkywingConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_DIR})
install(EXPORT SkywingTargets
  DESTINATION ${CMAKE_INSTALL_DIR}
  NAMESPACE skywing::)
