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

  template<typename T>
  concept Vector = std::same_as<T, std::vector<typename T::value_type>>

  template<typename T>
  concept BasicPublishable = std::floating_point<T> || std::signed_integral<T> || std::unsigned_integral<T>;

  template<typename T>
  // concept Publishable = BasicPublishable<T> || Vector<T>
  concept Publishable = BasicPublishable<T>

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
  template<Publishable... Ts>
  class Tag 
  {
    public:
      using DataTypeRef = std::uint8_t;

      Tag() = default;
      Tag(std::string id) :
        id_{std::move('p' + id)}
        , expected_types_(set_expected_types<Ts...>())
      {}
      auto operator<=>(const Tag&) const = default;

      /** @brief Get the string TagID for this Tag. */
      const std::string get_id() const { return id_; }

      /** @brief Get a vector representing the one or more data types
       *  associated with this Subscription's Tag.
      */
      const std::vector<DataTypeRef>& get_expected_types() const { return expected_types_; }

      const std::size_t get_hash() const { return std::hash<std::string>{}(get_id()); }

    private:
      std::string id_{};
      std::vector<DataTypeRef> expected_types_{};

      template<Publishable... Ts>
      inline static void set_expected_types(Ts&&... ts)
      {
        (expected_types_.push_back(static_cast<DataTypeRef>(index_of<ts, PublishValueTypeList>)), ...);
      }
  };

} // namespace skywing::skywing_core
#endif // TAG_HPP
