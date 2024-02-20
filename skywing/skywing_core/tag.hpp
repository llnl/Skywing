#ifndef TAG_HPP
#define TAG_HPP

#include "skywing_core/internal/utility/type_list.hpp"
#include "skywing_core/types.hpp"

#include <compare>
#include <concepts>
#include <functional>
#include <string>
#include <vector>

namespace skywing::skywing_core {

using DataTypeRef = std::uint8_t;

template<typename T>
concept Vector = std::same_as<T, std::vector<typename T::value_type>>;

template<typename T, typename... U>
concept IsAnyOf = (std::same_as<T, U> || ...);

template<typename T>
concept BasicPublishable = IsAnyOf<
  T,
  float,
  double,
  std::int8_t,
  std::int16_t,
  std::int32_t,
  std::int64_t,
  std::uint8_t,
  std::uint16_t,
  std::uint32_t,
  std::uint64_t,
  std::string,
  bool>;

template<typename T>
concept Publishable = BasicPublishable<T>;

template<typename T>
concept HasToInt = requires(T t) {
  {
    t.to_int()
  } -> std::same_as<std::vector<DataTypeRef>>;
};

/** @brief A collective-global unique identifier for a publication stream.
 *
 * A Tag consists of (a) one or more data types that will be
 * published by this publication stream, each of which must be a
 * valid type in the PublishValueTypeList in skywing_core/types.hpp,
 * and (b) an id (ie a string) identifier.
 *
 * @tparam Ts Set of data types that will be sent with each
 * publication in the publication stream.
 */

class Tag {
public:
  Tag() = default;
  Tag(std::string id) : id_{std::move('p' + id)} {}
  Tag(std::string id, HasToInt auto expected_types) : Tag(id) { expected_types_ = expected_types.to_int(); }
  auto operator<=>(const Tag&) const = default;

  /** @brief Get the string TagID for this Tag. */
  const std::string get_id() const { return id_; }

  /** @brief Get a vector representing the one or more data types associated with this Subscription's Tag. */
  const std::vector<DataTypeRef>& get_expected_types() const { return expected_types_; }

private:
  std::string id_{};
  std::vector<DataTypeRef> expected_types_{};
};

struct hash {
  std::size_t operator()(const Tag& tag) const { return std::hash<std::string>{}(tag.get_id()); }
};

template<Publishable... Types>
class PublishDataTypes {
public:
  PublishDataTypes() = default;

  // template<typename... Types>
  std::vector<DataTypeRef> to_int()
  {
    std::vector<DataTypeRef> tmp{};
    std::apply(
      [&tmp]() {
        ((tmp.push_back(static_cast<DataTypeRef>(skywing::internal::index_of<Types, PublishValueTypeList>))), ...);
      },
      expected_types_);
    return tmp;
  }

private:
  std::tuple<Types...> expected_types_;
};

} // namespace skywing::skywing_core
#endif // TAG_HPP
