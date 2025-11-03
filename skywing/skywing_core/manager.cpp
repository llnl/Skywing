#include "skywing_core/manager.hpp"

#include <iomanip>
#include <iostream>
#include <limits>

#include "skywing_core/internal/message_creators.hpp"
#include "skywing_core/internal/utility/algorithms.hpp"
#include "skywing_core/internal/utility/logging.hpp"
#include "skywing_core/message_handler.hpp"
#include "skywing_core/neighbor_agent.hpp"

namespace skywing
{
namespace
{
// This is more of a stop-gap than anything
std::vector<std::uint8_t>
make_need_one_pub(const std::vector<TagID>& tags) noexcept
{
    return std::vector<std::uint8_t>(tags.size(), 1);
}
} // namespace

////////////////////////////////////////////////
// Class Manager
////////////////////////////////////////////////

Manager::Manager(const std::uint16_t port,
                 const MachineID& id,
                 const heartbeat_cycle heartbeat_interval,
                 const std::size_t neighbor_timeout_factor) noexcept
    : server_socket_{port},
      id_{id},
      heartbeat_interval_{heartbeat_interval},
      neighbor_timeout_threshold_{heartbeat_interval * neighbor_timeout_factor},
      port_{port},
      message_handler_{std::make_unique<internal::MessageHandler>(*this)}
{}

Manager::Manager(std::string const& addr,
                 const MachineID& id,
                 const heartbeat_cycle heartbeat_interval,
                 const std::size_t neighbor_timeout_factor) noexcept
    : server_socket_{addr},
      id_{id},
      heartbeat_interval_{heartbeat_interval},
      neighbor_timeout_threshold_{heartbeat_interval * neighbor_timeout_factor},
      port_{0},
      message_handler_{std::make_unique<internal::MessageHandler>(*this)}
{}

Manager::Manager(SocketAddr const& addr,
                 const MachineID& id,
                 const heartbeat_cycle heartbeat_interval,
                 const std::size_t neighbor_timeout_factor) noexcept
    : server_socket_{addr},
      id_{id},
      heartbeat_interval_{heartbeat_interval},
      neighbor_timeout_threshold_{heartbeat_interval * neighbor_timeout_factor},
      port_{addr.port()},
      message_handler_{std::make_unique<internal::MessageHandler>(*this)}
{}

Manager::~Manager()
{
    send_to_neighbors(internal::make_goodbye());
}

Waiter<bool> Manager::connect_to_server(SocketAddr const& addr) noexcept
{
    std::lock_guard<std::mutex> lock{job_mut_};
    auto canonical = internal::to_canonical(addr);
    // Only actually try the connection if it doesn't already exist
    if (addr_to_machine_.find(canonical) == addr_to_machine_.cend()) {
        const auto [iter, inserted] = pending_conns_.try_emplace(
            canonical,
            PendingInfo{internal::SocketCommunicator{},
                        ConnStatus::waiting_for_conn,
                        ConnType::user_requested,
                        ""});
        if (inserted) {
            const auto status =
                iter->second.conn.connect_non_blocking(canonical);
            // Ignore status - if this initially fails it will be handled later
            (void) status;
            SKYWING_TRACE_LOG("\"{}\" making connection from {} to {}",
                              id_,
                              iter->second.conn.host_ip_address_and_port(),
                              iter->second.conn.ip_address_and_port());
        }
    }
    return make_waiter<bool>(
        job_mut_,
        connection_cv_,
        internal::ManagerConnectionIsComplete{*this, canonical},
        internal::ManagerGetConnectionSuccess{*this, canonical});
}

void Manager::accept_pending_connections() noexcept
{
    while (auto conn = server_socket_.accept()) {
        // This feels gross since it's basically the same thing as above, but
        // I'm not sure how to condense them as they are slightly different
        const auto& [address, port] = conn->ip_address_and_port();
        // NOTE (trb): this is a meaningful use of
        // "SocketCommunicator::ip_address_and_port()"
        auto info = PendingInfo{std::move(*conn),
                                ConnStatus::waiting_for_conn,
                                ConnType::user_requested,
                                ""};
        SKYWING_DEBUG_LOG(
            "\"{}\" accepted connection from {}:{}", id_, address, port);
        // Accept seems to re-use ports, and the actual address doesn't matter,
        // so keep shuffling until it manages to get in
        auto inc_port = port;
        while (true) {
            const auto [iter, inserted] = pending_conns_.try_emplace(
                SocketAddr{address, inc_port}, std::move(info));
            (void) iter;
            ++inc_port;
            if (inserted) {
                SKYWING_DEBUG_LOG("\"{}\" inserted accepted connection from {} "
                                  "into pending_conns_",
                                  id_,
                                  iter->second.conn.ip_address_and_port());
                break;
            }
        }
        // No need for waiters or anything
    }
}

size_t Manager::number_of_neighbors() const noexcept
{
    std::lock_guard<std::mutex> lock{job_mut_};
    return neighbors_.size();
}

void Manager::configure_initial_neighbors(
    std::vector<SocketAddr> const& neighbor_address_port_pairs,
    std::chrono::seconds timeout) noexcept
{
    for (const auto& neighbor : neighbor_address_port_pairs) {
        initial_neighbor_address_port_pairs_.emplace_back(neighbor);
    }
    initial_neighbor_connection_timeout_ = timeout;
}

void Manager::configure_initial_neighbors(
    std::vector<std::tuple<std::string, std::uint16_t>> const&
        neighbor_address_port_pairs,
    std::chrono::seconds timeout) noexcept
{
    for (const auto& [addr, port] : neighbor_address_port_pairs) {
        initial_neighbor_address_port_pairs_.emplace_back(
            SocketAddr{addr, port});
    }
    initial_neighbor_connection_timeout_ = timeout;
}

// NOTE: If calling multiple times, will overwrite the previously set timeout
// values
void Manager::configure_initial_neighbors(SocketAddr const& addr,
                                          std::chrono::seconds timeout) noexcept
{
    initial_neighbor_address_port_pairs_.emplace_back(addr);
    initial_neighbor_connection_timeout_ = timeout;
}

void Manager::make_neighbor_connection() noexcept
{
    for (const auto& addr : initial_neighbor_address_port_pairs_) {
        const auto time_limit = std::chrono::steady_clock::now()
                                + initial_neighbor_connection_timeout_;
        while (!connect_to_server(addr).get()) {
            if (std::chrono::steady_clock::now() > time_limit) {
                SKYWING_DEBUG_LOG("WARNING: Took too long to connect to {}",
                                  addr);
                return;
            }
        }
    }
}

bool Manager::submit_job(
    JobID name, std::function<void(Job&, ManagerHandle)> to_run) noexcept
{
    const auto res = jobs_.try_emplace(name, name, *this, std::move(to_run));
    return res.second;
}

void Manager::run() noexcept
{
    using namespace std::chrono_literals;
    std::vector<std::thread> threads;
    threads.reserve(jobs_.size());
    for (auto& [name, job] : jobs_) {
        (void) name;
        threads.push_back(job.run());
    }
    // Do processing while there are still jobs
    while (!jobs_.empty()) {
        const auto end_sleep_time = std::chrono::steady_clock::now() + 100us;
        {
            // Ensure there's no data race with jobs
            std::lock_guard lock{job_mut_};
            //   Remove any finished jobs
            for (auto iter = jobs_.begin(); iter != jobs_.end();) {
                std::unique_lock lock{iter->second.get_mutex(),
                                      std::try_to_lock};
                if (lock.owns_lock() && iter->second.is_finished()) {
                    // Need to unlock before deallocation
                    lock.unlock();
                    iter = jobs_.erase(iter);
                }
                else {
                    ++iter;
                }
            }
            process_pending_conns();
            accept_pending_connections();
            handle_neighbor_messages();
            for (auto& [id, neighbor] : neighbors_) {
                check_neighbor_timeout(neighbor);
            }
            remove_dead_neighbors();
            find_publishers_for_pending_tags();
            for (auto&& neighbor : neighbors_) {
                message_handler_->send_heartbeat_if_past_interval(
                    neighbor.second, heartbeat_interval_);
            }

            // // Process dynamic neighbor additions and removals
            // process_neighbor_changes();

            using cv_ref_pair = std::pair<bool&, std::condition_variable&>;
            std::array<cv_ref_pair, 2> cv_array{
                cv_ref_pair{notify_subscriptions_, subscription_cv_},
                cv_ref_pair{notify_connection_, connection_cv_}};
            for (auto& [notify, cv] : cv_array) {
                if (notify) {
                    cv.notify_all();
                    notify = false;
                }
            }
        }
        // Wait a bit for other messages
        std::this_thread::sleep_until(end_sleep_time);
    }

    //  Join all of the threads now
    for (auto& thread : threads) {
        thread.join();
    }
}

const std::string& Manager::id() const noexcept
{
    return id_;
}

size_t Manager::number_of_subscribers(const AbstractTag& tag) const noexcept
{
    std::lock_guard<std::mutex> lock{job_mut_};
    const auto self_iter = self_sub_count_.find(tag.id());
    const auto self_subs =
        self_iter == self_sub_count_.cend() ? 0 : self_iter->second;
    return self_subs
           + std::count_if(cbegin(neighbors_),
                           cend(neighbors_),
                           [&tag](const auto& neighbor_pair) noexcept {
                               return neighbor_pair.second.is_subscribed_to(
                                   tag.id());
                           });
}

std::uint16_t Manager::port() const noexcept
{
    return port_;
}

void Manager::handle_neighbor_messages() noexcept
{
    // std::cout << "Agent " << id() << " handling neighbor messages." <<
    // std::endl;
    for (auto&& neighbor : neighbors_) {
        // neighbor.second.get_and_handle_messages();
        message_handler_->get_and_handle_messages(neighbor.second);
    }
}

void Manager::publish(const VersionID version,
                      const TagID& tag_id,
                      std::span<PublishValueVariant> value) noexcept
{
    std::lock_guard lock{job_mut_};
    const auto msg = internal::make_publish(version, tag_id, value);
    (void) msg;
    SKYWING_TRACE_LOG(
        "\"{}\" publishing on tag \"{}\", version \"{}\", data {}",
        id_,
        tag_id,
        version,
        value);
    for (auto& [name, job] : jobs_) {
        (void) name;
        job.process_data(tag_id, value, version);
    }
    send_to_neighbors_if(msg, [&](const auto& neighbor) {
        return neighbor.is_subscribed_to(tag_id);
    });
}

bool Manager::add_data_to_queue(const internal::PublishData& msg) noexcept
{
    for (auto& [name, job] : jobs_) {
        (void) name;
        auto msg_var = msg.value();
        if (!msg_var) {
            return false;
        }
        if (!job.process_data(msg.tag_id(), *msg_var, msg.version())) {
            return false;
        }
    }
    return true;
}

void Manager::notify_of_new_neighbor(const MachineID& id) noexcept
{
    send_to_neighbors_if(internal::make_new_neighbor(id),
                         [&](const internal::NeighborAgent& neighbor) {
                             return neighbor.id() != id;
                         });
}

void Manager::remove_dead_neighbors() noexcept
{
    bool new_tags = false;
    for (auto it = neighbors_.begin(); it != neighbors_.end(); /* nothing */) {
        if (it->second.is_dead()) {
            SKYWING_TRACE_LOG(
                "\"{}\" removing dead neighbor \"{}\"", id_, it->first);
            cleanup_neighbor_state(it->first, it->second, new_tags);
            // This could affect subscriptions, so notify anything waiting on
            // them
            notify_subscriptions_ = true;
            send_to_neighbors(internal::make_remove_neighbor(it->first));
            it = neighbors_.erase(it);
        }
        else {
            ++it;
        }
    }
    // Do this after removing the neighbors so that the dead neighbors won't be
    // considered
    if (new_tags) {
        SKYWING_TRACE_LOG(
            "\"{}\" finding publishers for new tag after neighbor removal",
            id_);
        find_publishers_for_pending_tags(true);
    }
}

/**
 * @brief Cleans up local neighbor related state for a dead agent.
 * @param id The neighbor's machine id
 * @param neighbor The neighbor to clean up state for.
 * @param new_tags true if neighbor removal affects tags
 */
void Manager::cleanup_neighbor_state([[maybe_unused]] const MachineID& id,
                                     internal::NeighborAgent& neighbor,
                                     bool& new_tags) noexcept
{
    SKYWING_TRACE_LOG("\"{}\" cleaning up state for neighbor \"{}\"", id_, id);

    // Remove corresponding address
    const auto erase_addr = [&](auto& erase_from, const auto& on_erase) {
        const auto addr_matches = [&](const auto& pair) {
            return pair.second == &neighbor;
        };
        const auto find_next = [&](const auto& iter) {
            return std::find_if(iter, erase_from.end(), addr_matches);
        };
        for (auto iter = find_next(erase_from.begin());
             iter != erase_from.end();
             iter = find_next(iter))
        {
            on_erase(*iter);
            iter = erase_from.erase(iter);
        }
    };

    // CVP: Also need to remove from publishers_for_tag_?
    erase_addr(addr_to_machine_, [](const auto&) {});
    // Need to re-look for the subscription tags, if any
    erase_addr(tag_to_machine_, [&](const auto& tag_pair) {
        new_tags = true;
        for (auto& job_pair : jobs_) {
            job_pair.second.mark_tag_as_dead(tag_pair.first);
        }
        pending_tags_.emplace_back(tag_pair.first);
    });
}

/**
 * @brief Checks if a neighbor has timed out based on the last time it was heard
 * from.
 *
 * This function assumes that the caller has already acquired the `job_mut_`
 * mutex. As such, it does **not** acquire the lock locally to avoid a
 * self-deadlock.
 *
 * In particular, `Manager::run()` holds `job_mut_` for the duration of its main
 * loop and calls this method within that locked context.
 *
 * @warning Do not call this function outside of a context where `job_mut_` is
 * already held. Acquiring the lock locally in this function would lead to a
 * deadlock since `std::mutex` is not reentrant.
 *
 * @param neighbor The neighbor to check for timeout.
 */
void Manager::check_neighbor_timeout(internal::NeighborAgent& neighbor) noexcept
{
    auto elapsed =
        std::chrono::steady_clock::now() - neighbor.time_last_heard();

    if (!neighbor.is_dead() && elapsed > neighbor_timeout_threshold_) {
        neighbor.mark_as_dead();
        SKYWING_TRACE_LOG(
            "\"{}\" marked neighbor \"{}\" as dead after {}ms (timeout "
            "threshold: {}ms).",
            id_,
            neighbor.id(),
            to_ms(elapsed),
            to_ms(neighbor_timeout_threshold_));
    }
}

std::vector<MachineID> Manager::make_neighbor_vector() const noexcept
{
    std::vector<MachineID> to_ret(neighbors_.size());
    std::transform(neighbors_.cbegin(),
                   neighbors_.cend(),
                   to_ret.begin(),
                   [](const auto& val) { return val.first; });
    return to_ret;
}

template <typename Callable>
void Manager::send_to_neighbors_if(const std::vector<std::byte>& to_send,
                                   Callable condition) noexcept
{
    for (auto&& neighbor : neighbors_) {
        if (condition(neighbor.second)) {
            message_handler_->send_message(neighbor.second, to_send);
        }
    }
}

void Manager::send_to_neighbors(const std::vector<std::byte>& to_send) noexcept
{
    send_to_neighbors_if(to_send,
                         [](const internal::NeighborAgent&) { return true; });
}

bool Manager::subscribe_is_done(
    const std::vector<TagID>& required_tags) const noexcept
{
    for (const auto& tag : required_tags) {
        if (self_sub_count_.find(tag) != self_sub_count_.cend()) {
            continue;
        }
        const auto iter = tag_to_machine_.find(tag);
        if (iter == tag_to_machine_.cend()) {
            return false;
        }
    }
    SKYWING_DEBUG_LOG(
        "\"{}\" subscription for tags {} finished.", id_, required_tags);
    return true;
}

Waiter<void> Manager::subscribe(const std::vector<TagID>& tag_ids) noexcept
{
    std::lock_guard lock{job_mut_};
    SKYWING_DEBUG_LOG(
        "\"{}\" initializing subscription for tags {}", id_, tag_ids);

    // insert each tag id into list of pending tags (ie tags for which
    // we're still looking for a publisher) if it's not already being
    // looked for (ie already in pending_tags_) and not already found
    // (ie in tag_to_machine_)
    std::copy_if(
        tag_ids.cbegin(),
        tag_ids.cend(),
        std::back_inserter(pending_tags_),
        [&](const TagID& to_find) {
            if (tag_to_machine_.find(to_find) != tag_to_machine_.cend()) {
                return false;
            }
            if (std::find(pending_tags_.cbegin(), pending_tags_.cend(), to_find)
                != pending_tags_.cend())
            {
                return false;
            }
            return true;
        });

    // if there are any tags for which we need to find a publisher,
    // start the search.
    if (!pending_tags_.empty()) {
        message_handler_->find_publishers_for_tags(
            neighbors_, tag_ids, make_need_one_pub(tag_ids));
    }
    // Can potentially finish subscribing right away, so notify things
    notify_subscriptions_ = true;
    return make_waiter(job_mut_,
                       subscription_cv_,
                       internal::ManagerSubscribeIsDone{*this, tag_ids});
}

Waiter<bool> Manager::ip_subscribe(const SocketAddr& addr,
                                   const std::vector<TagID>& tag_ids) noexcept
{
    std::lock_guard lock{job_mut_};
    const auto canonical_addr = internal::to_canonical(addr);
    const auto iter = addr_to_machine_.find(canonical_addr);
    // Handle self-subscription
    bool is_self_sub = false;
    if (internal::to_ip_port(canonical_addr)
        == internal::to_ip_port({"localhost", port_}))
    {
        for (const auto& tag : tag_ids) {
            is_self_sub = true;
            const auto iter = self_sub_count_.find(tag);
            if (iter == self_sub_count_.cend()) {
                std::cerr << "Tag \"" << tag
                          << "\" was attempted to be self-subscribed but it "
                             "isn't produced!\n";
                std::exit(4);
            }
            iter->second += 1;
        }
        notify_subscriptions_ = true;
    }
    else if (iter != addr_to_machine_.cend()) {
        message_handler_->send_message(
            *(iter->second),
            internal::make_subscription_notice(tag_ids, false));

        notify_subscriptions_ = true;
    }
    else {
        // Put together IP address/port and tags
        const std::string tag_list = std::accumulate(
            tag_ids.cbegin(),
            tag_ids.cend(),
            canonical_addr.address() + ':'
                + std::to_string(canonical_addr.port()),
            [](const std::string& so_far, const std::string& next) {
                return so_far + '\0' + next;
            });
        const auto [iter, inserted] = pending_conns_.try_emplace(
            canonical_addr,
            PendingInfo{internal::SocketCommunicator{},
                        ConnStatus::waiting_for_conn,
                        ConnType::specific_ip,
                        tag_list});
        assert(inserted);
        // Ignore the status - it is handeled later
        (void) iter->second.conn.connect_non_blocking(canonical_addr);
    }
    return make_waiter<bool>(job_mut_,
                             subscription_cv_,
                             internal::ManagerIPSubscribeComplete{
                                 *this, canonical_addr, tag_ids, is_self_sub},
                             internal::ManagerIPSubscribeSuccess{
                                 *this, canonical_addr, tag_ids, is_self_sub});
}

void Manager::handle_get_publishers(const internal::GetPublishers& msg,
                                    internal::NeighborAgent& from) noexcept
{
    // If all of the tag requirements are fulfilled then
    const auto [remaining_tags, num_left] =
        remove_tags_with_enough_publishers(msg);
    if (remaining_tags.empty()) {
        SKYWING_TRACE_LOG("\"{}\" sending \"{}\" publisher information for {}, "
                          "all tags have been fulfilled",
                          id_,
                          from.id(),
                          msg.tags());
        // Send the information back now
        message_handler_->send_message(from,
                                       make_known_tag_publisher_message());
    }
    else {
        // Mark all tags from the message in the cache so that they will be
        // sent back so that the receiving end no longer thinks they are pending
        // Also clear them if the cache is being ignored, as it is assumed that
        // they are now invalid
        for (const auto& tag : remaining_tags) {
            const auto& [iter, inserted] = publishers_for_tag_.try_emplace(tag);
            (void) inserted;
            if (msg.ignore_cache()) {
                iter->second.clear();
            }
        }
        // If there are no other neighbors, just answer right away so
        // it doesn't stall
        if (neighbors_.size() == 1) {
            SKYWING_TRACE_LOG("\"{}\" sending \"{}\" publisher information for "
                              "{}, no neighbors to ask",
                              id_,
                              from.id(),
                              [&]() {
                                  std::vector<TagID> known_tags;
                                  for (const auto& [tag, publishers] :
                                       publishers_for_tag_) {
                                      if (!publishers.empty()) {
                                          known_tags.push_back(tag);
                                      }
                                  }
                                  return known_tags;
                              }());
            message_handler_->send_message(from,
                                           make_known_tag_publisher_message());
            return;
        }
        // Mark the information as needing to be propagated
        for (const auto& tag : remaining_tags) {
            auto [iter, dummy] =
                send_publisher_information_to_.try_emplace(tag);
            iter->second.emplace(from.id());
            (void) dummy;
        }
        // If there's already a pending request another one can't be sent, so
        // just return now
        if (from.has_outstanding_publishers_request()) {
            SKYWING_TRACE_LOG("\"{}\" returning early for request for tags {} "
                              "from \"{}\" to avoid potential deadlock",
                              id_,
                              msg.tags(),
                              from.id());
            message_handler_->send_message(from,
                                           make_known_tag_publisher_message());
            // No longer need to propagate information to this neighbor, as it
            // is being sent now
            send_publisher_information_to_.erase(from.id());
        }
        else {
            SKYWING_TRACE_LOG(
                "\"{}\" asking neighbors {} for tags {} for \"{}\"",
                id_,
                make_neighbor_vector(),
                msg.tags(),
                from.id());
            message_handler_->find_publishers_for_tags(
                neighbors_, remaining_tags, num_left);
        }
    }
}

auto Manager::remove_tags_with_enough_publishers(
    const internal::GetPublishers& msg) noexcept
    -> std::pair<std::vector<TagID>, std::vector<std::uint8_t>>
{
    auto tags_left = msg.tags();
    auto publishers_needed = msg.publishers_needed();
    // Remove tags that either have a known producer or are known locally
    const auto [tag_iter, num_iter] =
        std::remove_if(
            internal::zip_iter_equal_len(tags_left.begin(),
                                         publishers_needed.begin()),
            internal::zip_iter_equal_len(tags_left.end(),
                                         publishers_needed.end()),
            [&](const auto& id_left) {
                const auto& [tag, num_left] = id_left;
                // TODO: How to handle self-subscription with this?
                // Just count it as an additional source for now, but presumably
                // just having it be valid no matter what is the best option
                // going forward (why would you not trust yourself?)
                const auto self_subscribed =
                    self_sub_count_.find(tag) != self_sub_count_.cend();
                const auto loc = publishers_for_tag_.find(tag);
                const auto num_external_pubs =
                    loc == publishers_for_tag_.cend() ? 0 : loc->second.size();
                return num_external_pubs + self_subscribed >= num_left;
            })
            .underlying_iters();
    tags_left.erase(tag_iter, tags_left.end());
    publishers_needed.erase(num_iter, publishers_needed.end());
    return {tags_left, publishers_needed};
}

void Manager::add_publishers_and_propagate(
    const internal::ReportPublishers& msg,
    const internal::NeighborAgent& from) noexcept
{
    const auto insert_publisher_infos =
        [](decltype(publishers_for_tag_)::iterator iter,
           const std::vector<SocketAddr>& addresses,
           const std::vector<MachineID>& machines) noexcept {
            assert(addresses.size() == machines.size());
            const auto num_iters = addresses.size();
            for (std::size_t i = 0; i < num_iters; ++i) {
                iter->second.emplace(
                    internal::PublisherInfo{addresses[i], machines[i]});
            }
        };
    const auto tags = msg.tags();
    const auto publishers_list = msg.addresses();
    const auto machines_list = msg.machines();
    if (tags.size() != publishers_list.size()
        || tags.size() != machines_list.size())
    {
        // TODO: Propagate this information back and disconnect from the
        // neighbor
        SKYWING_WARN_LOG(
            "\"{}\" received tag/publisher list size mismatch from \"{}\"",
            id_,
            from.id());
        return;
    }
    // Add the information to what is locally known
    for (std::size_t i = 0; i < tags.size(); ++i) {
        const auto& tag = tags[i];
        const auto& publishers = publishers_list[i];
        const auto& machines = machines_list[i];
        // Find or create the tag
        decltype(publishers_for_tag_)::iterator iter = [&]() noexcept {
            const auto loc = publishers_for_tag_.find(tag);
            if (loc == publishers_for_tag_.end()) {
                const auto [iter, inserted] =
                    publishers_for_tag_.try_emplace(tag);
                (void) inserted;
                return iter;
            }
            else {
                return loc;
            }
        }();
        insert_publisher_infos(iter, publishers, machines);
    }
    // Add the tags that the neighbor produced
    const auto external_tags = msg.locally_produced_tags();
    for (const auto& tag : external_tags) {
        const decltype(publishers_for_tag_)::iterator iter = [&]() noexcept {
            const auto loc = publishers_for_tag_.find(tag);
            if (loc == publishers_for_tag_.end()) {
                using SecondType =
                    decltype(publishers_for_tag_)::value_type::second_type;
                const auto [iter, inserted] =
                    publishers_for_tag_.insert_or_assign(tag, SecondType{});
                return iter;
            }
            return loc;
        }();
        SKYWING_DEBUG_LOG(
            "TOM Add pubs and prop, tags from neighbors addr=\"{}\"",
            from.address());
        iter->second.insert(
            internal::PublisherInfo{from.address_pair(), from.id()});
    }
    // Propagate to any machines that need this information, marking them
    // as no longer needing propagation as well
    std::unordered_set<MachineID> machines_to_send_to;
    for (const auto& [tag, data] : publishers_for_tag_) {
        (void) data;
        const auto loc = send_publisher_information_to_.find(tag);
        if (loc != send_publisher_information_to_.cend()) {
            internal::merge_associative_containers(machines_to_send_to,
                                                   loc->second);
            send_publisher_information_to_.erase(loc);
        }
    }
    if (!machines_to_send_to.empty()) {
        const auto to_send = make_known_tag_publisher_message();
        // Send to the machines if they are present
        for (const auto& send_to : machines_to_send_to) {
            const auto loc = neighbors_.find(send_to);
            if (loc != neighbors_.end()) {
                SKYWING_TRACE_LOG(
                    "\"{}\" propagating back to \"{}\" with local tags {}",
                    id_,
                    send_to,
                    local_tags());
                message_handler_->send_message(loc->second, to_send);
            }
        }
    }
    init_connections_for_pending_tags();
}

std::vector<std::byte>
Manager::make_known_tag_publisher_message() const noexcept
{
    // Produce vectors for the machines and tags
    std::vector<TagID> tags_to_send;
    std::vector<std::vector<SocketAddr>> addresses_to_send;
    std::vector<std::vector<MachineID>> machines_to_send;
    for (const auto& [tag, infos] : publishers_for_tag_) {
        // Don't send data for tags that don't have any known publishers
        if (!infos.empty()) {
            auto& new_addrs = addresses_to_send.emplace_back();
            auto& new_machines = machines_to_send.emplace_back();
            new_addrs.reserve(infos.size());
            new_machines.reserve(infos.size());
            for (const auto& [addr, machine] : infos) {
                new_addrs.push_back(addr);
                new_machines.push_back(machine);
            }
            tags_to_send.push_back(tag);
        }
    }
    return internal::make_report_publishers(
        tags_to_send, addresses_to_send, machines_to_send, local_tags());
}

void Manager::report_new_publish_tags(const std::vector<TagID>& tags) noexcept
{
    std::lock_guard lock{job_mut_};
    SKYWING_TRACE_LOG("\"{}\" adding tags produced: {}", id_, tags);
    // Mark the tags produced by this job
    for (const auto& tag : tags) {
        const auto [iter, inserted] = self_sub_count_.emplace(tag, 0);
        (void) iter;
        if (!inserted) {
            // Two jobs on the same manager can't produce the same tag; fail
            // loudly
            std::cerr << "The tag " << std::quoted(tag)
                      << " was reported for publication more than once!\n";
            std::exit(1);
        }
    }
    // Notify publish groups for self-subscribing
    notify_subscriptions_ = true;
}

void Manager::init_connections_for_pending_tags() noexcept
{
    if (!pending_tags_.empty()) {
        SKYWING_TRACE_LOG(
            "\"{}\" is initiating connections for tags {}", id_, pending_tags_);
    }
    // A single connection can supply multiple tags, so look through all the
    // pending tags first so that multiple connections to the same machine
    // aren't started
    std::unordered_map<SocketAddr, std::string> to_conn;
    std::vector<decltype(pending_tags_)::iterator> to_delete;

    SKYWING_TRACE_LOG("\"{}\" in init_connections_for_pending_tags for "
                      "pendings_tags list of size {}",
                      id_,
                      pending_tags_.size());
    // for (const auto& [tag, publishers] : publishers_for_tag_)
    // {
    //   SKYWING_TRACE_LOG("\"{}\" knows {} publishers for tag {}", id_,
    //   publishers.size(), tag);
    // }

    for (auto tag_iter = pending_tags_.begin();
         tag_iter != pending_tags_.end();)
    {
        const auto& tag = *tag_iter;
        const auto iter = publishers_for_tag_.find(tag);
        // Delete pending tags for self-published tags
        if (const auto self_iter = self_sub_count_.find(tag);
            self_iter != self_sub_count_.cend())
        {
            ++self_iter->second;
            SKYWING_TRACE_LOG(
                "\"{}\" produces tag \"{}\", not creating connection",
                id_,
                tag);
            tag_iter = pending_tags_.erase(tag_iter);
            // This counts as a subscription change, make sure to notify things
            notify_subscriptions_ = true;
            continue;
        }
        if (iter == publishers_for_tag_.cend()) {
            SKYWING_TRACE_LOG(
                "\"{}\" knows no publishers for tag \"{}\"", id_, tag);
            ++tag_iter;
            continue;
        }

        auto& publishers = iter->second; // a unordered_set<PublisherInfo>
        if (publishers.empty()) {
            SKYWING_TRACE_LOG(
                "\"{}\" knows no publishers for tag \"{}\"", id_, tag);
            ++tag_iter;
        }
        else {
            const auto& [addr, connect_to_id] =
                *publishers.begin(); // a PublisherInfo object
            // Check if the machine is already a neighbor, and handle it if so
            const auto neighbor_iter = addr_to_machine_.find(addr);
            SKYWING_DEBUG_LOG("neighbor iter has address \"{}\"", addr);
            if (neighbor_iter != addr_to_machine_.cend()) {
                SKYWING_TRACE_LOG(
                    "\"{}\" already has connection for tag \"{}\"", id_, tag);
                assert(neighbor_iter->second);
                // Make sure the address matches the id
                if (neighbor_iter->second->id() != connect_to_id) {
                    SKYWING_WARN_LOG("\"{}\" was told id for address \"{}\" is "
                                     "\"{}\", locally id is \"{}\"",
                                     id_,
                                     addr,
                                     connect_to_id,
                                     neighbor_iter->second->id());
                    ++tag_iter;
                }
                else {
                    finalize_subscription(tag, *neighbor_iter->second);
                    tag_iter = pending_tags_.erase(tag_iter);
                }
            }
            else {
                const auto [addrstr, port] = addr;

                if (reconnecting_addrs_.count(addr)) {
                    SKYWING_TRACE_LOG("\"{}\" is skipping new subscription to "
                                      "{}:{} because reconnect is in progress",
                                      id_,
                                      addrstr,
                                      port);
                    ++tag_iter;
                    continue;
                }
                SKYWING_TRACE_LOG(
                    "\"{}\" will try to connect to {} \"{}\"", id_, addr, tag);
                auto [conn_iter, inserted] = to_conn.try_emplace(addr, tag);
                // Append tag to "list" if already there
                if (!inserted) {
                    SKYWING_TRACE_LOG(
                        "\"{}\" didn't insert {} because already in to_conn",
                        id_,
                        addr);
                    auto& tag_list = conn_iter->second;
                    if (!tag_list.empty()) {
                        tag_list.push_back('\0');
                    }
                    tag_list += tag;
                }
                to_delete.push_back(tag_iter);
                ++tag_iter;
            }
            // CVP: I'm pretty sure erasing this is incorrect because this
            // knowledge must be kept during make_known_tag_publisher_message()
            // publishers.erase(publishers.begin());
        }
    }
    for (const auto& [addr, tag] : to_conn) {
        internal::SocketCommunicator conn{};
        SKYWING_DEBUG_LOG(
            "\"{}\" about to connect to \"{}\" for tag \"{}\"", id_, addr, tag);

        auto const& key = addr;

        // SocketAddr key{addrstr, port};
        // Check if a reconnect connection is already pending for this
        // address/port
        auto pending_iter = pending_conns_.find(key);
        if (pending_iter != pending_conns_.end()
            && pending_iter->second.type == ConnType::reconnect)
        {
            SKYWING_DEBUG_LOG("\"{}\" skipping subscription connect to {} "
                              "due to reconnect in progress",
                              id_,
                              key);

            continue; // Skip this connection, reconnect will handle it
        }

        const auto err = conn.connect_non_blocking(key);
        if (err == internal::ConnectionError::connection_in_progress
            || err == internal::ConnectionError::no_error)
        {
            // Port can be recycled, so have to iterate until it gets inserted
            // Ignore the address as the IP isn't initialized until the
            // connection is complete
            auto [addrstr, port] = addr;
            while (true) {
                SKYWING_DEBUG_LOG(
                    "\"{}\" trying connecting to \"{}\" with key {} for tag "
                    "\"{}\"",
                    id_,
                    addr,
                    addr,
                    tag);
                const auto [iter, inserted] = pending_conns_.try_emplace(
                    SocketAddr{addrstr, port},
                    PendingInfo{std::move(conn),
                                ConnStatus::waiting_for_conn,
                                ConnType::subscription,
                                tag});
                (void) iter;
                if (inserted) {
                    SKYWING_DEBUG_LOG(
                        "\"{}\" connecting to \"{}\" for tag \"{}\"",
                        id_,
                        iter->first,
                        tag);
                    break;
                }
                ++port;
            }
        }
    }
    std::for_each(to_delete.rbegin(), to_delete.rend(), [&](const auto& iter) {
        pending_tags_.erase(iter);
    });
}

bool Manager::conn_is_complete(const SocketAddr& address) noexcept
{
    return pending_conns_.find(address) == pending_conns_.cend();
}

bool Manager::addr_is_connected(const SocketAddr& address) const noexcept
{
    const auto iter = addr_to_machine_.find(address);
    if (iter == addr_to_machine_.cend()) {
        return false;
    }
    return !iter->second->is_dead();
}

const char* Manager::to_c_str(ConnType type) noexcept
{
    switch (type) {
    case ConnType::user_requested: return "user_requested";
    case ConnType::by_accept: return "by_accept";
    case ConnType::subscription: return "subscription";
    case ConnType::specific_ip: return "specific_ip";
    case ConnType::reconnect: return "reconnect";
    }
    // This should never be reached
    assert(false);
    return "unknown type";
}

void Manager::process_pending_conns() noexcept
{
    bool new_pending_tags = false;
    // TODO: Move this into its own function?  It isn't used anywhere else...
    const auto handle_error = [&](PendingInfo& info) {
        const auto handle_tag = [&](const std::string& pub_tag,
                                    const std::string& base_tag) {
            new_pending_tags = true;
            const auto pub_iter = publishers_for_tag_.find(pub_tag);
            assert(pub_iter != publishers_for_tag_.cend());
            auto& publishers = pub_iter->second;
            // Set to ignore cache if there are no more publishers
            if (publishers.empty()) {
                SKYWING_TRACE_LOG(
                    "\"{}\" ran out of publishers for tag \"{}\", "
                    "look for new ones.",
                    id_,
                    info.tag);
                message_handler_->set_must_find_more_publishers(true);
            }
            else {
                SKYWING_TRACE_LOG("\"{}\" still has publishers for tag \"{}\", "
                                  "going to next one",
                                  id_,
                                  info.tag);
            }
            // Just replace the tag to re-init the connection
            pending_tags_.emplace_back(std::string{base_tag});
        };
        switch (info.type) {
        case ConnType::by_accept:
        case ConnType::user_requested:
        case ConnType::specific_ip:
            // nothing special needs to happen
            break;
        case ConnType::reconnect: break;
        case ConnType::subscription:
        {
            const auto tags = internal::split(info.tag, '\0');
            for (const auto& tag : tags) {
                const std::string tag_str{tag};
                handle_tag(tag_str, tag_str);
            }
        } break;
        }
    };
    for (auto iter = pending_conns_.begin(); iter != pending_conns_.end();) {
        // I don't like this okay variable, but I can't think of a better way
        bool okay = true;
        auto& info = iter->second;
        if (info.status == ConnStatus::waiting_for_conn) {
            const auto init_status = info.conn.connection_progress_status();
            switch (init_status) {
            case internal::ConnectionError::connection_in_progress: break;

            case internal::ConnectionError::no_error:
            {
                std::vector<std::byte> message =
                    (info.type == ConnType::reconnect) ? make_reconnect()
                                                       : make_handshake();

                if (info.conn.send_message(message.data(), message.size())
                    != internal::ConnectionError::no_error)
                {
                    notify_connection_ = true;
                    iter = pending_conns_.erase(iter);
                    continue;
                }

                if (info.type == ConnType::reconnect) {
                    SKYWING_TRACE_LOG("\"{}\" sent reconnect from {} to {}",
                                      id_,
                                      info.conn.host_ip_address_and_port(),
                                      info.conn.ip_address_and_port());
                }
                else {
                    SKYWING_TRACE_LOG(
                        "\"{}\" sent greeting from {} to {} for tag \"{}\"",
                        id_,
                        info.conn.host_ip_address_and_port(),
                        info.conn.ip_address_and_port(),
                        info.tag);
                }

                info.status = ConnStatus::waiting_for_resp;
            } break;

            // Anything else is an error
            default:
                if (iter->second.type == Manager::ConnType::subscription)
                    SKYWING_WARN_LOG("\"{}\" errored trying to connect to {}, "
                                     "type {}, tag {}",
                                     id_,
                                     iter->first,
                                     to_c_str(info.type),
                                     info.tag);
                else {
                    SKYWING_WARN_LOG(
                        "\"{}\" errored trying to connect to {}, type {}",
                        id_,
                        iter->first,
                        to_c_str(info.type));

                    handle_error(info);
                    notify_connection_ = true;
                    iter = pending_conns_.erase(iter);
                    okay = false;
                    break;
                }
            }
        }
        else if (info.status == ConnStatus::waiting_for_resp) {
            // TODO: Add timeout here?
            // Try to read message from the connection
            const auto bytes_to_read_or_error = read_network_size(info.conn);
            if (std::holds_alternative<internal::ConnectionError>(
                    bytes_to_read_or_error))
            {
                const auto err = *std::get_if<internal::ConnectionError>(
                    &bytes_to_read_or_error);
                if (err != internal::ConnectionError::would_block) {
                    okay = false;
                }
            }
            else {
                const auto bytes_to_read =
                    *std::get_if<NetworkSizeType>(&bytes_to_read_or_error);
                if (const auto message_buffer =
                        internal::read_chunked(info.conn, bytes_to_read);
                    !message_buffer.empty())
                {
                    if (const auto msg =
                            internal::MessageDeserializer::try_to_create(
                                message_buffer))
                    {
                        decltype(neighbors_)::iterator new_neighbor_iter;
                        okay =
                            msg->do_callback(
                                [&](const internal::Greeting& greeting) {
                                    // add connection to active list / remove
                                    // from pending list
                                    auto [neighbor_iter, inserted] =
                                        neighbors_.try_emplace(
                                            greeting.from(),
                                            std::move(info.conn),
                                            greeting.from(),
                                            greeting.neighbors(),
                                            *this,
                                            greeting.address(),
                                            "FIXME");
                                    new_neighbor_iter = neighbor_iter;
                                    if (!inserted) {
                                        SKYWING_TRACE_LOG(
                                            "\"{}\" already has a connection "
                                            "from "
                                            "\"{}\" so will simply add to "
                                            "communicators.",
                                            id_,
                                            neighbor_iter->first);
                                        new_neighbor_iter->second
                                            .add_communicator(
                                                std::move(info.conn));
                                        return true;
                                    }
                                    addr_to_machine_.try_emplace(
                                        new_neighbor_iter->second
                                            .address_pair(),
                                        &neighbor_iter->second);
                                    SKYWING_TRACE_LOG(
                                        "\"{}\" received greeting from \"{}\"",
                                        id_,
                                        neighbor_iter->first);
                                    return true;
                                },
                                [&](const internal::Reconnect& reconnect) {
                                    // New reconnect handler
                                    auto [neighbor_iter, inserted] =
                                        neighbors_.try_emplace(
                                            reconnect.from(),
                                            std::move(info.conn),
                                            reconnect.from(),
                                            reconnect.neighbors(),
                                            *this,
                                            reconnect.address(),
                                            "FIXME");

                                    new_neighbor_iter = neighbor_iter;
                                    if (!inserted) {
                                        SKYWING_TRACE_LOG(
                                            "\"{}\" already has a connection "
                                            "from "
                                            "\"{}\" so will simply add to "
                                            "communicators.",
                                            id_,
                                            neighbor_iter->first);
                                        new_neighbor_iter->second
                                            .add_communicator(
                                                std::move(info.conn));
                                        return true;
                                    }
                                    addr_to_machine_.try_emplace(
                                        new_neighbor_iter->second
                                            .address_pair(),
                                        &neighbor_iter->second);
                                    SKYWING_TRACE_LOG(
                                        "\"{}\" received reconnect from \"{}\"",
                                        id_,
                                        neighbor_iter->first);
                                    return true;
                                },
                                [&](...) {
                                    SKYWING_WARN_LOG(
                                        "\"{}\" received unexpected message "
                                        "from "
                                        "\"{}\", expected greeting",
                                        id_,
                                        iter->first);
                                    return false;
                                })
                            && okay;
                        if (okay) {
                            SKYWING_TRACE_LOG("\"{}\" finalizing connection to "
                                              "\"{}\" for tag \"{}\"",
                                              id_,
                                              iter->first,
                                              info.tag);
                            switch (info.type) {
                            case ConnType::by_accept:
                            case ConnType::user_requested: break;
                            case ConnType::reconnect: break;

                            case ConnType::subscription:
                                finalize_subscription(
                                    info.tag, new_neighbor_iter->second);
                                break;

                            case ConnType::specific_ip:
                            {
                                auto& new_neighbor = new_neighbor_iter->second;
                                // Erroring is different here because the tag
                                // shouldn't be marked as being wanted
                                // Furthermore, specific IP uses the
                                // subscription CV, not the connection one
                                const auto on_error = [&]() {
                                    new_neighbor.mark_as_dead();
                                    iter = pending_conns_.erase(iter);
                                    notify_subscriptions_ = true;
                                };
                                const auto ip_and_tag =
                                    internal::split(info.tag, '\0', 2);
                                assert(ip_and_tag.size() == 2);
                                const auto expected_ip = ip_and_tag[0];
                                const auto tags = ip_and_tag[1];
                                if (new_neighbor.address() != expected_ip) {
                                    SKYWING_ERROR_LOG(
                                        "Neighbor IP \"{}\" didn't match with "
                                        "expected IP \"{}\"!",
                                        new_neighbor.address(),
                                        expected_ip);
                                    on_error();
                                    continue;
                                }
                                finalize_subscription(std::string{tags},
                                                      new_neighbor);
                            } break;
                            }
                            // These will always happen at the end
                            notify_of_new_neighbor(new_neighbor_iter->first);
                            find_publishers_for_pending_tags();
                            // Finally, remove the pending connection and
                            // re-loop
                            notify_connection_ = true;
                            iter = pending_conns_.erase(iter);
                            continue;
                        }
                    }
                    else {
                        okay = false;
                    }
                }
            }
            if (!okay) {
                SKYWING_WARN_LOG(
                    "\"{}\" failed connecting to {} for tag \"{}\"",
                    id_,
                    info.conn.ip_address_and_port(),
                    info.tag);
                notify_connection_ = true;
                handle_error(info);
                iter = pending_conns_.erase(iter);
            }
        }
        if (okay) {
            ++iter;
        }
    }
    // Find publishers if there are new pending tags
    if (new_pending_tags) {
        find_publishers_for_pending_tags();
        init_connections_for_pending_tags();
    }
}

std::vector<std::byte> Manager::make_handshake() const noexcept
{
    auto const addr = server_socket_.listening_addr();
    if (addr.port() != port_) {
        std::cerr << "BAD PORT (addr.port() = " << addr.port()
                  << ", port_ = " << port_ << ")" << std::endl;
        std::terminate(); // sanity check in debug mode
    }
    return internal::make_greeting(id_, make_neighbor_vector(), addr);
}

std::vector<std::byte> Manager::make_reconnect() const noexcept
{
    auto const addr = server_socket_.listening_addr();
    assert(addr.port() == port_); // sanity check in debug mode
    return internal::make_reconnect(id_, make_neighbor_vector(), addr);
}

bool Manager::subscription_tags_are_produced(
    const internal::SubscriptionNotice& msg) const noexcept
{
    const auto& tags = msg.tags();
    for (const auto& tag : tags) {
        if (self_sub_count_.find(tag) == self_sub_count_.cend()) {
            return false;
        }
    }
    return true;
}

bool Manager::handle_publish_data(const internal::PublishData& msg,
                                  const internal::NeighborAgent& from) noexcept
{
    (void) from;
    if (auto value = msg.value()) {
        SKYWING_TRACE_LOG("\"{}\" received data on tag \"{}\" from \"{}\", "
                          "version {}, data: {}",
                          id_,
                          msg.tag_id(),
                          from.id(),
                          msg.version(),
                          *value);
        bool okay = true;
        for (auto& [job_id, job] : jobs_) {
            (void) job_id;
            okay =
                job.process_data(msg.tag_id(), *value, msg.version()) && okay;
        }
        return okay;
    }
    else {
        return false;
    }
}

void Manager::finalize_subscription(const std::string& tags,
                                    internal::NeighborAgent& source) noexcept
{
    const auto tags_str_view = internal::split(tags, '\0');
    SKYWING_TRACE_LOG(
        "\"{}\" finalizing subscription for tags {} with machine {}",
        id_,
        tags_str_view,
        source.id());
    std::vector<TagID> tags_to_sub_to;
    std::transform(tags_str_view.cbegin(),
                   tags_str_view.cend(),
                   std::back_inserter(tags_to_sub_to),
                   [](const std::string_view v) { return std::string{v}; });
    for (const auto& tag : tags_to_sub_to) {
        tag_to_machine_[tag] = &source;
    }
    const auto msg = internal::make_subscription_notice(tags_to_sub_to, false);
    message_handler_->send_message(source, msg);
    notify_subscriptions_ = true;
}

void Manager::find_publishers_for_pending_tags(const bool force_ask) noexcept
{
    if (force_ask) {
        SKYWING_TRACE_LOG(
            "\"{}\" forcefully asking for {}", id_, pending_tags_);
        message_handler_->find_publishers_for_tags(
            neighbors_, pending_tags_, make_need_one_pub(pending_tags_));
    }
    else {
        const auto no_known_publishers = [&](const TagID& tag) noexcept {
            if (tag_to_machine_.find(tag) != tag_to_machine_.cend()) {
                return false;
            }
            const auto iter = publishers_for_tag_.find(tag);
            if (iter == publishers_for_tag_.cend()) {
                return true;
            }
            return iter->second.empty();
        };
        std::vector<TagID> to_ask_for;
        std::copy_if(pending_tags_.cbegin(),
                     pending_tags_.cend(),
                     std::back_inserter(to_ask_for),
                     no_known_publishers);
        if (!to_ask_for.empty()) {
            // find publishers for tags only if necessary (false param)
            message_handler_->find_publishers_for_tags(
                neighbors_, to_ask_for, make_need_one_pub(to_ask_for), false);
        }
    }
}

std::vector<TagID> Manager::local_tags() const noexcept
{
    std::vector<TagID> to_ret(self_sub_count_.size());
    std::transform(self_sub_count_.cbegin(),
                   self_sub_count_.cend(),
                   to_ret.begin(),
                   [](const auto& tag_pair) { return tag_pair.first; });
    return to_ret;
}

bool Manager::request_disconnect(const MachineID& neighbor_agent_id) noexcept
{
    std::lock_guard<std::mutex> lock{job_mut_};
    auto it = neighbors_.find(neighbor_agent_id);
    if (it == neighbors_.end()) {
        SKYWING_WARN_LOG("Neighbor ID {} not found in \"{}\"'s neighbors map; "
                         "not disconnecting.",
                         neighbor_agent_id,
                         id_);
        return false;
    }

    if (it->second.is_dead()) {
        SKYWING_TRACE_LOG("Neighbor \"{}\" already marked dead in \"{}\"",
                          neighbor_agent_id,
                          id_);
        neighbors_.erase(it);
        return true;
    }
    bool new_tags = false;
    reconnect_cache[neighbor_agent_id] = it->second.address_pair();
    cleanup_neighbor_state(it->first, it->second, new_tags);

    // SocketCommunicator destructor called here to close neighbor socket.
    neighbors_.erase(neighbor_agent_id);

    return true;
}

bool Manager::request_reconnect(const MachineID& neighbor_agent_id) noexcept
{
    if (!reconnect_cache.contains(neighbor_agent_id)) {
        SKYWING_WARN_LOG("Neighbor ID {} not found in \"{}\"'s reconnect map; "
                         "not reconnecting.",
                         neighbor_agent_id,
                         id_);
        return false;
    }

    auto const& server_address = reconnect_cache.at(neighbor_agent_id);

    reconnecting_addrs_.insert(server_address);
    SKYWING_DEBUG_LOG(
        "\"{}\" added reconnect address {} to reconnecting_addrs_",
        id_,
        server_address);

    internal::SocketCommunicator new_conn;
    auto err = new_conn.connect_non_blocking(server_address);
    if (err != internal::ConnectionError::no_error
        && err != internal::ConnectionError::connection_in_progress)
    {
        SKYWING_DEBUG_LOG("Reconnect failed immediately with error {}",
                          static_cast<int>(err));
        reconnecting_addrs_.erase(server_address);
        return false;
    }

    PendingInfo info{
        std::move(new_conn), ConnStatus::waiting_for_conn, ConnType::reconnect};

    auto [iter, inserted] =
        pending_conns_.try_emplace(server_address, std::move(info));
    if (!inserted) {
        SKYWING_WARN_LOG(
            "Failed to insert reconnect pending connection id {} for {}",
            info.id,
            server_address);
        reconnecting_addrs_.erase(server_address);
        return false;
    }

    SKYWING_DEBUG_LOG(
        "Reconnect initiated with pending id {} and stored for {}",
        iter->second.id,
        server_address);
    return true;
}

bool Manager::request_reconnect_with_retry(const MachineID& neighbor_agent_id,
                                           int max_retries) noexcept
{
    for (int attempt = 0; attempt < max_retries; ++attempt) {
        if (request_reconnect(neighbor_agent_id)) {
            SKYWING_DEBUG_LOG("Reconnect succeeded on attempt {}", attempt + 1);
            return true;
        }
        SKYWING_WARN_LOG("Reconnect attempt {} failed for {}",
                         attempt + 1,
                         neighbor_agent_id);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    SKYWING_ERROR_LOG("All reconnect attempts failed for {}",
                      neighbor_agent_id);
    return false;
}

} // namespace skywing
