#pragma once

#include <limits>
#include <span>
#include <any>

#include "skywing_core/internal/utility/type_list.hpp"
#include "skywing_core/types.hpp"

namespace skywing::internal
{

/**
 * @name Constants
 * @{
 */
namespace
{
/**
 * @brief Constant to initialize version in MostRecentBuffer.
 * 
 * @details Unsigned int max is chosen instead of -1 to avoid
 * sign compare warnings and errors in code. Largest unsigned
 * integer value will wrap to 0.
 * 
 * Initialized at file scope internal linkage to be used
 * in free wrapper functions.
 */
static constexpr VersionID no_version =
    std::numeric_limits<std::uint32_t>::max();
}
/**
 * @}
 */

/** 
 * @brief Buffer for a tag that only keeps the latest version that has
 * been recieved.
 * 
 * @tparam Ts A template parameter pack of data types for data stored in the
 * buffer.
 */
template <typename... Ts>
class MostRecentBuffer
{
public:
    using ValueType = ValueOrTuple<Ts...>;

    void set_version(const VersionID version) noexcept
    {
        stored_version_ = version;
    }

    VersionID get_version() const noexcept { return stored_version_; }

    void set_last_fetched_version(const VersionID version) noexcept
    {
        last_fetched_version_ = version;
    }

    void set_value(const ValueType& value) noexcept { value_ = value; }

    /** 
     * @brief Returns true if data is present in the buffer.
     */
    [[nodiscard]] bool has_data() const noexcept
    {
        return stored_version_ != no_version
               && stored_version_ >= last_fetched_version_ + 1;
    }

    /** 
     * @brief Updates buffer to an empty state.
     * 
     * @details Since we return a pointer to the value in get_value(),
     * we do not reset value_ but use versions to describe buffer state.
     */
    void empty() noexcept { last_fetched_version_ = stored_version_; }

    /**
     * @brief Returns a copy of the stored data.
     * 
     * @pre There is stored data.
     */
    ValueType get_value() noexcept
    {
        assert(has_data());
        return value_;
    }

private:
    ValueType value_{};
    VersionID stored_version_{no_version};
    VersionID last_fetched_version_{no_version};
};

template <typename... Ts>
[[nodiscard]] bool has_data(const MostRecentBuffer<Ts...>& buffer) noexcept
{
    return buffer.has_data();
}

/** 
 * @brief Resets the buffer to the default state.
 */
template <typename... Ts>
void reset(MostRecentBuffer<Ts...>& buffer) noexcept
{
    buffer.set_version(no_version);
    buffer.set_last_fetched_version(no_version);
}

/** 
 * @brief Adds data to the buffer if the version is newer.
 */
template <typename... Ts>
void add(MostRecentBuffer<Ts...>& buffer,
         std::span<const PublishValueVariant> value,
         const VersionID version) noexcept
{
    auto stored_version_ = buffer.get_version();
    if (version > stored_version_ || stored_version_ == no_version) {
        buffer.set_version(version);
        auto new_value = skywing::internal::detail::make_value<Ts...>(
            value, std::index_sequence_for<Ts...>{});
        buffer.set_value(new_value);
    }
}

/** 
 * @brief Returns the data and marks it as removed
 * from the buffer.
 *
 * @param value data value from make_waiter to hold final data. 
 * wrapped and cast from std::any because Subscription is not a class template.
 */
template <typename... Ts>
void get(MostRecentBuffer<Ts...>& buffer, std::any& value)
{
    value = buffer.get_value();
    buffer.empty();
}

} // namespace skywing::internal