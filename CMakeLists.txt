cmake_minimum_required(VERSION 3.25.0)
project(Skywing
  VERSION 0.2
  DESCRIPTION
  "A high-reliability, real-time, decentralized platform for collaborative autonomy."
  HOMEPAGE_URL "https://github.com/LLNL/Skywing"
  LANGUAGES CXX
)

# Setup some things related to the package registry and CI
if (DEFINED ENV{CI})
  set(CMAKE_EXPORT_PACKAGE_REGISTRY OFF)
  set(CMAKE_FIND_USE_PACKAGE_REGISTRY OFF)
endif ()

# CMake modules
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# Options
option(SKYWING_BUILD_TESTS
  "Build tests"
  OFF
)
option(SKYWING_BUILD_EXAMPLES
  "Build the example programs"
  OFF
)
option(SKYWING_BUILD_LC_EXAMPLES
  "Build LC example programs"
  OFF
)
option(SKYWING_DEVELOPER_BUILD
  "Build with developer flags enabled"
  OFF
)
option(SKYWING_ENABLE_MEMCHECK
  "Build with address sanitizer (if compiler-supported)"
  ${SKYWING_DEVELOPER_BUILD}
)
option(SKYWING_WARNINGS_AS_ERRORS
  "Build with compiler warnings promoted to errors"
  ${SKYWING_DEVELOPER_BUILD}
)
option(SKYWING_USE_EIGEN
  "Include Eigen library for skywing_math_interface"
  OFF
)
set(SKYWING_LOG_LEVEL "off" CACHE STRING
  "Logging level to enable"
)

# Check the log level.
string(TOUPPER "${SKYWING_LOG_LEVEL}" SKYWING_LOG_LEVEL_UC)
set(_VALID_LOG_LEVELS "OFF" "CRITICAL" "ERROR" "WARN" "INFO" "DEBUG" "TRACE")
if (NOT SKYWING_LOG_LEVEL_UC IN_LIST _VALID_LOG_LEVELS)
  message(FATAL_ERROR
    "Specified log level: \"${SKYWING_LOG_LEVEL}\".
Valid choices: \"${_VALID_LOG_LEVELS}\"")
endif ()

# Dependencies

find_package(Threads REQUIRED)

## External dependencies
include(FetchContent)
macro(report_deps)
  foreach(PKG ${ARGN})
    if (${PKG}_FOUND)
      message(STATUS "Found ${PKG}: ${${PKG}_DIR}")
      message(STATUS "${PKG} version: ${${PKG}_VERSION}")
    else ()
      message(STATUS "Building ${PKG} with FetchContent")
    endif ()
  endforeach ()
endmacro()

FetchContent_Declare(
  spdlog
  SYSTEM
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG 27cb4c76708608465c413f6d0e6b8d99a4d84302 # v1.14.1
  FIND_PACKAGE_ARGS 1.0 CONFIG
)

FetchContent_Declare(
  CapnProto
  SYSTEM
  GIT_REPOSITORY https://github.com/capnproto/capnproto
  GIT_TAG b34ec28cceaf15b1082b74b50f03f770873c3636 # v1.1.0
  FIND_PACKAGE_ARGS 1.0 CONFIG
)

# Disable the annoying 'check' target in CapnProto
set(BUILD_TESTING OFF
  CACHE INTERNAL
  "Build testing infrastructure"
)
set(SPDLOG_INSTALL ON
  CACHE INTERNAL
  "Install spdlog"
)
FetchContent_MakeAvailable(spdlog CapnProto)
report_deps(spdlog CapnProto)
# Reset testing flag to whatever Skywing wants
set(BUILD_TESTING ${SKYWING_BUILD_TESTS}
  CACHE INTERNAL
  "Build testing infrastructure"
)

if (SKYWING_USE_EIGEN)
  set(EIGEN_BUILD_TESTING OFF)
  set(EIGEN_BUILD_CMAKE_PACKAGE ON)

  # Opting for >=3.0 is a guess. Anyone know the actual minimum requirement?
  FetchContent_Declare(
    Eigen3
    SYSTEM
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen
    GIT_TAG 68f4e58cfacc686583d16cff90361f0b43bc2c1b # '3.4' branch on 04/21/2025
    FIND_PACKAGE_ARGS 3.0 CONFIG
  )
  FetchContent_MakeAvailable(Eigen3)
  report_deps(Eigen3)
endif ()

# C++: Require at least C++20, and disallow compiler extensions.
if (NOT CMAKE_CXX_STANDARD
    OR CMAKE_CXX_STANDARD EQUAL 98
    OR CMAKE_CXX_STANDARD LESS 20)
  set(CMAKE_CXX_STANDARD 20 CACHE STRING
    "C++ Standard to use."
    FORCE)
endif ()
set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL
  "Force the C++ standard."
  FORCE)

# Handle developer options
add_library(_skywing_dev_opts INTERFACE)
if (SKYWING_DEVELOPER_BUILD)
  # Convenience for some IDE integration
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

  # Developer checks for header independence
  if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
    set(CMAKE_VERIFY_INTERFACE_HEADER_SETS ON CACHE BOOL "" FORCE)
  endif ()

  if (CMAKE_CXX_COMPILER_ID MATCHES ".*[Cclang]"
      OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
      OR CMAKE_CXX_COMPILER_ID MATCHES "Intel.*")

    # Intel has some unique requirements.
    if (CMAKE_CXX_COMPILER_ID MATCHES "Intel.*")
      set(SKYWING_USING_INTEL ON)
    endif ()

    target_compile_options(
      _skywing_dev_opts
      INTERFACE

      # Warning flags DO NOT PROPAGATE downstream.
      $<BUILD_INTERFACE:-Wall>
      $<BUILD_INTERFACE:-Wextra>
      $<BUILD_INTERFACE:$<IF:$<BOOL:${SKYWING_USING_INTEL}>,-pedantic,-Wpedantic>>
      $<BUILD_INTERFACE:$<$<AND:$<BOOL:${SKYWING_WARNINGS_AS_ERRORS}>,$<COMPILE_LANGUAGE:CXX>>:-Werror>>

      # This is somewhere in between.
      $<$<BOOL:${SKYWING_USING_INTEL}>:-fp-model=strict>

      # Memcheck flags should probably propagate? I'm on the fence.
      $<$<BOOL:${SKYWING_ENABLE_MEMCHECK}>:-fno-omit-frame-pointer>
      $<$<BOOL:${SKYWING_ENABLE_MEMCHECK}>:-fsanitize=address>

    )
    target_link_options(
      _skywing_dev_opts
      INTERFACE
      $<$<BOOL:${SKYWING_ENABLE_MEMCHECK}>:-fno-omit-frame-pointer>
      $<$<BOOL:${SKYWING_ENABLE_MEMCHECK}>:-fsanitize=address>
    )
  endif ()
endif ()

# Build libraries
add_library(skywing_core)
target_sources(skywing_core
  PUBLIC
  FILE_SET core_api
  TYPE HEADERS
  BASE_DIRS skywing
)
target_include_directories(skywing_core
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/skywing>
)
target_link_libraries(skywing_core
  PUBLIC
  Threads::Threads
  CapnProto::capnp
  spdlog::spdlog
  _skywing_dev_opts
)
target_compile_definitions(skywing_core
  PUBLIC
  SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_${SKYWING_LOG_LEVEL_UC}
)

add_library(skywing_mid INTERFACE)
target_sources(skywing_mid
  PUBLIC
  FILE_SET mid_api
  TYPE HEADERS
  BASE_DIRS skywing
)
target_link_libraries(skywing_mid
  INTERFACE
  skywing_core
)

add_library(skywing_math_interface INTERFACE)
target_sources(skywing_math_interface
  PUBLIC
  FILE_SET math_api
  TYPE HEADERS
  BASE_DIRS skywing
)
target_link_libraries(skywing_math_interface
  INTERFACE
  skywing_mid
)

# Conditionally link Eigen to skywing_math_interface
if (TARGET Eigen3::Eigen)
  target_link_libraries(skywing_math_interface
    INTERFACE
    Eigen3::Eigen
  )
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
if (SKBUILD)
  add_subdirectory(python/bindings)
endif ()

# Helper targets for consistent view of linking
add_library(skywing::core ALIAS skywing_core)
add_library(skywing::mid ALIAS skywing_mid)
add_library(skywing::math_interface ALIAS skywing_math_interface)
add_library(skywing::skywing IMPORTED INTERFACE)
target_link_libraries(skywing::skywing
  INTERFACE
  skywing::core
  skywing::mid
  skywing::math_interface)
set_target_properties(skywing_core
  PROPERTIES
  EXPORT_NAME core
  CXX_EXTENSIONS OFF
)
set_target_properties(skywing_mid
  PROPERTIES
  EXPORT_NAME mid
  CXX_EXTENSIONS OFF
)
set_target_properties(skywing_math_interface
  PROPERTIES
  EXPORT_NAME math_interface
  CXX_EXTENSIONS OFF
)

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
  NO_SET_AND_CHECK_MACRO
)

install(
  TARGETS skywing_core skywing_mid skywing_math_interface _skywing_dev_opts
  EXPORT SkywingTargets
  FILE_SET capnp DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/skywing"
  FILE_SET core_api DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/skywing"
  FILE_SET mid_api DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/skywing"
  FILE_SET math_api DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/skywing"
)

install(
  FILES "${PROJECT_BINARY_DIR}/SkywingConfig.install.cmake"
  RENAME "SkywingConfig.cmake"
  DESTINATION ${CMAKE_INSTALL_DIR}
)
install(
  FILES "${PROJECT_BINARY_DIR}/SkywingConfigVersion.cmake"
  DESTINATION ${CMAKE_INSTALL_DIR}
)
install(
  EXPORT SkywingTargets
  DESTINATION ${CMAKE_INSTALL_DIR}
  NAMESPACE skywing::
)
