#ifndef SKYNET_JOB_HPP
#define SKYNET_JOB_HPP

#include "skynet/internal/utility/mutex_guarded.hpp"
#include "skynet/internal/utility/type_list.hpp"
#include "skynet/types.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <thread>
#include <unordered_map>
#include <vector>

namespace skynet
{
  namespace internal
  {
    // The default poll frequency for Job::get_when_ready
    inline static constexpr std::chrono::milliseconds default_poll_freq{1};
  } // namespace internal

  //  A Job needs to be able to communicate with the Master so forward declare it
  class Master;

  /** \brief A tag for sending values
   *
   * Tags certain values to be sent; all values sent for a specific tag must
   * be of the same type.
   */
  template <typename T>
  class Tag
  {
  public:
    /** \brief The type being sent over this tag
     */
    using ValueType = T;

    /** \brief Construct a tag with an ID
     */
    explicit constexpr Tag(const TagID id) noexcept
      : id_{id}
    {}

    /** \brief Return the id of the tag
     */
    constexpr TagID id() const noexcept { return id_; }

  private:
    // The id of this tag
    TagID id_;
  }; // class Tag

  /** \brief Job with known tags
   */
  class Job
  {
  public:
    /** \brief Creates a job with the specified id and master
     */
    explicit Job(const JobID& id, Master& master, std::function<void(Job&)> to_run) noexcept;

    // Disable copying and moving; can add moving later if it is needed
    Job(const Job&) = delete;
    Job& operator=(const Job&) = delete;
    Job(Job&&) = delete;
    Job& operator=(Job&&) = delete;

    /** \brief Gets the latest data on a tag (if any exists)
     */
    template<typename GetTag>
    std::optional<typename GetTag::ValueType> get(const GetTag& tag) noexcept
    {
      if (const auto val = get_impl(tag.id()))
      {
        // Wrong types should never be put in the buffer
        using ValueType = typename GetTag::ValueType;
        assert(std::get_if<ValueType>(&*val) != nullptr);
        return std::get<ValueType>(*val);
      }
      return {};
    }

    /** \brief Waits for data to be available on a certain tag and returns it
     * when it is.
     */
    template<
      typename GetTag,
      typename Rep = decltype(internal::default_poll_freq)::rep,
      typename Period = decltype(internal::default_poll_freq)::period
    >
    typename GetTag::ValueType get_when_ready(
      const GetTag& tag,
      const std::chrono::duration<Rep, Period> poll_freq = internal::default_poll_freq
    ) noexcept
    {
      while (!has_data(tag))
      {
        std::this_thread::sleep_for(poll_freq);
      }
      return *get(tag);
    }

    /** \brief Checks if a tag buffer has data or not
     */
    template<typename GetTag>
    bool has_data(const GetTag& tag) noexcept
    {
      return has_data_impl(tag.id());
    }

    /** \brief Subscribes to the passed tag, hard errors if already the job
     * is already subscribed to the tag (this is never valid)
     */
    template<typename Tag>
    void subscribe(const Tag& tag) noexcept
    {
      using ValueType = typename Tag::ValueType;
      subscribe_impl(tag.id(), internal::index_of<ValueType, BroadcastDataTypeList>);
    }

    /** \brief Subscribes to all of the passed tags
     */
    template<typename... SubTags>
    void subscribe(const SubTags&... tags) noexcept
    {
      (subscribe(tags), ...);
    }

    /** \brief Unsubscribes to the passed tag, does nothing if the job is not
     * subscribed to the tag
     */
    template<typename Tag>
    void unsubscribe(const Tag& tag) noexcept
    {
      unsubscribe_impl(tag.id());
    }

    /** \brief Unsubscribes from all of the passed tags
     */
    template<typename... UnsubTags>
    void unsubscribe(const UnsubTags&... tags) noexcept
    {
      (unsubscribe(tags), ...);
    }

    /** \brief Publish data on the passed tag
     */
    template<typename Tag>
    void publish(const Tag& tag, const typename Tag::ValueType& value) noexcept
    {
      global_broadcast(tag.id(), value);
    }

    /** \brief Returns true if the job is finished, false if it is not
     */
    bool is_finished() const noexcept;

    // Allow the master to call process data and run
    struct Accessor
    {
    private:
      friend class Master;
      static bool process_data(
        Job& j,
        const TagID& tag,
        BroadcastDataVariant data
      ) noexcept
      {
        return j.process_data(tag, data);
      }

      static std::thread run(Job& j) noexcept
      {
        return std::thread{[&j]() {
          j.to_run_(j);
          j.is_finished_ = true;
        }};
      }
    };

  private:
    /** \brief Processes the raw information sent from a job on another instance
     *
     * \param tag The id of the tag the data was sent with
     * \param data The data sent on the tag
     * \return True if processing went fine, false if there was an error
     */
    bool process_data(const TagID& tag_id, BroadcastDataVariant data) noexcept;

    /** \brief Broadcasts a value on a tag to all nodes in the network
     */
    void global_broadcast(const TagID& tag_id, BroadcastDataVariant to_send) noexcept;

    /** \brief Broadcasts a value on a tag to all neighbors
     */
    void local_broadcast(const TagID& tag_id, BroadcastDataVariant to_send) noexcept;

    // Implementation of public functions
    std::optional<BroadcastDataVariant> get_impl(const TagID& tag_id) noexcept;
    bool has_data_impl(const TagID& tag_id) noexcept;
    void subscribe_impl(const TagID& tag_id, std::uint8_t expected_type) noexcept;
    void unsubscribe_impl(const TagID& tag_id) noexcept;

    // Put both buffers in a struct so they can be guarded by the same mutex easily
    struct Buffers {
      // The buffer of data for each tag
      std::unordered_map<TagID, BroadcastDataVariant> values_;

      // The expected type for each tag ID
      std::unordered_map<TagID, std::uint8_t> expected_types_;
    };

    MutexGuarded<Buffers> bufs_;

    // The id for the message to send
    // Could keep a seperate id for each tag, but running out of message id's
    // isn't very realistic
    MessageID message_id_{1};

    // The id for this job; must be the same across all instances
    JobID id_;

    // The master that this job is working with
    Master* master_;

    // The function this job will run
    std::function<void(Job&)> to_run_;

    // If the job is finished
    bool is_finished_{false};
  }; // Class Job
} // namespace skynet

#endif // SKYNET_JOB_HPP
