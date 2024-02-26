#ifndef SKYNET_TYPES_HPP
#define SKYNET_TYPES_HPP

#include "skywing_core/internal/utility/type_list.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace skywing {
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
  std::vector<std::byte>,
  bool,
  // TODO: std::vector<bool> is awful, but don't really want an exception either...
  std::vector<bool>>;

/// Variant version of the above
using PublishValueVariant = internal::ApplyTo<PublishValueTypeList, std::variant>;

namespace internal::detail {
// Can't use std::conditional_t because of At not working for 0 size packs
// but conditional_t requires both types to be well-formed
template<typename... Ts>
struct ValueOrTupleImpl {
  using Type = std::tuple<Ts...>;
};
template<typename T>
struct ValueOrTupleImpl<T> {
  using Type = T;
};
} // namespace internal::detail

/// Takes a parameter pack and either packs it into a tuple or
/// turns it into a single type
template<typename... Ts>
using ValueOrTuple = typename internal::detail::ValueOrTupleImpl<Ts...>::Type;

/// Address/port pair
using AddrPortPair = std::pair<std::string, std::uint16_t>;

/** \brief Wrapper for returning void values in various situations
 */
struct VoidWrapper {};

namespace internal {
/// Wraps void values in VoidWrappers or returns the type unmodified
template<typename T>
using WrapVoidValue = std::conditional_t<
  // don't care about cv-qualified void because those shouldn't really be a thing
  std::is_same_v<T, void>,
  VoidWrapper,
  T>;

/// Wraps void returning functions into returning VoidWrapper instead
template<typename Callable, typename... Args>
auto wrap_void_func(Callable&& c, Args&&... args) noexcept
  -> WrapVoidValue<decltype(::std::forward<Callable>(c)(::std::forward<Args>(args)...))>
{
  using RetType = WrapVoidValue<decltype(::std::forward<Callable>(c)(::std::forward<Args>(args)...))>;
  if constexpr (std::is_same_v<RetType, VoidWrapper>) {
    ::std::forward<Callable>(c)(::std::forward<Args>(args)...);
    return VoidWrapper{};
  }
  else {
    return ::std::forward<Callable>(c)(::std::forward<Args>(args)...);
  }
}

// Marker prepended to mark tags as publish tags
inline constexpr char publish_tag_marker = 'p';

// Marker prepended to mark tags as private tags
inline constexpr char private_tag_marker = 'x';

/// Structure for testing if a value is any of the supplied values
template<typename T, T... Values>
struct any_of {
  template<typename U>
  friend constexpr bool operator==(const U& comp, any_of) noexcept
  {
    return ((comp == Values) || ...);
  }

  template<typename U>
  friend constexpr bool operator==(any_of, const U& comp) noexcept
  {
    return comp == any_of{};
  }
};

// Checks if a tag name is bad
inline bool tag_name_okay(const std::string& tag) noexcept
{
  return !tag.empty() && (tag[0] == any_of<char, publish_tag_marker, private_tag_marker>{});
}
} // namespace internal
} // namespace skywing

// Hashing support
template<>
struct std::hash<skywing::AddrPortPair> {
  std::size_t operator()(const skywing::AddrPortPair& val) const noexcept
  {
    return std::hash<std::string>{}(val.first) ^ std::hash<std::uint16_t>{}(val.second);
  }
};

#endif // SKYNET_TYPES_HPP
