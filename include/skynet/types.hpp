#ifndef SKYNET_TYPES_HPP
#define SKYNET_TYPES_HPP

#include "skynet/internal/utility/type_list.hpp"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace skynet
{
  /// The ID type for machines
  using MachineID = std::string;

  /// The ID type for jobs
  using JobID = std::string;

  /// The ID type for message versions.
  using VersionID = std::uint32_t;

  /// The ID type for tags
  using TagID = std::string;

  /// The type used for communicating message sizes over the network
  using NetworkSizeType = std::uint32_t;

  /// A typelist of all the types that can be published
  using PublishValueTypeList = internal::TypeList<
    float,
    std::vector<float>,
    double,
    std::vector<double>,
    std::int8_t,
    std::vector<std::int8_t>,
    std::int16_t,
    std::vector<std::int16_t>,
    std::int32_t,
    std::vector<std::int32_t>,
    std::int64_t,
    std::vector<std::int64_t>,
    std::uint8_t,
    std::vector<std::uint8_t>,
    std::uint16_t,
    std::vector<std::uint16_t>,
    std::uint32_t,
    std::vector<std::uint32_t>,
    std::uint64_t,
    std::vector<std::uint64_t>,
    std::string,
    std::vector<std::string>
  >;

  /// Variant version of the above
  using PublishValueVariant = internal::ApplyTo<PublishValueTypeList, std::variant>;
} // namespace skynet

#endif // SKYNET_TYPES_HPP
