# FIXME (trb 2024/01/03): I would recommend changing the naming and
# location of these files. Based on the comments in the meson.build
# file, all of these generated files live here for Meson-y reasons.
# CMake does not have the same limitations that the comments claim
# Meson has. These files could be integrated more naturally into the
# rest of the source, wherever would be appropriate.
#
# The CapnProto header is also not generated/used ideally. The header
# is generated and included as `#include "message_format.capnp.h"`.
# Though it's unlikely, this could easily collide with some other
# header in the include path. It might be worth relocating it so it
# could be included as "skywing_core/message_format.capnp.h", e.g.
# (though I'd personally maybe prefer "skywing/message_format.capnp.h"
# or "skywing/core/message_format.capnp.h").
capnp_generate_cpp(CAPNP_SRCS CAPNP_HDRS message_format.capnp)
target_sources(skywing_core
  PRIVATE
  "${CAPNP_SRCS}"
)
target_sources(skywing_core
  PUBLIC
  FILE_SET capnp
  TYPE HEADERS
  BASE_DIRS
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}"
  FILES
  "${CAPNP_HDRS}"
)
add_custom_target(capnproto-gen-srcs
  DEPENDS ${CAPNP_SRCS} ${CAPNP_HDRS})
add_dependencies(skywing_core capnproto-gen-srcs)

# FIXME (trb 2024/01/03): It's worth noting that C++20 has support for
# detecting endianness without CMake's help. Also, it is likely a bad
# choice to hard-code this into a header in case fat binaries are
# used. It might be better to use a (constexpr) function or inline
# variable:
#
#   #include <bits>
#   inline constexpr bool is_little_endian =
#     (std::endian::native == std::endian::little);
#   inline constexpr bool is_little_endian() {
#     return (std::endian::native == std::endian::little);
#   }
#
# Moreover, the compiler switch here could be implemented directly in
# the header, which might benefit developers. In particular, the
# switch would be in the C++ header rather than a build system file,
# improving readability.
if (CMAKE_CXX_BYTE_ORDER STREQUAL "LITTLE_ENDIAN")
  set(is_little_endian "true")
else ()
  set(is_little_endian "false")
endif ()

if (MSVC)
  set(endian_include "skywing_core/internal/endian_win.hpp")
else ()
  set(endian_include "skywing_core/internal/endian_posix.hpp")
endif ()

configure_file(
  endian.hpp.in
  "${CMAKE_CURRENT_BINARY_DIR}/endian.hpp"
  @ONLY)

# FIXME (trb 2024/01/03): This header just #define's one thing. I'd
# recommend either just adding this as a (PUBLIC) compiler define, or
# perhaps merging this with the endianness header, perhaps into a more
# general "skywing_config.hpp".
if (APPLE)
  set(no_sig_pipe "SO_NOSIGPIPE")
else ()
  # Otherwise assume this is valid until it becomes non-valid
  set(no_sig_pipe "MSG_NOSIGNAL")
endif ()

configure_file(
  socket_no_sigpipe.hpp.in
  "${CMAKE_CURRENT_BINARY_DIR}/socket_no_sigpipe.hpp"
  @ONLY
)

# Make sure we find all this stuff at build time.
target_include_directories(skywing_core
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
