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

#include "skywing_core/internal/manager_waiter_callables.hpp"
#include "skywing_core/internal/tag_buffer.hpp"
#include "skywing_core/internal/utility/mutex_guarded.hpp"
#include "skywing_core/internal/utility/type_list.hpp"
#include "skywing_core/types.hpp"
#include "skywing_core/waiter.hpp"
#include "tag.hpp"
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
    // Allow the manager to call process data and run
    struct Accessor
    {
    private:
        friend class Manager;
        friend class Job;

        static bool process_data(Job& j,
                                 const TagID& tag,
                                 std::span<const PublishValueVariant> data,
                                 const VersionID version) noexcept
        {
            return j.process_data(tag, data, version);
        }

        static std::thread run(Job& j) noexcept;

        static std::mutex& get_mutex(Job& j) noexcept
        {
            return j.bufs_.mutex();
        }

        static void report_dead_tag(Job& j, const TagID& tag) noexcept
        {
            j.mark_tag_as_dead(tag);
        }

        // Work around to disallow construction of Jobs outside of the manager
        // A public constructor is needed due to it being emplaced into a map
        struct AllowConstruction
        {
            // Explicit constructor so that it has to be named, but the name is
            // private
            explicit AllowConstruction() = default;
        };
    };

    /** \brief Creates a job with the specified manager and work
     */
    Job(Accessor::AllowConstruction,
        const std::string& id,
        Manager& manager,
        std::function<void(Job&, ManagerHandle)> to_run) noexcept;

  /** \brief Declare intent to publish on tags, this must be done before publishing
   * on a tag
   */
  template<typename... Ts>
  void declare_publication_intent(const Ts&... tags) noexcept
  {
    using TagPtr = std::unique_ptr<const AbstractTag>;
    const std::array<TagPtr, sizeof...(Ts)> tag_ptrs{tags.clone()...};
    declare_publication_intent_impl(std::span<const TagPtr>{tag_ptrs});
  }

  /** \brief Declare publication intent for a range
   */
  template<typename Range>
  void declare_publication_intent_range(const Range& tags_in) noexcept
    requires std::ranges::contiguous_range<Range>
  {
    std::vector<std::unique_ptr<const AbstractTag>> tags;
    tags.reserve(tags_in.size());
    for (auto const& t : tags_in)
      tags.push_back(t.clone());
    declare_publication_intent_impl(tags);
  }

  /** \brief Retrieves the specified version for the tag, or latest if no version
   * is specified
   *
   * \return A Waiter for the value
   * \pre The tag is subscribed to
   */
  template<typename... Ts>
  Waiter<std::optional<ValueOrTuple<Ts...>>> get_waiter(const Tag<Ts...>& tag) noexcept
  {
    using ValueType = ValueOrTuple<Ts...>;
    // Can just capture the reference to the value as it
    // will never get invalidated except when the element is deleted
    // due to being in an unordered_map
    auto [buffers, lock] = bufs_.get();
    (void)lock;
    const auto tag_iter = buffers.find(tag.get_id());
    assert(tag_iter != buffers.cend());
    auto& tag_info = tag_iter->second;
    const auto tag_conn_id = tag_info.connection_id;
    return make_waiter<std::optional<ValueType>>(
      bufs_.mutex(),
      data_buffer_modified_cv_,
      [&tag_info, tag_conn_id]() {
        return tag_info.buffer->has_data() || tag_info.error_occurred != TagInfo::Error::no_error
            || tag_info.connection_id != tag_conn_id;
      },
      [&tag_info]() mutable -> std::optional<ValueType> {
        // Don't check error information because the connection could have
        // errored between storing the value in the buffer and then retrieving it
        if (tag_info.buffer->has_data()) { return *static_cast<ValueType*>(tag_info.buffer->get()); }
        else {
          return std::nullopt;
        }
      });
  }

  /** \brief Get a value from a subscription, if there is data to get.

      \return std::optional<Ts...> An optional that, if there is data,
      holds the data.
   */
    template<typename... Ts>
    std::optional<ValueOrTuple<Ts...>>
    get_data_if_present(const PublishTag<Ts...>& tag)
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

  /** \brief Request a subscription to publication stream denoted by the given tag.
   *
   * \pre The tag is not currently subscribed to
   * \return A future for when the tag has been subscribed to
   */
  template<typename... Ts>
  Waiter<void> subscribe(const Tag<Ts...>& tag) noexcept
  //  requires (... && std::is_base_of_v<internal::PublishTagBase, Ts>)
  {
    const auto tag_is_not_subscribed = [&](const auto& tag) noexcept {
      const auto [buffers, lock] = bufs_.get();
      (void)lock;
      return buffers.find(tag.get_id()) == buffers.cend();
    };
    (void)tag_is_not_subscribed; // avoid compiler warning in release buiild

    // TODO: Make this std::terminate or something instead?
    assert("Tag attempted to be subscribed to twice!" && tag_is_not_subscribed(tag));
    using BufferPtr = std::unique_ptr<internal::DiscardOldVersionTagBufferBase>;
    BufferPtr buffer = std::make_unique<typename Tag<Ts...>::BufferType>();
    init_or_update_subscribe(
      std::span<std::unique_ptr<const AbstractTag>, 1>{std::addressof(tag.clone()), 1},
      std::span<BufferPtr, 1>{std::addressof(buffer), 1});
    return get_subscribe_future(std::span<std::unique_ptr<const AbstractTag>, 1>{std::addressof(tag.clone()), 1});
  }

  /** \brief Subscribes to a range of tags.
   */
  template<typename Range>
  Waiter<void> subscribe_range(const Range& tags) noexcept
    requires std::ranges::contiguous_range<Range>
  {
    using IterType = std::decay_t<decltype(tags.begin())>;
    using TagType = typename std::iterator_traits<IterType>::value_type;
    using BufferPtr = std::unique_ptr<internal::DiscardOldVersionTagBufferBase>;
    using ValueType = typename TagType::ValueType;
    using BufferType = UnwrapAndApply_t<ValueType, internal::DiscardOldVersionTagBuffer>;
    std::vector<BufferPtr> ptrs(tags.size());
    std::generate(ptrs.begin(), ptrs.end(), []() noexcept { return std::make_unique<BufferType>(); });
    std::span<std::unique_ptr<const AbstractTag>> tag_span;
    std::transform(tags.cbegin(), tags.cend(), tag_span.begin(), [](const AbstractTag& t) { return t.clone(); });
    init_or_update_subscribe(tag_span, std::span<BufferPtr>{ptrs});
    return get_subscribe_future(tag_span);
  }

  /** \brief Subscribe to a set of tags from a specific IP
   */
  template<typename... Ts>
  Waiter<bool> ip_subscribe(const std::string& address, const Ts&... tags) noexcept
    requires IsTag<Ts...>
  {
    using BufferPtr = std::unique_ptr<internal::DiscardOldVersionTagBufferBase>;
    using TagPtr = std::unique_ptr<const AbstractTag>;
    std::array<BufferPtr, sizeof...(Ts)> ptrs{std::make_unique<typename Ts::BufferType>()...};
    const std::array<TagPtr, sizeof...(Ts)> tag_ptrs{tags.clone()...};
    init_or_update_subscribe(std::span<TagPtr>{tag_ptrs}, std::span<BufferPtr>{ptrs});
    return get_ip_subscribe_future(address, std::span<TagPtr>{tag_ptrs});
  }

    // /** \brief Unsubscribes to the passed tag, does nothing if the job is not
    //  * subscribed to the tag
    //  */
    // template<typename Tag>
    // void unsubscribe(const Tag& tag) noexcept
    // {
    //   unsubscribe_impl(tag.id());
    // }

    // /** \brief Unsubscribes from all of the passed tags
    //  */
    // template<typename... UnsubTags>
    // void unsubscribe(const UnsubTags&... tags) noexcept
    // {
    //   (unsubscribe(tags), ...);
    // }

  /** \brief Publish data on the passed tag
   *
   * Will abort in debug mode if the tag has not been declared for publication
   */
  template<typename... PublishTagTypes, typename... ArgTypes>
  void publish(const Tag<PublishTagTypes...>& tag, ArgTypes&&... values) noexcept
  {
    static_assert(
      sizeof...(PublishTagTypes) == sizeof...(ArgTypes) && (... && std::is_convertible_v<ArgTypes, PublishTagTypes>),
      "Argument values can not be converted to tag types!");
    std::array<PublishValueVariant, sizeof...(ArgTypes)> variants{
      static_cast<PublishTagTypes>(std::forward<ArgTypes>(values))...};
    publish_impl(tag, std::span<PublishValueVariant>{variants});
  }

  template<typename... PublishTagTypes, typename... TupleTypes>
  void publish(const Tag<PublishTagTypes...>& tag, const std::tuple<TupleTypes...>& value_tuple) noexcept
  {
    const auto apply_to = [&](const auto&... values) { publish(tag, values...); };
    std::apply(apply_to, value_tuple);
  }

  template<typename... PublishTagTypes, typename... TupleTypes>
  void publish_tuple(const Tag<PublishTagTypes...>& tag, const std::tuple<TupleTypes...>& value_tuple) noexcept
  {
    const auto apply_to = [&](const auto&... values) { publish(tag, values...); };
    std::apply(apply_to, value_tuple);
  }

    /** \brief Returns true if the job is finished, false if it is not
     */
    bool is_finished() const noexcept;

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
  template<typename Range>
  Waiter<void> rebuild_tags(const Range& tags)
  {
    std::vector<std::unique_ptr<internal::DiscardOldVersionTagBufferBase>> ptrs{tags.size()};
    std::span<std::unique_ptr<const AbstractTag>> tag_span;
    std::transform(tags.cbegin(), tags.cend(), tag_span.begin(), [&](const std::unique_ptr<const AbstractTag>& t) {
      return t->clone();
    });
    init_or_update_subscribe(tag_span, std::span<std::unique_ptr<internal::DiscardOldVersionTagBufferBase>>{ptrs});
    return get_subscribe_future(tag_span);
  }

  /** \brief Rebuilds connections for any missing tags
   *
   * \return A future for when the tags are re-connected
   */
  Waiter<void> rebuild_missing_tag_connections() noexcept
  {
    // init_or_update_subscribe obtains a lock, so might as well just
    // init this in a lambda (since it can then be const)
    const std::vector<std::unique_ptr<const AbstractTag>> tags = [&]() {
      const auto [buffers, lock] = bufs_.get();
      (void)lock;
      std::vector<std::unique_ptr<const AbstractTag>> tags;
      for (const auto& tag_pair : buffers) {
        if (tag_pair.second.error_occurred != TagInfo::Error::no_error) {
          // The expected type here doesn't matter
          // Also have to remove the first letter as it identifies the type of
          // tag, but it will just get added again later
          std::unique_ptr<const AbstractTag> tag_ptr
            = std::make_unique<const Tag<std::uint8_t>>(tag_pair.first.substr(1));
          tags.push_back(std::move(tag_ptr));
          ;
        }
      }
      return tags;
    }();
    return rebuild_tags(tags);
  }

  /** \brief Check if a tag's subscription is valid or not
   */
  bool tag_has_subscription(const AbstractTag& tag) const noexcept;

  template<typename Range>
  bool tags_have_subscriptions(const Range& tags) const noexcept
  {
    return tags_have_subscriptions_impl(std::span<const AbstractTag>{begin(tags), end(tags)});
  }

  /** \brief Returns the number of subscriptions that a tag has
   *
   * TODO: There's currently no distinction between tags for a subscription,
   * add a way to do this and also only send data on tags which machines are
   * subscribed to.
   */
  size_t number_of_subscribers(const AbstractTag& tag) const noexcept;

    void wait_for_update()
    {
        std::unique_lock<std::mutex> lock{bufs_.mutex()};
        data_buffer_modified_cv_.wait(lock);
        lock.unlock();
    }

    template <typename Duration>
    void wait_for_update(Duration duration)
    {
        std::unique_lock<std::mutex> lock{bufs_.mutex()};
        data_buffer_modified_cv_.wait_for(lock, duration);
        lock.unlock();
    }

  void notify_of_update() { data_buffer_modified_cv_.notify_all(); }

private:
  /** \brief Checks if a buffer has data without locking
   */
  bool has_data_no_lock(const AbstractTag& tag) noexcept;

    /** \brief Processes the raw information sent from a job on another instance
     *
     * \param tag The id of the tag the data was sent with
     * \param data The data sent on the tag
     * \param version The version of the data
     * \return True if processing went fine, false if there was an error
     */
    bool process_data(const TagID& tag_id,
                      std::span<const PublishValueVariant> data,
                      VersionID version) noexcept;

    /** \brief Marks a tag as dead due to connection issues
     *
     * \param tag The id of the tag to mark as dead
     */
    void mark_tag_as_dead(const TagID& tag_id) noexcept;

  void publish_impl(const AbstractTag& tag, std::span<PublishValueVariant> to_send) noexcept;

  void init_or_update_subscribe(
    std::span<std::unique_ptr<const AbstractTag>> tags,
    std::span<std::unique_ptr<internal::DiscardOldVersionTagBufferBase>> ptr) noexcept;

  Waiter<void> get_subscribe_future(std::span<std::unique_ptr<const AbstractTag>> tags) noexcept;

  Waiter<bool>
    get_ip_subscribe_future(const std::string& address, std::span<std::unique_ptr<const AbstractTag>> tags) noexcept;

  void declare_publication_intent_impl(std::span<const AbstractTag> tags) noexcept;

  void declare_publication_intent_impl(std::span<const std::unique_ptr<const AbstractTag>> tags) noexcept;

    // void unsubscribe_impl(const TagID& tag_id) noexcept;

  bool tag_has_active_publisher_impl(const TagID& tag_id) const noexcept;

  bool tags_have_subscriptions_impl(std::span<const AbstractTag> tags) const noexcept;

    // The id of the job
    JobID id_;

    // Group all of the related data to a tag ID in a single structure
    struct TagInfo
    {
        // For potential future use
        // Currently just used as a "is broken" flag essentially
        enum struct Error
        {
            no_error,
            incorrect_type,
            disconnected
        };
        // The buffer
        std::unique_ptr<internal::DiscardOldVersionTagBufferBase> buffer;
        // The expected type
        std::span<const std::uint8_t> expected_types;
        // ID for the connection so if a subscription is broken then reformed
        // they can be differentiated
        std::uint16_t connection_id;
        // The error (if any)
        Error error_occurred;
    };
    MutexGuarded<std::unordered_map<std::string, TagInfo>> bufs_;

    // The last version published on each tag
    std::unordered_map<std::string, VersionID> last_published_version_;

    // The manager that this job is working with
    Manager* manager_;

    // The function this job will run
    std::function<void(Job&, ManagerHandle)> to_run_;

    // The list of tags this job produces and the expected types
    std::unordered_map<TagID, std::span<const std::uint8_t>> tags_produced_;

    // Condition variable when data is added to buffers or an error occurs
    std::condition_variable data_buffer_modified_cv_;
}; // Class Job
} // namespace skywing

#endif // SKYWING_JOB_HPP
