#ifndef SKYWING_INTERNAL_TAG_BUFFER_HPP
#define SKYWING_INTERNAL_TAG_BUFFER_HPP

#include <cassert>
#include <cstring>
#include <optional>
#include <span>
#include <vector>

#include "skywing_core/internal/utility/type_list.hpp"
#include "skywing_core/types.hpp"

namespace skywing::internal
{
namespace detail
{
// Checks if a span representing a tag's value is valid compared to what is
// expected
template <typename... Ts, std::size_t... Is>
bool span_is_valid(const std::span<const PublishValueVariant> value,
                   std::index_sequence<Is...>) noexcept
{
    return value.size() == sizeof...(Ts)
           && (...
               && (value[Is].index() == index_of<Ts, PublishValueTypeList>) );
}

// Takes a tag value and turns it into either a value (single element) or tuple
// non-const version
template <typename... Ts, std::size_t... Is>
ValueOrTuple<Ts...> make_value(std::span<PublishValueVariant> value,
                               std::index_sequence<Is...> seq) noexcept
{
    (void) seq; // avoid compiler warning in release buiild
    assert(span_is_valid<Ts...>(value, seq));
    if constexpr (sizeof...(Ts) == 1) {
        assert(std::get_if<Ts...>(&value[0]));
        return std::move(*std::get_if<Ts...>(&value[0]));
    }
    else {
        // Is lines up for the span, Ts is the types for the tuple
        assert((... && std::get_if<Ts>(&value[Is])));
        return std::make_tuple(std::move(*std::get_if<Ts>(&value[Is]))...);
    }
}

// Const version of the above
template <typename... Ts, std::size_t... Is>
ValueOrTuple<Ts...> make_value(std::span<const PublishValueVariant> value,
                               std::index_sequence<Is...> seq) noexcept
{
    (void) seq; // avoid compiler warning in release buiild
    assert(span_is_valid<Ts...>(value, seq));
    if constexpr (sizeof...(Ts) == 1) {
        assert(std::get_if<Ts...>(&value[0]));
        return *std::get_if<Ts...>(&value[0]);
    }
    else {
        assert((... && std::get_if<Ts>(&value[Is])));
        return std::make_tuple(*std::get_if<Ts>(&value[Is])...);
    }
}
} // namespace detail


template <typename TagT>
struct hash
{
    std::size_t operator()(const TagT& tb) const
    {
        return std::hash<TagID>{}(tb.id());
    }
}; // struct hash<TagBase<BaseTagType>>

inline static constexpr VersionID tag_no_data = -1;

/** \brief Buffer for a tag that only keeps the latest version that has
 * been recieved.
 */
class DiscardOldVersionTagBufferBase
{
public:
    /** \brief Returns true if data is present in the buffer.
     */
    bool has_data() const noexcept { return do_has_data(); }

    /** \brief Returns a void* to the stored data and marks it as removed
     * from the buffer
     *
     * \pre There is stored data
     */
    void* get() noexcept { return do_get(); }

  /** \brief Adds data if the version is newer
   */
  void add(std::span<const PublishValueVariant> value, const VersionID version) noexcept
  {
    return do_add(value, version);
  }

    /** \brief Resets the tag buffer to the default state
     */
    void reset() noexcept { do_reset(); }

    virtual ~DiscardOldVersionTagBufferBase() = default;

private:
  virtual bool do_has_data() const noexcept = 0;
  virtual void* do_get() noexcept = 0;
  virtual void do_add(std::span<const PublishValueVariant> value, const VersionID version) noexcept = 0;
  virtual void do_reset() noexcept = 0;
}; // DiscardOldVersionTagBufferBase

template <typename... Ts>
class DiscardOldVersionTagBuffer : public DiscardOldVersionTagBufferBase
{
private:
    bool do_has_data() const noexcept override
    {
        return stored_version_ != tag_no_data
               && stored_version_ >= last_fetched_version_ + 1;
    }

    void* do_get() noexcept override
    {
        assert(this->has_data());
        this->last_fetched_version_ = this->stored_version_;
        return &value_;
    }


    void do_add(std::span<const PublishValueVariant> value,
                const VersionID version) noexcept override
    {
        if (version > this->stored_version_
            || this->stored_version_ == tag_no_data)
        {
            this->stored_version_ = version;
            value_ = detail::make_value<Ts...>(
                value, std::index_sequence_for<Ts...>{});
        }
    }

    void do_reset() noexcept override
    {
        stored_version_ = tag_no_data;
        last_fetched_version_ = tag_no_data;
    }

    ValueOrTuple<Ts...> value_;
    VersionID stored_version_ = tag_no_data;
    VersionID last_fetched_version_ = tag_no_data;
}; // class DiscardOldVersionTagBuffer

/** \brief Buffer for a tag that keeps all new recieved versions, and returns
 * them in order.  Discards old or already recieved tags.
 */
template <typename... Ts>
class FifoTagBuffer
{
public:
    using ValueType = ValueOrTuple<Ts...>;

    /** \brief Returns a pointer to the stored data and remove it from the
     * buffer
     *
     * \pre The buffer has available data
     */
    ValueType get(const VersionID required_version) noexcept
    {
        while (true) {
            assert(!buffer_.empty());
            const auto [data, version] = buffer_.front();
            if (version >= required_version) {
                last_fetched_version_ = version;
                buffer_.erase(buffer_.begin());
                return data;
            }
            else {
                buffer_.erase(buffer_.begin());
            }
        }
    }

    /** \brief Returns true if data can be retrieved for the specified version
     */
    bool has_data(const VersionID required_version) const noexcept
    {
        return !buffer_.empty() && buffer_.back().second >= required_version;
    }

    /** \brief Adds data to the buffer if the version is newer than the last
     * version
     *
     * \pre value matches the expected types for the derived class
     */
    void add(std::span<const PublishValueVariant> value,
             const VersionID version) noexcept
    {
        assert(detail::span_is_valid<Ts...>(value,
                                            std::index_sequence_for<Ts...>{}));
        if (version > last_stored_version_
            || last_stored_version_ == tag_no_data)
        {
            buffer_.emplace_back(detail::make_value<Ts...>(
                                     value, std::index_sequence_for<Ts...>{}),
                                 version);
        }
    }

    /** \brief Resets the buffer to the default state
     */
    void reset() noexcept
    {
        buffer_.clear();
        last_stored_version_ = tag_no_data;
        last_fetched_version_ = tag_no_data;
    }

    std::vector<std::pair<ValueType, VersionID>> buffer_;
    VersionID last_stored_version_ = tag_no_data;
    VersionID last_fetched_version_ = tag_no_data;
}; // class FifoTagBuffer
} // namespace skywing::internal

#endif // SKYWING_INTERNAL_TAG_BUFFER_HPP
