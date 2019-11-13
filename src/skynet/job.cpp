#include "skynet/job.hpp"

#include "skynet/internal/utility/logging.hpp"
#include "skynet/master.hpp"

namespace skynet
{
  Job::Job(
    Accessor::AllowConstruction,
    const std::string& id,
    Master& master,
    std::function<void(Job&)> to_run
  ) noexcept
    : id_{id}
    , master_{&master}
    , to_run_{std::move(to_run)}
  {
    assert(!id.empty());
  }

  bool Job::is_finished() const noexcept
  {
    return to_run_ == nullptr;
  }

  const std::unordered_map<TagID, std::uint8_t>& Job::tags_produced() const noexcept
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
    assert(tags_produced_.find(tag_id) != tags_produced_.cend()
      && "Attempted to publish on a tag that was not declared for publishing!");
    assert(tags_produced_.find(tag_id)->second == to_send.index()
      && "Attempted to publish the wrong type on a tag!");
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
    assert(
      buffers.find(tag_id) != buffers.cend() &&
      buffers.find(tag_id)->second.buffer.has_data(version) &&
      "Attempted to get data for a tag that had no data or was not subscribed to!"
    );
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

  void Job::init_subscribe(
    const std::vector<TagID>& tag_ids,
    const std::vector<std::uint8_t>& expected_types
  ) noexcept
  {
    assert(tag_ids.size() == expected_types.size());
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

  bool Job::is_subscribe_finished(const std::vector<TagID>& tag_ids) noexcept
  {
    return Master::JobAccessor::subscribe(*master_, tag_ids);
  }

  // void Job::unsubscribe_impl(const TagID& tag_id) noexcept
  // {
  //   auto [buffers, lock] = bufs_.get();
  //   (void)lock;
  //   // Just remove any the expected types and data maps
  //   buffers.erase(tag_id);
  // }

  internal::ReduceGroupNeighbors Job::create_reduce_group_init(
    const TagID& tag_produced,
    const std::vector<TagID>& reduce_over_tags,
    std::uint8_t expected_type
  ) noexcept
  {
    assert(
      tags_produced_.find(tag_produced) == tags_produced_.cend() &&
      "Attempted to create a reduce group with a tag that's published on by this type!"
    );
    tags_produced_.try_emplace(tag_produced, expected_type);
    auto bin_tree = reduce_over_tags;
    // A heap can't be used; can produce different ordering depending on the input order
    std::sort(bin_tree.begin(), bin_tree.end());
    const auto index = std::distance(bin_tree.cbegin(), std::find(bin_tree.cbegin(), bin_tree.cend(), tag_produced));
    const auto parent_index = (index - 1) / 2;
    const auto lchild_index = (2 * index) + 1;
    const auto rchild_index = (2 * index) + 2;
    internal::ReduceGroupNeighbors tags_to_find;
    if (index != 0)
    {
      tags_to_find.parent() = bin_tree[parent_index];
    }
    for (const auto child_index : {lchild_index, rchild_index})
    {
      const auto write_index = (child_index == lchild_index ? 1 : 2);
      if (child_index < static_cast<decltype(child_index)>(bin_tree.size()))
      {
        tags_to_find.tags[write_index] = bin_tree[child_index];
      }
    }
    return tags_to_find;
  }

  bool Job::is_reduce_group_created(
    const TagID& tag_produced,
    const TagID& group_tag_id,
    const internal::ReduceGroupNeighbors& tags_to_find,
    const std::uint8_t expected_type
  ) noexcept
  {
    return Master::JobAccessor::create_reduce_group(*master_, tag_produced, group_tag_id, tags_to_find, expected_type);
  }

  void Job::report_tags(const std::vector<TagID>& tags) noexcept
  {
    Master::JobAccessor::report_new_tags_produced(*master_, tags);
  }

  internal::ReduceGroupBase& Job::get_reduce_group(const TagID& group_id) noexcept
  {
    return Master::JobAccessor::get_reduce_group(*master_, group_id);
  }

} // namespace skynet
