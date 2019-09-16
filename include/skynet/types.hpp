#ifndef SKYNET_TYPES_HPP
#define SKYNET_TYPES_HPP

#include <cstdint>

namespace skynet
{
  /// The ID type for machines
  using MachineID = std::uint16_t;

  /// The ID type for jobs
  using JobID = std::uint16_t;

  /// The ID type for messages
  using MessageID = std::uint32_t;

  /// The ID type for tags
  using TagID = std::uint8_t;
} // namespace skynet

#endif // SKYNET_TYPES_HPP
