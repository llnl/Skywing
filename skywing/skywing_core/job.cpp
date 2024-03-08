#include "skywing_core/job.hpp"

#include <iostream>

#include "skywing_core/internal/utility/logging.hpp"
#include "skywing_core/manager.hpp"

namespace skywing
{
std::thread Job::Accessor::run(Job& j) noexcept
{
    return std::thread{[&j]() {
        j.to_run_(j, ManagerHandle{*j.manager_});
        // Re-use the buffer mutex here
        std::lock_guard lock{j.bufs_.mutex()};
        // Signify that the work is done
        j.to_run_ = nullptr;
    }};
}

Job::Job(Accessor::AllowConstruction,
         const std::string& id,
         Manager& manager,
         std::function<void(Job&, ManagerHandle)> to_run) noexcept
    : id_{id}, manager_{&manager}, to_run_{std::move(to_run)}
{
    assert(!id.empty());
}

bool Job::is_finished() const noexcept
{
    return to_run_ == nullptr;
}

const std::unordered_map<TagID, std::span<const std::uint8_t>>&
Job::tags_produced() const noexcept
{
    return tags_produced_;
}

bool Job::process_data(const TagID& tag_id,
                       std::span<const PublishValueVariant> data,
                       const VersionID version) noexcept
{
    auto [buffers, lock] = bufs_.get();
    (void) lock;
    const auto loc = buffers.find(tag_id);
    // Not subscribed; don't do anything, but not an error
    if (loc == cend(buffers)) {
        SKYWING_TRACE_LOG(
            "\"{}\", job \"{}\" discarded tag \"{}\", version {}, "
            "data {}, due to not being subscribed",
            manager_->id(),
            id_,
            tag_id,
            version,
            data);
        return true;
    }
    // If the types are wrong then something went wrong
    const auto comparer = [](std::uint8_t lhs, const PublishValueVariant& rhs) {
        return lhs == rhs.index();
    };
    const auto& expected_types = loc->second.expected_types;
    if (!std::equal(cbegin(expected_types),
                    cend(expected_types),
                    cbegin(data),
                    cend(data),
                    comparer))
    {
        SKYWING_WARN_LOG("\"{}\", job \"{}\" discarded tag \"{}\", version {}, "
                         "data {}, due to it having the wrong type index",
                         manager_->id(),
                         id_,
                         tag_id,
                         version,
                         data);
        loc->second.error_occurred = TagInfo::Error::incorrect_type;
        data_buffer_modified_cv_.notify_all();
        return false;
    }
    SKYWING_TRACE_LOG(
        "\"{}\", job \"{}\" accepted tag \"{}\", version {}, data {}",
        manager_->id(),
        id_,
        tag_id,
        version,
        data);
    // Otherwise just make it the current value
    loc->second.buffer->add(data, version);
    data_buffer_modified_cv_.notify_all();
    return true;
}

bool Job::tag_has_subscription(const AbstractTag& tag) const noexcept
{
  auto [buffers, lock] = bufs_.get();
  (void)lock;
  const auto iter = buffers.find(tag.get_id());
  return iter != cend(buffers) && iter->second.error_occurred == TagInfo::Error::no_error;
}

bool Job::tags_have_subscriptions_impl(std::span<const AbstractTag> tags) const noexcept
{
  auto [buffers, lock] = bufs_.get();
  (void)lock;
  for (const auto& tag : tags) {
    const auto iter = buffers.find(tag.get_id());
    if (iter == cend(buffers) || iter->second.error_occurred != TagInfo::Error::no_error) { return false; }
  }
  return true;
}

size_t Job::number_of_subscribers(const AbstractTag& tag) const noexcept
{
    return ManagerHandle{*manager_}.number_of_subscribers(tag);
}

void Job::mark_tag_as_dead(const TagID& tag_id) noexcept
{
    SKYWING_TRACE_LOG("\"{}\" tag \"{}\" marked as dead.", id_, tag_id);
    auto [buffers, lock] = bufs_.get();
    (void) lock;
    const auto tag_loc = buffers.find(tag_id);
    if (tag_loc == cend(buffers)) {
        return;
    }
    auto& tag_info = tag_loc->second;
    tag_info.error_occurred = TagInfo::Error::disconnected;
    ++tag_info.connection_id;
    // TODO: Allow passing multiple tags so the cv is notified a bunch
    // of times if there are many tags?  Errors are expected to be rare
    // so maybe this isn't a problem
    data_buffer_modified_cv_.notify_all();
}

void Job::publish_impl(const AbstractTag& tag, const std::span<PublishValueVariant> to_send) noexcept
{
  assert(
    tags_produced_.find(tag.id()) != cend(tags_produced_)
    && "Attempted to publish on a tag that was not declared for publishing!");
  // assert(tags_produced_.find(tag.id())->second == to_send.index()
  //   && "Attempted to publish the wrong type on a tag!");
  // Find / create the last version and obtain a reference to it
  auto& last_version = last_published_version_.try_emplace(tag.get_id(), internal::tag_no_data).first->second;
  last_version = last_version + 1;
  Manager::JobAccessor::publish(*manager_, last_version, tag.get_id(), to_send);
}

// Private implementation of public functions
bool Job::has_data(const AbstractTag& tag) noexcept
{
    std::lock_guard<std::mutex> lock{bufs_.mutex()};
    return has_data_no_lock(tag);
}

bool Job::has_data_no_lock(const AbstractTag& tag) noexcept
{
  auto& buffers = bufs_.unsafe_get();
  const auto loc = buffers.find(tag.get_id());
  if (loc == cend(buffers)) { return false; }
  return loc->second.buffer->has_data();
}

const JobID& Job::id() const noexcept
{
    return id_;
}

void Job::init_or_update_subscribe(
  std::span<std::unique_ptr<const AbstractTag>> tags,
  std::span<std::unique_ptr<internal::DiscardOldVersionTagBufferBase>> ptrs) noexcept
{
  assert(tags.size() == ptrs.size());
  auto [buffers, lock] = bufs_.get();
  (void)lock;
  // Always subscribe ahead of time, since the gap between the
  // Job::subscribe calls can cause messages to get discarded once the
  // connection is made but before it's marked as subscribed
  for (size_t i = 0; i < tags.size(); ++i) {
    auto& ptr = ptrs[i];
    // Then add the expected type; marking the tag as watched
    const auto [iter, inserted] = buffers.try_emplace(
      tags[i]->get_id(),
      TagInfo{// Just need a dummy value here
              std::move(ptr),
              tags[i]->get_expected_types(),
              0,
              TagInfo::Error::no_error});
    // Already exists - update the connection id and reset the buffer / error
    if (!inserted) {
      ++iter->second.connection_id;
      // Reset it to a default constructed buffer
      iter->second.buffer->reset();
      iter->second.error_occurred = TagInfo::Error::no_error;
    }
  }
}

Waiter<void> Job::get_subscribe_future(std::span<std::unique_ptr<const AbstractTag>> tags) noexcept
{
  std::vector<TagID> tag_ids(tags.size());
  std::transform(
    cbegin(tags), cend(tags), tag_ids.begin(), [&](std::unique_ptr<const AbstractTag>& t) { return t->get_id(); });
  return Manager::JobAccessor::subscribe(*manager_, tag_ids);
}

Waiter<bool>
  Job::get_ip_subscribe_future(const std::string& address, std::span<std::unique_ptr<const AbstractTag>> tags) noexcept
{
  std::vector<TagID> tag_ids(tags.size());
  std::transform(
    cbegin(tags), cend(tags), tag_ids.begin(), [&](std::unique_ptr<const AbstractTag>& t) { return t->get_id(); });
  const auto addr_pair = internal::split_address(address);
  if (addr_pair.first.empty()) {
    std::cerr << fmt::format(
      "Invalid address \"{}\" for Job::ip_subscribe!  Note that a port must be specified.\n", address);
    std::exit(1);
  }
  return Manager::JobAccessor::ip_subscribe(*manager_, addr_pair, tag_ids);
}

void Job::declare_publication_intent_impl(std::span<const AbstractTag> tags) noexcept
{
  const std::vector<TagID> tag_ids = [&]() {
    std::lock_guard g{bufs_.mutex()};
    for (const auto& tag : tags) {
      tags_produced_.try_emplace(tag.get_id(), tag.get_expected_types());
    }
    std::vector<TagID> tag_ids(tags.size());
    std::transform(cbegin(tags), cend(tags), tag_ids.begin(), [&](const AbstractTag& t) { return t.get_id(); });
    return tag_ids;
  }();
  Manager::JobAccessor::report_new_publish_tags(*manager_, tag_ids);
}

void Job::declare_publication_intent_impl(std::span<std::unique_ptr<const AbstractTag>> tags) noexcept
{
  const std::vector<TagID> tag_ids = [&]() {
    std::lock_guard g{bufs_.mutex()};
    for (const auto& tag : tags) {
      tags_produced_.try_emplace(tag->get_id(), tag->get_expected_types());
    }
    std::vector<TagID> tag_ids(tags.size());
    std::transform(cbegin(tags), cend(tags), tag_ids.begin(), [&](const std::unique_ptr<const AbstractTag>& t) {
      return t->get_id();
    });
    return tag_ids;
  }();
  Manager::JobAccessor::report_new_publish_tags(*manager_, tag_ids);
}

// void Job::unsubscribe_impl(const TagID& tag_id) noexcept
// {
//   auto [buffers, lock] = bufs_.get();
//   (void)lock;
//   // Just remove any the expected types and data maps
//   buffers.erase(tag_id);
// }

bool Job::tag_has_active_publisher_impl(const TagID& tag_id) const noexcept
{
    auto [buffers, lock] = bufs_.get();
    (void) lock;
    const auto iter = buffers.find(tag_id);
    if (iter == cend(buffers)) {
        return false;
    }
    return iter->second.error_occurred == TagInfo::Error::no_error;
}
} // namespace skywing
