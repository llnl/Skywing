#ifndef SKYNET_JOB_HPP
#define SKYNET_JOB_HPP

#include "skynet/internal/utility/mutex_guarded.hpp"
#include "skynet/internal/utility/type_list.hpp"
#include "skynet/types.hpp"

#include <cassert>
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
    // Allow the master to call process data and run
    struct Accessor
    {
    private:
      friend class Master;
      friend class Job;

      static bool process_data(
        Job& j,
        const TagID& tag,
        PublishValueVariant data,
        const VersionID version
      ) noexcept
      {
        return j.process_data(tag, data, version);
      }

      static std::thread run(Job& j) noexcept
      {
        return std::thread{[&j]() {
          j.to_run_(j);
          // Signify that the work is done
          j.to_run_ = nullptr;
        }};
      }

      // Work around to disallow construction outside of the master
      // A public constructor is needed due to it being emplaced into a map
      struct AllowConstruction
      {
        // Explicit constructor so that it has to be named, but the name is private
        explicit AllowConstruction() = default;
      };
    };

    /** \brief Creates a job with the specified master and work
     */
    Job(
      Accessor::AllowConstruction,
      Master& master,
      std::vector<TagID> tags,
      std::function<void(Job&)> to_run
    ) noexcept;

    /** \brief The value to pass to use default behavior for
     * versions when publishing and subscribing.
     */
    inline static constexpr VersionID tag_default_version = -1;

    /** \brief Gets the latest data on a tag (if any exists)
     */
    template<typename GetTag>
    std::optional<typename GetTag::ValueType> get(
      const GetTag& tag,
      const VersionID version = tag_default_version
    ) noexcept
    {
      if (const auto val = get_impl(tag.id(), version))
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
      const VersionID version = tag_default_version,
      const std::chrono::duration<Rep, Period> poll_freq = internal::default_poll_freq
    ) noexcept
    {
      while (!has_data(tag, version))
      {
        std::this_thread::sleep_for(poll_freq);
      }
      return *get(tag);
    }

    /** \brief Checks if a tag buffer has data or not
     */
    template<typename GetTag>
    bool has_data(const GetTag& tag, const VersionID version = tag_default_version) noexcept
    {
      return has_data_impl(tag.id(), version);
    }

    /** \brief Subscribes to the passed tag, does nothing if the tag is
     * already subscribed to.
     *
     * Returns true if the tag could be subscribed to (there's a known producer for the tag)
     * or false if it couldn't.
     */
    template<typename Tag>
    bool subscribe(const Tag& tag) noexcept
    {
      using ValueType = typename Tag::ValueType;
      return subscribe_impl(
        {tag.id()},
        {internal::index_of<ValueType, PublishValueTypeList>}
      );
    }

    /** \brief Subscribe to all tags passed into the vector.
     *
     * Returns true only if all of the passed tags could be subscribed to.
     */
    template<typename Tag>
    bool subscribe(const std::vector<TagID>& tags) noexcept
    {
      using ValueType = typename Tag::ValueType;
      std::vector<std::uint8_t> expected_types(
        tags.size(),
        internal::index_of<ValueType, PublishValueTypeList>
      );
      return subscribe_impl(tags, expected_types);
    }

    /** \brief Subscribes to all of the passed tags.
     *
     * Returns true only if all of the passed tags could be subscribed to.
     */
    template<typename... SubTags>
    std::enable_if_t<sizeof...(SubTags), bool> subscribe(const SubTags&... tags) noexcept
    {
      return subscribe_impl(
        {tags.id()...},
        {internal::index_of<typename SubTags::ValueType, PublishValueTypeList>...}
      );
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
    template<typename Tag>
    void publish(
      const Tag& tag,
      const typename Tag::ValueType& value,
      const VersionID version = tag_default_version
    ) noexcept
    {
      publish_impl(tag.id(), value, version);
    }

    /** \brief Returns true if the job is finished, false if it is not
     */
    bool is_finished() const noexcept;

    /** \brief Returns a list of the produced tags
     */
    const std::vector<TagID>& tags_produced() const noexcept;

  private:
    // Updates the version that's passed in and returns a copy of it
    // Do it like this instead of with a tag ID to prevent trying to double lock a mutex
    VersionID update_version(VersionID& to_update, const VersionID new_version) noexcept;

    /** \brief Processes the raw information sent from a job on another instance
     *
     * \param tag The id of the tag the data was sent with
     * \param data The data sent on the tag
     * \param version The version of the data
     * \return True if processing went fine, false if there was an error
     */
    bool process_data(const TagID& tag_id, PublishValueVariant data, VersionID version) noexcept;

    /** \brief Broadcasts a value on a tag to all nodes in the network
     */
    void publish_impl(
      const TagID& tag_id,
      const PublishValueVariant& to_send,
      VersionID version
    ) noexcept;

    // Implementation of public functions
    std::optional<PublishValueVariant> get_impl(const TagID& tag_id, VersionID version) noexcept;
    bool has_data_impl(const TagID& tag_id, VersionID version) noexcept;
    bool subscribe_impl(
      const std::vector<TagID>& tag_ids,
      const std::vector<std::uint8_t>& expected_types
    ) noexcept;
    // void unsubscribe_impl(const TagID& tag_id) noexcept;

    // Group all of the related data to a tag ID in a single structure
    struct TagInfo {
      // The data for the tag
      PublishValueVariant value;

      // The expected type
      std::uint8_t expected_type;

      // The last version retrieved
      VersionID last_fetched_version;

      // The version of the data that is currently stored
      VersionID stored_version;
    };
    MutexGuarded<std::unordered_map<std::string, TagInfo>> bufs_;

    // The last version published on each tag
    std::unordered_map<std::string, VersionID> last_published_version_;

    // The master that this job is working with
    Master* master_;

    // The function this job will run
    std::function<void(Job&)> to_run_;

    // The list of tags this job produces
    std::vector<TagID> tags_produced_;
  }; // Class Job
} // namespace skynet

#endif // SKYNET_JOB_HPP
