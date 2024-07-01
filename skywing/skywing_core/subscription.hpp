#ifndef SKYWING_SUBSCRIPTION_HPP
#define SKYWING_SUBSCRIPTION_HPP

#include <cstdint>
#include <memory>
#include <span>

#include "skywing_core/internal/tag_buffer.hpp"
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
     * TODO : The buffer member variable should be templated to work with many
     * types of buffers. For now, we use buffer type DiscardOldVersionTagBuffer.
     */
    template <typename... Ts>
    explicit Subscription(Tag<Ts...> tag)
        : tag_(std::make_unique<Tag<Ts...>>(std::move(tag))),
          buffer_(
              std::make_unique<internal::DiscardOldVersionTagBuffer<Ts...>>())
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
        buffer_->reset();
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
        buffer_->add(value, version);
    }

    bool has_data() { return buffer_->has_data(); }

    void* get_data() const { return buffer_->get(); }

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
    std::unique_ptr<internal::DiscardOldVersionTagBufferBase> buffer_;
};

} // namespace skywing

#endif // SKYWING_SUBSCRIPTION_HPP