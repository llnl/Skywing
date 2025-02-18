#ifndef SKYWING_SKYWING_CORE_TAG_HPP
#define SKYWING_SKYWING_CORE_TAG_HPP

#include <compare>
#include <concepts>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

#include "skywing_core/internal/utility/type_list.hpp"
#include "skywing_core/types.hpp"

namespace skywing
{

class AbstractTag;

template <typename... Ts>
concept IsTag = (std::is_base_of<AbstractTag, Ts>::value && ...);

template <typename T, typename... U>
concept IsAnyOf = (std::same_as<T, U> || ...);

template <typename T>
concept Publishable = IsAnyOf<T,
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
                              std::vector<bool>>;

/**
 * @brief Base class for concrete Tag class.
 *
 * @details
 * This abstract class defines an interface for the Tag class.
 * It is not to be extended for different Tag types but just
 * so the Tag class template can be stored as a single type in std::vector.
 */
class AbstractTag
{
public:
    using DataTypeRef = std::uint8_t;
    virtual ~AbstractTag() = default;
    [[nodiscard]] auto operator<=>(const AbstractTag&) const = default;
    virtual std::string id() const = 0;
    virtual std::vector<DataTypeRef> get_expected_types() const = 0;
    std::unique_ptr<AbstractTag> clone() const
    {
        return std::unique_ptr<AbstractTag>{this->do_clone()};
    }

private:
    virtual AbstractTag* do_clone() const = 0;
};

/**
 * @brief A collective-global unique identifier for a publication stream.
 * @tparam Types Set of data types that will be sent with each
 * publication in the publication stream.
 *
 * @details
 * A Tag consists of (a) one or more data types that will be
 * published by this publication stream, each of which must be a
 * valid type in the PublishValueTypeList in skywing_core/types.hpp,
 * and (b) an id (ie a string) identifier.
 *
 */
template <Publishable... Types>
class Tag final : public AbstractTag
{
public:
    using DataTypeRef = std::uint8_t;
    using ValueType = ValueOrTuple<Types...>;

    /**
     * Constructor to create a Tag.
     *
     * @param id a unique string id associated with stream of data to be
     * published.
     */
    Tag(std::string id) : id_{std::move(id)}
    {
        (expected_types_.push_back(static_cast<DataTypeRef>(
             skywing::internal::index_of<Types, PublishValueTypeList>)),
         ...);
    }

    /** @brief Compiler generated comparison operators.
     *
     * @param rhs right-hand side Tag object to be compared.
     *
     * @return Compiler deduces std::weak_ordering, std::strong_ordering,
     * or std::partial_ordering depending on the member variables of the class.
     */
    [[nodiscard]] auto operator<=>(const Tag& rhs) const = default;

    /** @brief Get the string TagID for this Tag.
     */
    std::string id() const final { return id_; }

    /** @brief Get a vector representing the one or more data types
     * associated with this Tag.
     */
    std::vector<DataTypeRef> get_expected_types() const final
    {
        return expected_types_;
    }

private:
    Tag<Types...>* do_clone() const final { return new Tag<Types...>(*this); }
    std::string id_;
    std::vector<DataTypeRef> expected_types_;
};

template <typename TagT>
struct hash
{
    std::size_t operator()(const TagT& tb) const
    {
        return std::hash<TagID>{}(tb.id());
    }
}; // struct hash<TagBase<BaseTagType>>

} // namespace skywing

#endif // SKYWING_SKYWING_CORE_TAG_HPP
