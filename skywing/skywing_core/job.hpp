#ifndef SKYWING_JOB_HPP
#define SKYWING_JOB_HPP

#include <cassert>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iterator>
#include <numeric>
#include <optional>
#include <span>
#include <thread>
#include <unordered_map>
#include <vector>

#include "skywing_core/buffer.hpp"
#include "skywing_core/internal/buffer.hpp"
#include "skywing_core/internal/manager_waiter_callables.hpp"
#include "skywing_core/internal/utility/mutex_guarded.hpp"
#include "skywing_core/internal/utility/type_list.hpp"
#include "skywing_core/subscription.hpp"
#include "skywing_core/tag.hpp"
#include "skywing_core/types.hpp"
#include "skywing_core/waiter.hpp"
#include "skywing_mid/internal/iterative_helpers.hpp"

namespace skywing
{
//  A Job needs to be able to communicate with the Manager so forward declare it
class Manager;
class ManagerHandle;

/** \brief Job with known tags
 */
class Job
{
public:
    /** \brief Creates a job with the specified manager and work
     */
    Job(const std::string& id,
        Manager& manager,
        std::function<void(Job&, ManagerHandle)> to_run) noexcept;

    /** \brief Creates a thread of execution for the job with a callable
     * \return A thread for this job
     */
    std::thread run() noexcept;

    /** \brief Returns a reference to the mutex that guards subscriptions
     */
    std::mutex& get_mutex() noexcept { return subs_.mutex(); }

    /** \brief Declare intent to publish on tags, this must be done before
     * publishing on a tag
     */
    template <IsTag... Ts>
    void declare_publication_intent(const Ts&... tags) noexcept
    {
        using TagPtr = const AbstractTag*;
        const std::array<TagPtr, sizeof...(Ts)> tag_ptrs{&tags...};
        declare_publication_intent_impl(tag_ptrs);
    }

    /** \brief Declare publication intent for a range
     */
    template <typename TagContainer>
        requires IsTag<typename TagContainer::value_type>
    void declare_publication_intent_range(const TagContainer& tags_in) noexcept
    {
        std::vector<const AbstractTag*> tags;
        tags.reserve(tags_in.size());
        for (auto const& t : tags_in)
            tags.push_back(&t);
        declare_publication_intent_impl(tags);
    }

    /** \brief Retrieves the specified version for the tag, or latest if no
     * version is specified
     *
     * \return A Waiter for the value
     * \pre The tag is subscribed to
     */
    template <typename... Ts>
    Waiter<std::optional<ValueOrTuple<Ts...>>> get_waiter(const Tag<Ts...>& tag)
    {
        using ValueType = ValueOrTuple<Ts...>;
        // Can just capture the reference to the value as it
        // will never get invalidated except when the element is deleted
        // due to being in an unordered_map
        auto [subscriptions, lock] = subs_.get();
        (void) lock;
        const auto tag_iter = subscriptions.find(tag.id());
        assert(tag_iter != subscriptions.cend());
        auto& subscription = tag_iter->second;
        const auto tag_conn_id = subscription.id();
        return make_waiter<std::optional<ValueType>>(
            subs_.mutex(),
            data_buffer_modified_cv_,
            [&subscription, tag_conn_id]() {
                return subscription.has_data() || subscription.has_error()
                       || subscription.id() != tag_conn_id;
            },
            [&subscription]() mutable -> std::optional<ValueType> {
                // Don't check error information because the connection could
                // have errored between storing the value in the buffer and then
                // retrieving it
                if (subscription.has_data()) {
                    std::any any_value = ValueType{};
                    subscription.get_data(any_value);
                    return std::make_optional(
                        internal::detail::cast_to_value_or_tuple<Ts...>(
                            any_value));
                }
                else {
                    return std::nullopt;
                }
            });
    }

      /** \brief Retrieves the specified version for the tag, or latest if no
     * version is specified
     *
     * \return A void waiter
     * \pre The tag is subscribed to.
     * \pre The value to wait to recieve.
     */
    template <typename... Ts>
    Waiter<void> get_target_val_waiter(const Tag<Ts...>& tag, ValueOrTuple<Ts...> target_val)
    {
        using ValueType = ValueOrTuple<Ts...>;
        auto [subscriptions, lock] = subs_.get();
        (void) lock;
        const auto tag_iter = subscriptions.find(tag.id());
        assert(tag_iter != subscriptions.cend());
        auto& subscription = tag_iter->second;
        return make_waiter(
            subs_.mutex(),
            data_buffer_modified_cv_,
            [&subscription, target_val]() {
                if (subscription.has_data()) {
                    std::any any_value = ValueType{};
                    subscription.get_data(any_value);
                    ValueOrTuple<Ts...> out_val =
                        internal::detail::cast_to_value_or_tuple<Ts...>(
                            any_value);
                    // std::cout << "Recieved value " << out_val << " from" <<
                    // tag_conn_id << std::endl; std::cout << "Target value " <<
                    // target_val << std::endl;
                    return (out_val == target_val);
                }
                return false;
            });
    }

    /** \brief Get a value from a subscription, if there is data to get.

        \return std::optional<Ts...> An optional that, if there is data,
        holds the data.
     */
    template <typename... Ts>
    std::optional<ValueOrTuple<Ts...>>
    get_data_if_present(const Tag<Ts...>& tag)
    {
        using OptT = std::optional<ValueOrTuple<Ts...>>;
        Waiter<OptT> waiter = get_waiter(tag);
        if (waiter.is_ready())
            return waiter.get();
        else
            return std::nullopt;
    }

    /** \brief Checks if a tag buffer has data or not
     */
    bool has_data(const AbstractTag& tag) noexcept;

    /** \brief Request a subscription to publication stream denoted by the given
     * tag.
     *
     * \pre The tag is not currently subscribed to
     * \return A future for when the tag has been subscribed to
     */
    template <IsTag... Ts>
    Waiter<void> subscribe(const Ts&... tags) noexcept
    {
        const auto tag_is_not_subscribed = [&](const auto& tag) noexcept {
            const auto [subscriptions, lock] = subs_.get();
            (void) lock;
            return !subscriptions.contains(tag.id());
        };
        (void) tag_is_not_subscribed; // avoid compiler warning in release build

        // TODO: Make this std::terminate or something instead?
        assert("Tag attempted to be subscribed to twice!"
               && (... && tag_is_not_subscribed(tags)));
        {
            [[maybe_unused]] auto [subscriptions, lock] = subs_.get();
            for (const auto& tag : {tags...}) {
                if (subscriptions.contains(tag.id())) {
                    Subscription& sub = subscriptions.at(tag.id());
                    sub.reset();
                }
                else {
                    auto sub{tmp_buffer_ ? Subscription(tag, *tmp_buffer_)
                                         : Subscription(tag)};
                    subscriptions.insert_or_assign(
                        tag.id(),
                        tmp_buffer_ ? Subscription(tag, *tmp_buffer_)
                                    : Subscription(tag));
                }
            }
        }

        using TagPtr = const AbstractTag*;
        std::array<TagPtr, sizeof...(Ts)> tag_ptrs{&tags...};
        return get_subscribe_future(tag_ptrs);
    }

    /** \brief Subscribes to a range of tags.
     */
    template <typename TagContainer>
        requires IsTag<typename TagContainer::value_type>
    Waiter<void> subscribe_range(const TagContainer& tags) noexcept
    {
        std::vector<const AbstractTag*> tag_span;
        tag_span.reserve(tags.size());
        {
            [[maybe_unused]] auto [subscriptions, lock] = subs_.get();
            for (auto const& tag : tags) {
                tag_span.push_back(&tag);
                if (subscriptions.contains(tag.id())) {
                    Subscription& sub = subscriptions.at(tag.id());
                    sub.reset();
                }
                else {
                    auto sub{tmp_buffer_ ? Subscription(tag, *tmp_buffer_)
                                         : Subscription(tag)};
                    subscriptions.insert_or_assign(
                        tag.id(),
                        tmp_buffer_ ? Subscription(tag, *tmp_buffer_)
                                    : Subscription(tag));
                }
            }
        }
        return get_subscribe_future(tag_span);
    }

    /** \brief Subscribe to a set of tags from a specific IP
     */
    template <IsTag... Ts>
    Waiter<bool> ip_subscribe(const std::string& address,
                              const Ts&... tags) noexcept
    {
        subscribe(tags...);
        using TagPtr = const AbstractTag*;
        std::array<TagPtr, sizeof...(Ts)> tag_ptrs{&tags...};
        return get_ip_subscribe_future(address, tag_ptrs);
    }

    /** \brief Publish data on the passed tag
     *
     * Will abort in debug mode if the tag has not been declared for publication
     */
    template <typename... PublishTagTypes, typename... ArgTypes>
    void publish(const Tag<PublishTagTypes...>& tag,
                 ArgTypes&&... values) noexcept
    {
        static_assert(
            sizeof...(PublishTagTypes) == sizeof...(ArgTypes)
                && (... && std::is_convertible_v<ArgTypes, PublishTagTypes>),
            "Argument values can not be converted to tag types!");
        std::array<PublishValueVariant, sizeof...(ArgTypes)> variants{
            static_cast<PublishTagTypes>(std::forward<ArgTypes>(values))...};
        publish_impl(tag, variants);
    }

    template <typename... PublishTagTypes, typename... TupleTypes>
    void publish(const Tag<PublishTagTypes...>& tag,
                 const std::tuple<TupleTypes...>& value_tuple) noexcept
    {
        const auto apply_to = [&](const auto&... values) {
            publish(tag, values...);
        };
        std::apply(apply_to, value_tuple);
    }

    template <typename... PublishTagTypes, typename... TupleTypes>
    void publish_tuple(const Tag<PublishTagTypes...>& tag,
                       const std::tuple<TupleTypes...>& value_tuple) noexcept
    {
        const auto apply_to = [&](const auto&... values) {
            publish(tag, values...);
        };
        std::apply(apply_to, value_tuple);
    }

    /** \brief Returns true if the job is finished, false if it is not
     */
    bool is_finished() const noexcept;

    /**
     * @brief Set the buffer type for all subscriptions in this job
     *
     * @param buffer_type A Buffer object that is a template specialization
     * of Buffer types, one of specializations defined from wrapper functions in
     * skywing_core/buffer.hpp
     *
     * @details Users can specify buffer types by using the following syntax
     * job.set_buffer( Buffer::IntQueue() );
     * job.set_buffer( Buffer::IntMostRecent() );
     *
     * For custom buffer types, see function overload.
     */
    void set_buffer(const Buffer& buffer_type)
    {
        tmp_buffer_ = buffer_type.get_buffer();
    }

    void set_buffer(const internal::Buffer& buffer)
    {
        tmp_buffer_ = buffer;
    }

    /** \brief Returns a list of the produced tags
     */
    const std::unordered_map<TagID, std::span<const std::uint8_t>>&
    tags_produced() const noexcept;

    /** \brief Returns the job's id
     */
    const JobID& id() const noexcept;

    /** \brief Returns if the specified tag has a corresponding connection
     */
    template <typename T>
    bool tag_has_active_publisher(const T& tag) const noexcept
    {
        return tag_has_active_publisher_impl(tag.id());
    }

    /** \brief Rebuilds connections for the specified tags
     */
    template <typename TagPtrContainer>
    Waiter<void> rebuild_tags(const TagPtrContainer& tags)
    {
        subscribe_range(tags);
        std::vector<const AbstractTag*> tag_ptrs;
        for (auto const& t : tags) {
            tag_ptrs.push_back(std::to_address(t));
        }
        return get_subscribe_future(tag_ptrs);
    }

    /** \brief Rebuilds connections for any missing tags
     *
     * \return A future for when the tags are re-connected
     */
    Waiter<void> rebuild_missing_tag_connections() noexcept
    {
        [[maybe_unused]] const auto [subscriptions, lock] = subs_.get();
        std::vector<const AbstractTag*> missing_tags;
        for (auto& [tag_id, subscription] : subscriptions) {
            if (subscription.has_error()) {
                subscription.reset();
                const auto& missing_tag = subscription.get_tag();
                missing_tags.emplace_back(&missing_tag);
            }
        }
        return get_subscribe_future(missing_tags);
    }

    /** \brief Check if a tag's subscription is valid or not
     */
    bool tag_has_subscription(const AbstractTag& tag) const noexcept;

    /** \brief Returns the number of subscriptions that a tag has
     *
     * TODO: There's currently no distinction between tags for a subscription,
     * add a way to do this and also only send data on tags which machines are
     * subscribed to.
     */
    size_t number_of_subscribers(const AbstractTag& tag) const noexcept;

    void wait_for_update()
    {
        std::unique_lock<std::mutex> lock{subs_.mutex()};
        data_buffer_modified_cv_.wait(lock);
        lock.unlock();
    }

    template <typename Duration>
    void wait_for_update(Duration duration)
    {
        std::unique_lock<std::mutex> lock{subs_.mutex()};
        data_buffer_modified_cv_.wait_for(lock, duration);
        lock.unlock();
    }

    void notify_of_update() { data_buffer_modified_cv_.notify_all(); }

    /** \brief Processes the raw information sent from a job on another instance
     *
     * \param tag The id of the tag the data was sent with
     * \param data The data sent on the tag
     * \param version The version of the data
     * \return True if processing went fine, false if there was an error
     */
    bool process_data(const TagID& tag_id,
                      std::span<PublishValueVariant> data,
                      VersionID version) noexcept;

    /** \brief Marks a tag as dead due to connection issues
     *
     * \param tag The id of the tag to mark as dead
     */
    void mark_tag_as_dead(const TagID& tag_id) noexcept;

private:
    /** \brief Checks if a buffer has data without locking
     */
    bool has_data_no_lock(const AbstractTag& tag) noexcept;

    void publish_impl(const AbstractTag& tag,
                      std::span<PublishValueVariant> to_send) noexcept;

    Waiter<void>
    get_subscribe_future(std::span<const AbstractTag* const> tags) noexcept;

    Waiter<bool>
    get_ip_subscribe_future(const std::string& address,
                            std::span<const AbstractTag* const> tags) noexcept;

    void declare_publication_intent_impl(
        std::span<const AbstractTag* const> tags) noexcept;

    bool tag_has_active_publisher_impl(const TagID& tag_id) const noexcept;

    // The id of the job
    JobID id_;

    MutexGuarded<std::unordered_map<std::string, Subscription>> subs_;

    // The last version published on each tag
    std::unordered_map<std::string, VersionID> last_published_version_;

    static constexpr int tag_no_data = -1;

    // The manager that this job is working with
    Manager* manager_;

    // The function this job will run
    std::function<void(Job&, ManagerHandle)> to_run_;

    // The list of tags this job produces and the expected types
    std::unordered_map<TagID, std::span<const std::uint8_t>> tags_produced_;

    // Condition variable when data is added to buffers or an error occurs
    std::condition_variable data_buffer_modified_cv_;

    // Temporary variable to hold buffer type until subscription is made
    std::optional<internal::Buffer> tmp_buffer_;
}; // Class Job
} // namespace skywing

#endif // SKYWING_JOB_HPP
