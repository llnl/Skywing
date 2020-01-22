#ifndef SKYNET_TYPES_HPP
#define SKYNET_TYPES_HPP

#include "skynet_core/internal/utility/type_list.hpp"

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

  /// The type used for disconnection notifications in reduce groups
  using ReductionDisconnectID = std::uint64_t;

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

  /// A type indicating that a reduce did not produce a result intentionally
  /// (i.e., that it is not the root of the reduce tree)
  struct ReduceNoValue{};

  /// A type indicating that a reduce failed due to a disconnection
  struct ReduceDisconnection{};

  /** \brief The return result for a normal reduce operation.
   *
   * As a reduce operation can either not produce a value because it isn't
   * the root or not produce a value because of a connection error, can't
   * just return an optional.
   */
  template<typename T>
  class ReduceResult
  {
  public:
    constexpr ReduceResult(ReduceNoValue) noexcept
      : var_{ReduceNoValue{}}
    {}
    constexpr ReduceResult(ReduceDisconnection) noexcept
      : var_{ReduceDisconnection{}}
    {}
    constexpr ReduceResult(T value) noexcept
      : var_{std::move(value)}
    {}

    /** \brief Returns true if an error occurred
     */
    constexpr bool error_occurred() const noexcept
    {
      return std::holds_alternative<ReduceDisconnection>(var_);
    }

    /** \brief Returns true if the variant holds a value
     */
    constexpr bool has_value() const noexcept
    {
      return std::holds_alternative<T>(var_);
    }

    /** \brief Returns the value held
     *
     * \pre obj.has_value() == true
     */
    constexpr const T& value() const & noexcept
    {
      assert(has_value());
      return *std::get_if<T>(&var_);
    }
    constexpr T value() && noexcept
    {
      assert(has_value());
      return *std::get_if<T>(&var_);
    }
    constexpr const T& operator*() const & noexcept
    {
      return value();
    }
    constexpr T operator*() && noexcept
    {
      return value();
    }

  private:
    std::variant<ReduceNoValue, ReduceDisconnection, T> var_;
  };

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
