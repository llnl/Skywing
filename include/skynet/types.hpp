#ifndef SKYNET_TYPES_HPP
#define SKYNET_TYPES_HPP

#include "skynet/internal/utility/type_list.hpp"

#include <array>
#include <cstddef>
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
    std::vector<std::string>,
    std::vector<std::byte>
  >;

  /// Variant version of the above
  using PublishValueVariant = internal::ApplyTo<PublishValueTypeList, std::variant>;

  namespace internal
  {
    /// Structure for reporting reduce group building
    struct ReduceGroupNeighbors
    {
      // Having everything as an array is nice sometimes, but so is having named
      // members
      std::array<TagID, 3> tags;

      const TagID& parent() const noexcept { return tags[0]; }
      TagID& parent() noexcept { return tags[0]; }
      const TagID& left_child() const noexcept { return tags[1]; }
      TagID& left_child() noexcept { return tags[1]; }
      const TagID& right_child() const noexcept { return tags[2]; }
      TagID& right_child() noexcept { return tags[2]; }
    };

    // Marker prepended to mark tags as publish tags
    constexpr char publish_tag_marker = 'p';

    // Marker prepended to mark tags as begin for reduce groups
    constexpr char reduce_value_marker = 'r';

    // Marker preprended to mark tags as reduce group tags
    constexpr char reduce_group_marker = 'g';

    // Checks if a tag name is bad
    inline bool tag_name_okay(const std::string& tag) noexcept
    {
      return
        !tag.empty() &&
        (
          tag[0] == publish_tag_marker ||
          tag[0] == reduce_value_marker ||
          tag[0] == reduce_group_marker
        );
    }
  } // namespace skynet::internal
} // namespace skynet

#endif // SKYNET_TYPES_HPP
