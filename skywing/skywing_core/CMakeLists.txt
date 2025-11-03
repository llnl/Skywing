if (APPLE)
  target_sources(skywing_core
    PRIVATE
    internal/devices/socket_wrappers_osx.cpp)
elseif (LINUX OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_sources(skywing_core
    PRIVATE
    internal/devices/socket_wrappers_linux.cpp)
else ()
  message(FATAL_ERROR
    "Unsupported build target: ${CMAKE_SYSTEM_NAME}")
endif ()

target_sources(skywing_core
  PRIVATE
  # basic_manager_config.cpp
  internal/capn_proto_wrapper.cpp
  internal/devices/socket_communicator.cpp
  internal/manager_waiter_callables.cpp
  internal/message_creators.cpp
  internal/utility/network_conv.cpp
  neighbor_agent.cpp
  message_handler.cpp
  job.cpp
  manager.cpp
)

# Selectively add the endian header
if (MSVC)
  set(_endian_header internal/endian_win.hpp)
else ()
  set(_endian_header internal/endian_posix.hpp)
endif ()

# "Public" headers enumerated in a FILE_SET
target_sources(skywing_core
  PUBLIC
  FILE_SET core_api
  TYPE HEADERS
  FILES
  # basic_manager_config.hpp #<< "Not yet supported"
  enable_logging.hpp
  internal/capn_proto_wrapper.hpp
  internal/devices/socket_communicator.hpp
  "${_endian_header}"
  internal/manager_waiter_callables.hpp
  internal/message_creators.hpp
  internal/buffer.hpp
  internal/most_recent_buffer.hpp
  internal/queue_buffer.hpp
  internal/utility/algorithms.hpp
  internal/utility/logging.hpp
  internal/utility/mutex_guarded.hpp
  internal/utility/network_conv.hpp
  internal/utility/overload_set.hpp
  internal/utility/type_list.hpp
  job.hpp
  manager.hpp
  neighbor_agent.hpp
  message_handler.hpp
  skywing.hpp
  types.hpp
  waiter.hpp
  tag.hpp
  subscription.hpp
  buffer.hpp
)

# "Private" headers, enumerated to help IDEs
target_sources(skywing_core
  PRIVATE
  FILE_SET core_private
  TYPE HEADERS
  BASE_DIRS include
  FILES
  include/publish_value_handler.hpp
  include/socket_wrappers.hpp
)
