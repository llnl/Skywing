#ifndef SKYWING_SUBSCRIPTION_HPP
#define SKYWING_SUBSCRIPTION_HPP

#include <cstdint>
#include <memory>
#include <span>

#include "skywing_core/internal/buffer.hpp"
#include "skywing_core/internal/most_recent_buffer.hpp"
#include "skywing_core/tag.hpp"

namespace skywing
{
/**
 * @brief A subscription to a publication held by one job on one agent.
 *
 * A Subscription contains, fundamentally, a Tag that provides a
 * collective-global unique identifier for the publication stream,
 * along with a local data buffer holding received data.
 *
 */
class Subscription
{
public:
    /**
     * Constructor to create a Subscription.
     *
     * @tparam Ts Set of data types that will be sent with each
     * publication in the publication stream.
     * @param tag a Tag to uniquely identify this publication stream. The
     * expected data types are extracted from the template parameter to create
     * the expected types for the data buffer.
     * @param buffer a Buffer object which sets the buffer type for data.
     */
    template <typename... Ts>
    Subscription(Tag<Ts...> tag, internal::Buffer buffer)
        : tag_(std::make_unique<Tag<Ts...>>(std::move(tag))), buffer_{buffer}
    {}

    /**
     * Delegating constructor.
     */
    template <typename... Ts>
    Subscription(Tag<Ts...> tag)
        : Subscription(tag, internal::MostRecentBuffer<Ts...>())
    {}

    Subscription(Subscription const&) = delete;
    Subscription& operator=(Subscription const&) = delete;
    Subscription(Subscription&&) noexcept = default;
    Subscription& operator=(Subscription&&) noexcept = default;
    ~Subscription() noexcept = default;
    Subscription() noexcept = delete;

    void reset()
    {
        connection_id_++;
        using internal::reset;
        reset(buffer_);
        error_ = Error::no_error;
    }

    void mark_tag_as_dead()
    {
        connection_id_++;
        error_ = Error::disconnected;
    }

    void discard_tag() { error_ = Error::incorrect_type; }

    std::uint16_t id() const { return connection_id_; }

    bool has_error() const { return error_ != Error::no_error; }

    bool has_incorrect_type() const { return error_ == Error::incorrect_type; }

    bool is_disconnected() const { return error_ == Error::disconnected; }

    const auto& get_tag() const { return *tag_; }

    void add_data(std::span<const PublishValueVariant> value,
                  const VersionID version)
    {
        add(buffer_, value, version);
    }

    bool has_data()
    {
        using internal::has_data;
        return has_data(buffer_);
    }

    void get_data(std::any& value) const { get(buffer_, value); }

private:
    enum class Error
    {
        no_error,
        incorrect_type,
        disconnected
    };
    Error error_{Error::no_error};
    std::uint16_t connection_id_{0};
    std::unique_ptr<AbstractTag> tag_;
    internal::Buffer buffer_;
};

} // namespace skywing

#endif // SKYWING_SUBSCRIPTION_HPP