#ifndef SKYNET_INTERNAL_UTILITY_NETWORK_CONV_HPP
#define SKYNET_INTERNAL_UTILITY_NETWORK_CONV_HPP

#include "skynet_core/types.hpp"

#include <array>
#include <cstddef>

namespace skynet::internal {
/// Convert from an array of bytes from the network to a local value
NetworkSizeType from_network_bytes(const std::array<std::byte, sizeof(NetworkSizeType)>& data) noexcept;

/// Convert a value to an array of bytes from local data
std::array<std::byte, sizeof(NetworkSizeType)> to_network_bytes(const NetworkSizeType value) noexcept;
} // namespace skynet::internal

#endif // SKYNET_INTERNAL_UTILITY_NETWORK_CONV_HPP
