#ifndef TAG_HPP
#define TAG_HPP

#include <compare>
#include <functional>
#include <string>
#include <vector>

namespace skywing::skywing_core {

  class AbstractTag {
  public:
    using DataTypeRef = std::uint8_t;
    using TagDataTypes = std::vector<DataTypeRef>;

    virtual ~AbstractTag() = default;
    virtual const std::string get_id() const = 0;
    virtual const TagDataTypes& get_expected_types() const = 0;
    virtual const std::size_t get_hash() const = 0;
  };

  class Tag;

  class TagTypeStrategy {
  public:
    using DataTypeRef = std::uint8_t;
    using TagDataTypes = std::vector<DataTypeRef>;

    virtual ~TagTypeStrategy() {}
    virtual TagDataTypes get_datatype_refs(Tag const& tag) const = 0;
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
  class Tag : public AbstractTag {
  public:
    Tag() = default;
    Tag(std::string id) :
      id_{std::move('p' + id)}
      , expected_types_()
    {}
    auto operator<=>(const Tag&) const = default;

    const std::string get_id() const override { return id_; }

    const std::size_t get_hash() const override { return std::hash<std::string>{}(get_id()); }

  private:
    std::string id_{};
    TagDataTypes expected_types_{};
  };

} // namespace skywing::skywing_core
#endif // TAG_HPP
