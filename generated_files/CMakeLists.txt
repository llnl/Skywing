add_subdirectory(generated)

# TODO (trb 2023/12/04) -- Many TUs #include these things with the
# "generated/" prefix, so we add the binary dir corresponding to this
# directory to the include path. It might be good to reconsider this
# layout in the future, instead generating things directly into
# ${PROJECT_BINARY_DIR}/include/skywing or something of that nature,
# and adjusting the source file #includes accordingly.
target_include_directories(skywing_core
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
