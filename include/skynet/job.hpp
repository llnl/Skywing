#ifndef SKYNET_JOB_HPP
#define SKYNET_JOB_HPP

#include "skynet/internal/utility/mutex_guarded.hpp"
#include "skynet/internal/utility/type_list.hpp"
#include "skynet/internal/reduce_group.hpp"
#include "skynet/internal/tag_buffer.hpp"
#include "skynet/local_future.hpp"
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
    constexpr Tag(const TagID& id) noexcept
      : id_{id}
    {
      assert(!id.empty());
    }

    /** \brief Return the id of the tag
     */
    const TagID& id() const noexcept { return id_; }

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

      // Work around to disallow construction of Jobs outside of the master
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
      const std::string& id,
      Master& master,
      std::function<void(Job&)> to_run
    ) noexcept;

    /** \brief Declare intent to publish on tags, this must be done before publishing
     * on a tag
     */
    template<typename... Tags>
    void declare_publication_intent(const Tags&... tags) noexcept
    {
      (tags_produced_.try_emplace(
        tags.id(),
        internal::index_of<typename Tags::ValueType, PublishValueTypeList>
      ), ...);
      report_tags({tags.id()...});
    }

    /** \brief Retrieves the specified version for the tag, or latest if no version
     * is specified
     *
     * \return A LocalFuture for the value
     */
    template<typename GetTag>
    auto get_future_for(
      const GetTag& tag,
      const VersionID version = internal::tag_default_version
    ) noexcept
    {
      return internal::make_local_future(
        [this, tag, version]() {
          return has_data(tag, version);
        },
        [this, tag, version]() {
          using ValueType = typename GetTag::ValueType;
          const auto variant = get_impl(tag.id(), version);
          assert(std::get_if<ValueType>(&variant) != nullptr);
          return *std::get_if<ValueType>(&variant);
        }
      );
    }

    /** \brief Checks if a tag buffer has data or not
     */
    template<typename GetTag>
    bool has_data(const GetTag& tag, const VersionID version = internal::tag_default_version) noexcept
    {
      return has_data_impl(tag.id(), version);
    }

    /** \brief Attempts to subscribe to the passed tag
     *
     * \return A LocalFuture for when the tag is subscribed to
     */
    template<typename Tag>
    auto subscribe(const Tag& tag) noexcept
    {
      using ValueType = typename Tag::ValueType;
      init_subscribe({tag.id()}, {internal::index_of<ValueType, PublishValueTypeList>});
      return internal::make_local_future(
        [tag, this]() { return is_subscribe_finished({tag.id()}); }
      );
    }

    /** \brief Subscribe to all tags passed into the vector.
     *
     * \return A LocalFuture for when the tags have been subscribed to
     */
    template<typename Tag>
    auto subscribe(const std::vector<TagID>& tags) noexcept
    {
      using ValueType = typename Tag::ValueType;
      const std::vector<std::uint8_t> expected_types(
        tags.size(),
        internal::index_of<ValueType, PublishValueTypeList>
      );
      init_subscribe(tags, expected_types);
      return internal::make_local_future(
        [tags, this]() { return is_subscribe_finished(tags); }
      );
    }

    /** \brief Subscribes to all of the passed tags.
     *
     * \return A LocalFuture for when the tags have been subscribed to
     */
    template<typename... SubTags>
    std::enable_if_t<sizeof...(SubTags), bool> subscribe(const SubTags&... tags) noexcept
    {
      const std::vector<std::string> tag_ids{tags.id()...};
      const std::vector<std::uint8_t> expected_types{
        internal::index_of<typename SubTags::ValueType, PublishValueTypeList>...
      };
      init_subscribe(tag_ids, expected_types);
      return internal::make_local_future(
        [tag_ids, this]() {
          return is_subscribe_finished(tag_ids);
        }
      );
    }

    /** \brief Create a reduce group over the specified tag names
     */
    template<typename ReduceTag>
    auto create_reduce_group(
      const ReduceTag& group_tag,
      const ReduceTag& tag_produced_for_group,
      const std::vector<TagID>& tags
    ) noexcept
    {
      using ValueType = typename ReduceTag::ValueType;
      constexpr auto expected_type =
        internal::index_of<ValueType, PublishValueTypeList>;
      const auto tags_to_find = create_reduce_group_init(tag_produced_for_group.id(), tags, expected_type);
      return internal::make_local_future(
        [group_tag, tag_produced_for_group, tags_to_find, this]() {
          return is_reduce_group_created(group_tag.id(), tag_produced_for_group.id(), tags_to_find, expected_type);
        },
        [group_tag, this]() -> internal::ReduceGroup<ValueType>& {
          return static_cast<internal::ReduceGroup<ValueType>&>(get_reduce_group(group_tag.id()));
        }
      );
    }

    /** \brief Create a reduce group over the specified tags
     *
     * \return A LocalFuture for when the reduce group has been created
     */
    template<typename ReduceTag, typename... ReduceOverTags>
    auto create_reduce_group(
      const ReduceTag& group_tag,
      const ReduceTag& tag_produced_for_group,
      const ReduceOverTags&... tags
    ) noexcept
    {
      constexpr auto expected_type =
        internal::index_of<typename ReduceTag::ValueType, PublishValueTypeList>;
      static_assert(
        ((expected_type == internal::index_of<typename ReduceOverTags::ValueType, PublishValueTypeList>) && ...),
        "All tags in a reduce group must produce the same type!"
      );
      const std::vector<std::string> tag_ids{tags.id()...};
      return create_reduce_group(group_tag, tag_produced_for_group, tag_ids);
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
      const VersionID version = internal::tag_default_version
    ) noexcept
    {
      publish_impl(tag.id(), value, version);
    }

    /** \brief Returns true if the job is finished, false if it is not
     */
    bool is_finished() const noexcept;

    /** \brief Returns a list of the produced tags
     */
    const std::unordered_map<TagID, std::uint8_t>& tags_produced() const noexcept;

    /** \brief Returns the job's id
     */
    const JobID& id() const noexcept;

  private:
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

    /** \brief Gets data for the specified tag
     *
     * \pre Data exists on the tag
     */
    PublishValueVariant get_impl(const TagID& tag_id, VersionID version) noexcept;

    bool has_data_impl(const TagID& tag_id, VersionID version) noexcept;

    void init_subscribe(
      const std::vector<TagID>& tag_ids,
      const std::vector<std::uint8_t>& expected_types
    ) noexcept;

    bool is_subscribe_finished(const std::vector<TagID>& tag_ids) noexcept;

    // void unsubscribe_impl(const TagID& tag_id) noexcept;

    // Returns the tags that connections need to be made with
    internal::ReduceGroupNeighbors create_reduce_group_init(
      const TagID& tag_produced,
      const std::vector<TagID>& reduce_over_tags,
      std::uint8_t expected_type
    ) noexcept;

    bool is_reduce_group_created(
      const TagID& tag_produced,
      const TagID& group_tag_id,
      const internal::ReduceGroupNeighbors& tags_to_find,
      std::uint8_t expected_type
    ) noexcept;

    // Report that new tags are going to be published on
    // Requires a seperate function because it needs to talk with the master
    void report_tags(const std::vector<TagID>& tags) noexcept;

    // Gets a reduce group reference from the master
    internal::ReduceGroupBase& get_reduce_group(const TagID& group_id) noexcept;

    // The id of the job
    JobID id_;

    // Group all of the related data to a tag ID in a single structure
    struct TagInfo
    {
      // The buffer
      internal::DiscardOldVersionTagBuffer<PublishValueVariant> buffer;
      // The expected type
      std::uint8_t expected_type;
    };
    MutexGuarded<std::unordered_map<std::string, TagInfo>> bufs_;

    // Similar to the above, but for

    // The last version published on each tag
    std::unordered_map<std::string, VersionID> last_published_version_;

    // The master that this job is working with
    Master* master_;

    // The function this job will run
    std::function<void(Job&)> to_run_;

    // The list of tags this job produces and the expected types
    std::unordered_map<TagID, std::uint8_t> tags_produced_;
  }; // Class Job
} // namespace skynet

#endif // SKYNET_JOB_HPP
