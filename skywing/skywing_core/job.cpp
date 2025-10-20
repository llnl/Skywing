#include "skywing_core/job.hpp"

#include <iostream>

#include "skywing_core/internal/utility/logging.hpp"
#include "skywing_core/manager.hpp"

namespace skywing
{

Job::Job(const std::string& id,
         Manager& manager,
         std::function<void(Job&, ManagerHandle)> to_run) noexcept
    : id_{id}, manager_{&manager}, to_run_{std::move(to_run)}
{
    assert(!id.empty());
}

std::thread Job::run() noexcept
{
    return std::thread{[=, this]() {
        // Make the initial neighbor connection here. This
        // is done in this location of the code (as opposed to
        // within the manager) because it must be done asynchronously.
        manager_->make_neighbor_connection();
        to_run_(*this, ManagerHandle{*manager_});
        // Re-use the buffer mutex here
        std::lock_guard lock{subs_.mutex()};
        // Signify that the work is done
        to_run_ = nullptr;
    }};
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
                       std::span<PublishValueVariant> data,
                       const VersionID version) noexcept
{
    auto [subscriptions, lock] = subs_.get();
    (void) lock;
    const auto loc = subscriptions.find(tag_id);
    auto& subscription = loc->second;
    // Not subscribed; don't do anything, but not an error
    if (loc == cend(subscriptions)) {
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
    const auto& expected_types = subscription.get_tag().get_expected_types();
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
        subscription.discard_tag();
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
    subscription.add_data(data, version);
    data_buffer_modified_cv_.notify_all();
    return true;
}

bool Job::tag_has_subscription(const AbstractTag& tag) const noexcept
{
    auto [subscriptions, lock] = subs_.get();
    (void) lock;
    const auto iter = subscriptions.find(tag.id());
    return iter != cend(subscriptions) && !iter->second.has_error();
}

size_t Job::number_of_subscribers(const AbstractTag& tag) const noexcept
{
    return ManagerHandle{*manager_}.number_of_subscribers(tag);
}

void Job::mark_tag_as_dead(const TagID& tag_id) noexcept
{
    SKYWING_TRACE_LOG("\"{}\" tag \"{}\" marked as dead.", id_, tag_id);
    auto [subscriptions, lock] = subs_.get();
    (void) lock;
    const auto tag_loc = subscriptions.find(tag_id);
    if (tag_loc == cend(subscriptions)) {
        return;
    }
    auto& subscription = tag_loc->second;
    subscription.mark_tag_as_dead();
    // TODO: Allow passing multiple tags so the cv is notified a bunch
    // of times if there are many tags?  Errors are expected to be rare
    // so maybe this isn't a problem
    data_buffer_modified_cv_.notify_all();
}

void Job::publish_impl(const AbstractTag& tag,
                       const std::span<PublishValueVariant> to_send) noexcept
{
    assert(tags_produced_.contains(tag.id())
           && "Attempted to publish on a tag that was not declared for "
              "publishing!");
    // assert(tags_produced_.find(tag.id())->second == to_send.index()
    //   && "Attempted to publish the wrong type on a tag!");
    // Find / create the last version and obtain a reference to it
    auto& last_version =
        last_published_version_.try_emplace(tag.id(), tag_no_data)
            .first->second;
    last_version = last_version + 1;
    manager_->publish(last_version, tag.id(), to_send);
}

// Private implementation of public functions
bool Job::has_data(const AbstractTag& tag) noexcept
{
    std::lock_guard<std::mutex> lock{subs_.mutex()};
    return has_data_no_lock(tag);
}

bool Job::has_data_no_lock(const AbstractTag& tag) noexcept
{
    auto& subscriptions = subs_.unsafe_get();
    if (subscriptions.contains(tag.id())) {
        auto& subscription = subscriptions.at(tag.id());
        return subscription.has_data();
    }
    return false;
}

const JobID& Job::id() const noexcept
{
    return id_;
}

Waiter<void>
Job::get_subscribe_future(std::span<const AbstractTag* const> tags) noexcept
{
    std::vector<TagID> tag_ids(tags.size());
    std::transform(cbegin(tags),
                   cend(tags),
                   tag_ids.begin(),
                   [&](auto const& t) { return t->id(); });
    return manager_->subscribe(tag_ids);
}

Waiter<bool>
Job::get_ip_subscribe_future(const std::string& address,
                             std::span<const AbstractTag* const> tags) noexcept
{
    std::vector<TagID> tag_ids(tags.size());
    std::transform(cbegin(tags),
                   cend(tags),
                   tag_ids.begin(),
                   [&](auto const& t) { return t->id(); });
    const auto addr_pair = internal::split_address(address);
    if (addr_pair.address().empty()) {
        std::cerr << fmt::format(
            "Invalid address \"{}\" for Job::ip_subscribe!  Note that a port "
            "must be specified.\n",
            address);
        std::exit(1);
    }
    return manager_->ip_subscribe(addr_pair, tag_ids);
}

void Job::declare_publication_intent_impl(
    std::span<const AbstractTag* const> tags) noexcept
{
    const std::vector<TagID> tag_ids = [&]() {
        std::lock_guard g{subs_.mutex()};
        for (const auto& tag : tags) {
            tags_produced_.try_emplace(tag->id(), tag->get_expected_types());
        }
        std::vector<TagID> tag_ids(tags.size());
        std::transform(cbegin(tags),
                       cend(tags),
                       tag_ids.begin(),
                       [&](auto const& t) { return t->id(); });
        return tag_ids;
    }();
    manager_->report_new_publish_tags(tag_ids);
}

bool Job::tag_has_active_publisher_impl(const TagID& tag_id) const noexcept
{
    auto [subscriptions, lock] = subs_.get();
    (void) lock;
    const auto iter = subscriptions.find(tag_id);
    if (iter == cend(subscriptions)) {
        return false;
    }
    return !iter->second.has_error();
}
} // namespace skywing
