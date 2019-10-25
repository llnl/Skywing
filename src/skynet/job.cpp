#include "skynet/job.hpp"

#include "skynet/master.hpp"

namespace skynet
{
  Job::Job(
    Accessor::AllowConstruction,
    Master& master,
    std::vector<TagID> tags,
    std::function<void(Job&)> to_run
  ) noexcept
    : master_{&master}
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

  VersionID Job::update_version(VersionID& to_update, const VersionID new_version) noexcept
  {
    to_update =
      new_version == tag_default_version
        ? to_update + 1
        : new_version;
    return to_update;
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
    if (loc == buffers.cend()) { return true; }
    // If the type is wrong then something went wrong
    if (data.index() != loc->second.expected_type) { return false; }
    // Otherwise just make it the current value
    loc->second.value = std::move(data);
    loc->second.stored_version = version;
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
      last_published_version_.try_emplace(tag_id, tag_default_version).first->second;
    update_version(last_version, version);
    Master::JobAccessor::publish(
      *master_,
      last_version,
      tag_id,
      to_send
    );
  }

  // Implementation of public functions
  std::optional<PublishValueVariant> Job::get_impl(
    const TagID& tag_id,
    const VersionID version
  ) noexcept
  {
    auto [buffers, lock] = bufs_.get();
    (void)lock;
    auto version_needed = buffers.find(tag_id)->second.last_fetched_version;
    update_version(version_needed, version);
    // Then check if there's been anything seen on the tag and return it if it's
    // at least the required version
    if (const auto loc = buffers.find(tag_id);
      loc != buffers.cend() &&
      loc->second.stored_version != tag_default_version &&
      loc->second.stored_version >= version_needed)
    {
      loc->second.last_fetched_version = version_needed;
      return loc->second.value;
    }
    return {};
  }

  bool Job::has_data_impl(const TagID& tag_id, const VersionID version) noexcept
  {
    auto [buffers, lock] = bufs_.get();
    (void)lock;
    const auto loc = buffers.find(tag_id);
    auto version_needed = buffers.find(tag_id)->second.last_fetched_version;
    update_version(version_needed, version);
    return loc != buffers.cend() &&
      loc->second.stored_version != tag_default_version &&
      loc->second.stored_version >= version_needed;
  }

  bool Job::subscribe_impl(
    const std::vector<TagID>& tag_ids,
    const std::vector<std::uint8_t>& expected_types
  ) noexcept
  {
    assert(tag_ids.size() == expected_types.size());
    auto [buffers, lock] = bufs_.get();
    (void)lock;
    if (Master::JobAccessor::subscribe(*master_, tag_ids))
    {
      for (std::size_t i = 0; i < tag_ids.size(); ++i)
      {
        const auto& tag_id = tag_ids[i];
        const auto& expected_type = expected_types[i];
        // Then add the expected type; marking the tag as watched
        buffers.try_emplace(
          tag_id,
          TagInfo{
            // Just need a dummy value here
            std::int32_t{},
            expected_type,
            tag_default_version,
            tag_default_version
          }
        );
      }
      return true;
    }
    return false;
  }

  // void Job::unsubscribe_impl(const TagID& tag_id) noexcept
  // {
  //   auto [buffers, lock] = bufs_.get();
  //   (void)lock;
  //   // Just remove any the expected types and data maps
  //   buffers.erase(tag_id);
  // }

} // namespace skynet
