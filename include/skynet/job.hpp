#ifndef SKYNET_JOB_HPP
#define SKYNET_JOB_HPP

#include "skynet/internal/utility/mutex_guarded.hpp"
#include "skynet/internal/utility/type_list.hpp"
#include "skynet/internal/future.hpp"
#include "skynet/internal/master_future_callables.hpp"
#include "skynet/internal/reduce_group.hpp"
#include "skynet/internal/tag_buffer.hpp"
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

  namespace internal
  {
    enum class TagType : char
    {
      publish_tag = publish_tag_marker,
      reduce_value = reduce_value_marker,
      reduce_group = reduce_group_marker
    };

    // The implementation for all tags would be the same,
    // so abstract it into a base
    template<TagType BaseTagType>
    class TagBase
    {
    public:
      TagBase(const TagID& id, std::uint8_t expected_type) noexcept
        : id_{static_cast<char>(BaseTagType) + id}
        , expected_type_{expected_type}
      {}

      const TagID& id() const noexcept { return id_; }
      std::uint8_t expected_type() const noexcept { return expected_type_; }

    private:
      TagID id_;
      std::uint8_t expected_type_;
    }; // class TagBase

    // Convenience aliases
    using PublishTagBase = internal::TagBase<TagType::publish_tag>;
    using ReduceValueTagBase = internal::TagBase<TagType::reduce_value>;
    using ReduceGroupTagBase = internal::TagBase<TagType::reduce_group>;
  } // namespace skynet::internal

  /** \brief Tag for pub/sub values
   */
  template<typename T>
  class PublishTag : public internal::PublishTagBase
  {
  public:
    PublishTag(const TagID& id) noexcept
      : internal::PublishTagBase{id, internal::index_of<T, PublishValueTypeList>}
    {
      assert(!id.empty());
    }
  }; // class PublishTag

  /** \brief Tag for reduce values
   */
  template<typename T>
  class ReduceValueTag : public internal::ReduceValueTagBase
  {
  public:
    ReduceValueTag(const TagID& id) noexcept
      : internal::ReduceValueTagBase{id, internal::index_of<T, PublishValueTypeList>}
    {
      assert(!id.empty());
    }
  }; // class ReduceValueTag

  /** \brief Tag for reduce groups
   */
  template<typename T>
  class ReduceGroupTag : public internal::ReduceGroupTagBase
  {
  public:
    ReduceGroupTag(const TagID& id) noexcept
      : internal::ReduceGroupTagBase{id, internal::index_of<T, PublishValueTypeList>}
    {
      assert(!id.empty());
    }
  }; // class ReduceGroupTag

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
          // Re-use the buffer mutex here
          std::lock_guard lock{j.bufs_.mutex()};
          // Signify that the work is done
          j.to_run_ = nullptr;
        }};
      }

      static std::mutex& get_mutex(Job& j) noexcept
      {
        return j.bufs_.mutex();
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
    void declare_publication_intent(const std::vector<internal::PublishTagBase>& tags) noexcept;

    /** \brief Retrieves the specified version for the tag, or latest if no version
     * is specified
     *
     * \return A LocalFuture for the value
     */
    template<typename ValueType>
    auto get_future_for(
      const PublishTag<ValueType>& tag,
      const VersionID version = internal::tag_default_version
    ) noexcept
    {
      return internal::make_future(
        bufs_.mutex(),
        data_added_to_buffer_cv_,
        [this, tag, version]() {
          return has_data_no_lock(tag, version);
        },
        [this, tag, version]() {
          const auto variant = get_impl_no_lock(tag, version);
          assert(std::get_if<ValueType>(&variant) != nullptr);
          return *std::get_if<ValueType>(&variant);
        }
      );
    }

    /** \brief Checks if a tag buffer has data or not
     */
    bool has_data(
      const internal::PublishTagBase& tag,
      VersionID version = internal::tag_default_version
    ) noexcept;

    /** \brief Subscribe to all tags passed into the vector.
     *
     * \return A future for when the tags have been subscribed to
     */
    auto subscribe(const std::vector<internal::PublishTagBase>& tags) noexcept
    {
      init_subscribe(tags);
      return get_subscribe_future(tags);
      // return Master::JobAccessor::subscribe(*master_, tags);
    }

    /** \brief Attempts to subscribe to the passed tag
     *
     * \return A future for when the tag is subscribed to
     */
    auto subscribe(const internal::PublishTagBase& tag) noexcept
    {
      return subscribe(std::vector<internal::PublishTagBase>{tag});
    }

    /** \brief Create a reduce group over the specified tags
     */
    template<typename ValueType>
    auto create_reduce_group(
      const ReduceGroupTag<ValueType>& group_tag,
      const ReduceValueTag<ValueType>& tag_produced_for_group,
      const std::vector<ReduceValueTag<ValueType>>& tags
    ) noexcept
    {
      std::vector<TagID> tag_ids(tags.size());
      std::transform(tags.cbegin(), tags.cend(), tag_ids.begin(), [](const auto& t) { return t.id(); });
      const auto tags_to_find = create_reduce_group_init(tag_produced_for_group.id(), tag_ids, group_tag.expected_type());
      return create_reduce_group_future(group_tag.id(), tag_produced_for_group.id(), tags_to_find, group_tag.expected_type())
        .adjust_get_function([](internal::ReduceGroupBase& group) {
          return ReduceGroup<ValueType>(group);
        }
      );
    }

    /** \brief Create a reduce group over the specified tags
     *
     * \return A future for when the reduce group has been created
     */
    // template<typename ReduceTag, typename... ReduceOverTags>
    // auto create_reduce_group(
    //   const ReduceTag& group_tag,
    //   const ReduceTag& tag_produced_for_group,
    //   const ReduceOverTags&... tags
    // ) noexcept
    // {
    //   constexpr auto expected_type =
    //     internal::index_of<typename ReduceTag::ValueType, PublishValueTypeList>;
    //   static_assert(
    //     ((expected_type == internal::index_of<typename ReduceOverTags::ValueType, PublishValueTypeList>) && ...),
    //     "All tags in a reduce group must produce the same type!"
    //   );
    //   const std::vector<std::string> tag_ids{tags.id()...};
    //   return create_reduce_group(group_tag, tag_produced_for_group, tag_ids);
    // }

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
    template<typename T>
    void publish(
      const PublishTag<T>& tag,
      const T& value,
      VersionID version = internal::tag_default_version
    ) noexcept
    {
      publish_impl(tag, value, version);
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
    /** \brief Checks if a buffer has data without locking
     */
    bool has_data_no_lock(const internal::PublishTagBase& tag, VersionID version) noexcept;

    /** \brief Processes the raw information sent from a job on another instance
     *
     * \param tag The id of the tag the data was sent with
     * \param data The data sent on the tag
     * \param version The version of the data
     * \return True if processing went fine, false if there was an error
     */
    bool process_data(const TagID& tag_id, PublishValueVariant data, VersionID version) noexcept;

    void publish_impl(
      const internal::PublishTagBase& tag,
      const PublishValueVariant& to_send,
      VersionID version
    ) noexcept;

    PublishValueVariant get_impl_no_lock(
      const internal::PublishTagBase& tag,
      VersionID version
    ) noexcept;

    void init_subscribe(
      const std::vector<internal::PublishTagBase>& tags
    ) noexcept;

    auto get_subscribe_future(const std::vector<internal::PublishTagBase>& tags) noexcept
      -> internal::Future<void, internal::MasterSubscribeIsDone, internal::FutureGetNoOp>;

    // void unsubscribe_impl(const TagID& tag_id) noexcept;

    // Returns the tags that connections need to be made with
    internal::ReduceGroupNeighbors create_reduce_group_init(
      const TagID& tag_produced,
      const std::vector<TagID>& reduce_over_tags,
      std::uint8_t expected_type
    ) noexcept;

    auto create_reduce_group_future(
      const TagID& group_id,
      const TagID& tag_produced,
      const internal::ReduceGroupNeighbors& tags_to_find,
      std::uint8_t expected_type
    ) noexcept
      -> internal::Future<internal::ReduceGroupBase&, internal::MasterReduceGroupIsCreated, internal::MasterGetReduceGroup>;

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

    // Condition variable when data is added to buffers
    std::condition_variable data_added_to_buffer_cv_;
  }; // Class Job
} // namespace skynet

#endif // SKYNET_JOB_HPP
