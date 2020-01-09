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
#include <numeric>
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

      static void report_dead_tag(Job& j, const TagID& tag) noexcept
      {
        j.mark_tag_as_dead(tag);
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
     * \return A Future for the value
     */
    template<typename ValueType>
    auto get_future_for(
      const PublishTag<ValueType>& tag,
      const VersionID version = internal::tag_default_version
    ) noexcept
    {
      // Can just capture the reference to the value as it
      // will never get invalidated except when the element is deleted
      // due to being in an unordered_map
      auto& buffers = bufs_.unsafe_get();
      const auto tag_iter = buffers.find(tag.id());
      assert(tag_iter != buffers.cend());
      auto& tag_info = tag_iter->second;
      const auto tag_conn_id = tag_info.connection_id;
      return internal::make_future(
        bufs_.mutex(),
        data_buffer_modified_cv_,
        [this, &tag_info, tag_conn_id, version]() {
          return tag_info.buffer.has_data(version)
            || tag_info.error_occurred != TagInfo::Error::no_error
            || tag_info.connection_id != tag_conn_id;
        },
        [this, &tag_info, tag_conn_id, version]() mutable -> std::optional<ValueType> {
          // Don't check tag_info.error_occurred because the connection could have
          // errored between storing the value in the buffer and then retrieving it
          if (tag_info.buffer.has_data(version)
            && tag_info.connection_id == tag_conn_id)
          {
            const auto variant = tag_info.buffer.get();
            assert(std::get_if<ValueType>(&variant) != nullptr);
            return *std::get_if<ValueType>(&variant);
          }
          else
          {
            return {};
          }
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
     * \pre The tags are not currently subscribed to
     * \return A future for when the tags have been subscribed to
     */
    auto subscribe(const std::vector<internal::PublishTagBase>& tags) noexcept
    {
      // Check if any tags are subscribed to
      // (Not seperated out of the assert so that it's only in debug mode)
      // TODO: Make this std::terminate or something instead?
      assert("Tag attempted to be subscribed to twice!" && (
        [&]() {
          const auto [buffers, lock] = bufs_.get();
          (void)lock;
          return std::accumulate(
            tags.cbegin(),
            tags.cend(),
            true,
            [&](const bool missing, const internal::PublishTagBase& tag) {
              return missing && buffers.find(tag.id()) == buffers.cend();
          });
        }()
      ));
      init_or_update_subscribe(tags);
      return get_subscribe_future(tags);
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

    /** \brief Returns if the specified tag has a corresponding connection
     */
    template<typename T>
    bool tag_has_active_publisher(const T& tag) const noexcept
    {
      return tag_has_active_publisher_impl(tag.id());
    }

    /** \brief Rebuilds connections for any missing tags
     *
     * \return A future for when the tags are re-connected
     */
    auto rebuild_missing_tag_connections() noexcept
    {
      // init_or_update_subscribe obtains a lock, so might as well just
      // init this in a lambda (since it can then be const)
      const std::vector<internal::PublishTagBase> tags = [&]() {
        const auto [buffers, lock] = bufs_.get();
        (void)lock;
        std::vector<internal::PublishTagBase> tags;
        for (const auto& tag_pair : buffers)
        {
          // The expected type here doesn't matter
          // Also have to remove the first letter as it identifies the type of
          // tag, but it will just get added again later
          tags.emplace_back(tag_pair.first.substr(1), 0);
        }
        return tags;
      }();
      init_or_update_subscribe(tags);
      return get_subscribe_future(tags);
    }

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

    /** \brief Marks a tag as dead due to connection issues
     *
     * \param tag The id of the tag to mark as dead
     */
    void mark_tag_as_dead(const TagID& tag_id) noexcept;

    void publish_impl(
      const internal::PublishTagBase& tag,
      const PublishValueVariant& to_send,
      VersionID version
    ) noexcept;

    void init_or_update_subscribe(
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

    bool tag_has_active_publisher_impl(const TagID& tag_id) const noexcept;

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
      internal::DiscardOldVersionTagBuffer<PublishValueVariant> buffer;
      // ID for the connection so if a subscription is broken then reformed
      // they can be differentiated
      std::uint16_t connection_id;
      // The expected type
      std::uint8_t expected_type;
      // The error (if any)
      Error error_occurred;
    };
    MutexGuarded<std::unordered_map<std::string, TagInfo>> bufs_;

    // The last version published on each tag
    std::unordered_map<std::string, VersionID> last_published_version_;

    // The master that this job is working with
    Master* master_;

    // The function this job will run
    std::function<void(Job&)> to_run_;

    // The list of tags this job produces and the expected types
    std::unordered_map<TagID, std::uint8_t> tags_produced_;

    // Condition variable when data is added to buffers or an error occurs
    std::condition_variable data_buffer_modified_cv_;
  }; // Class Job
} // namespace skynet

#endif // SKYNET_JOB_HPP
