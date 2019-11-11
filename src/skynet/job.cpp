#include "skynet/job.hpp"

#include "skynet/internal/utility/logging.hpp"
#include "skynet/master.hpp"

namespace skynet
{
  Job::Job(
    Accessor::AllowConstruction,
    const std::string& id,
    Master& master,
    std::vector<TagID> tags,
    std::function<void(Job&)> to_run
  ) noexcept
    : id_{id}
    , master_{&master}
    , to_run_{std::move(to_run)}
    , tags_produced_(std::move(tags))
  {}

  bool Job::is_finished() const noexcept
  {
    return to_run_ == nullptr;
  }

  const std::vector<TagID>& Job::tags_produced() const noexcept
  {
    return tags_produced_;
  }

  /** \brief Processes the raw information sent from a job on another instance
   *
   * \param tag The id of the tag the data was sent with
   * \param data The data sent on the tag
   * \param version The version of the data
   * \return True if processing went fine, false if there was an error
   */
  bool Job::process_data(
    const TagID& tag_id,
    PublishValueVariant data,
    const VersionID version
  ) noexcept
  {
    auto [buffers, lock] = bufs_.get();
    (void)lock;
    const auto loc = buffers.find(tag_id);
    // Not subscribed; don't do anything, but not an error
    if (loc == buffers.cend())
    {
      SKYNET_TRACE_LOG(
        "{}, job {} discarded tag {}, version {}, data {}, due to not being subscribed",
        master_->id(),
        id_,
        tag_id,
        version,
        data
      );
      return true;
    }
    // If the type is wrong then something went wrong
    if (data.index() != loc->second.expected_type)
    {
      SKYNET_WARN_LOG(
        "{}, job {} discarded tag {}, version {}, data {}, due to it having the wrong type index (expected {}, got {})",
        master_->id(),
        id_,
        tag_id,
        version,
        data,
        loc->second.expected_type,
        data.index()
      );
      return false;
    }
    SKYNET_TRACE_LOG(
      "{}, job {} accepted tag {}, version {}, data {}",
      master_->id(),
      id_,
      tag_id,
      version,
      data
    );
    // Otherwise just make it the current value
    loc->second.buffer.add(std::move(data), version);
    return true;
  }

  void Job::publish_impl(
    const TagID& tag_id,
    const PublishValueVariant& to_send,
    const VersionID version
  ) noexcept
  {
    assert(std::find(tags_produced_.cbegin(), tags_produced_.cend(), tag_id) != tags_produced_.cend()
      && "Attempted to publish on a tag that was not declared for publishing!");
    // Find / create the last version and obtain a reference to it
    auto& last_version =
      last_published_version_.try_emplace(tag_id, internal::tag_default_version).first->second;
    last_version = internal::detail::update_version(last_version, version);
    Master::JobAccessor::publish(
      *master_,
      last_version,
      tag_id,
      to_send
    );
  }

  // Implementation of public functions
  PublishValueVariant Job::get_impl(
    const TagID& tag_id,
    const VersionID version
  ) noexcept
  {
    auto [buffers, lock] = bufs_.get();
    (void)lock;
    assert(buffers.find(tag_id) != buffers.cend());
    assert(buffers.find(tag_id)->second.buffer.has_data(version));
    return buffers.find(tag_id)->second.buffer.get();
  }

  bool Job::has_data_impl(const TagID& tag_id, const VersionID version) noexcept
  {
    auto [buffers, lock] = bufs_.get();
    (void)lock;
    const auto loc = buffers.find(tag_id);
    if (loc == buffers.cend())
    {
      return false;
    }
    return loc->second.buffer.has_data(version);
  }

  const JobID& Job::id() const noexcept
  {
    return id_;
  }

  bool Job::subscribe_impl(
    const std::vector<TagID>& tag_ids,
    const std::vector<std::uint8_t>& expected_types
  ) noexcept
  {
    assert(tag_ids.size() == expected_types.size());
    // Put in seperate scope to release the buffer mutex before calling
    // subscribe; not doing this can cause deadlocks
    {
      auto [buffers, lock] = bufs_.get();
      (void)lock;
      // Always subscribe ahead of time, since the gap between the
      // Job::subscribe calls can cause messages to get discarded once the
      // connection is made but before it's marked as subscribed
      for (std::size_t i = 0; i < tag_ids.size(); ++i)
      {
        const auto& tag_id = tag_ids[i];
        const auto& expected_type = expected_types[i];
        // Then add the expected type; marking the tag as watched
        buffers.try_emplace(
          tag_id,
          TagInfo{
            // Just need a dummy value here
            {},
            expected_type
          }
        );
      }
    }
    return Master::JobAccessor::subscribe(*master_, tag_ids);
  }

  // void Job::unsubscribe_impl(const TagID& tag_id) noexcept
  // {
  //   auto [buffers, lock] = bufs_.get();
  //   (void)lock;
  //   // Just remove any the expected types and data maps
  //   buffers.erase(tag_id);
  // }

  // bool Job::create_reduce_group_impl(
  //   const TagID& new_tag_id,
  //   const std::uint8_t expected_type,
  //   const std::vector<TagID>& reduce_over_tags
  // ) noexcept
  // {
  //   // TODO: Send request to master; will be similar to the subscribe stuff
  // }

} // namespace skynet
