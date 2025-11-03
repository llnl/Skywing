#include "skywing_core/neighbor_agent.hpp"
#include "skywing_core/manager.hpp"
#include "skywing_core/internal/utility/logging.hpp"

namespace skywing
{
namespace internal
{
NeighborAgent::NeighborAgent(SocketCommunicator conn,
                             const MachineID& id,
                             const std::vector<MachineID>& neighbors,
                             Manager& manager,
                             SocketAddr const& addr,
                             std::string) noexcept
    : id_{id},
      last_heard_(std::chrono::steady_clock::now()),
      neighbors_{neighbors},
      manager_{&manager},
      addr_{addr}
{
    comms_.push_back(std::move(conn));
}

MachineID NeighborAgent::id() const noexcept
{
    return id_;
}

std::string NeighborAgent::address() const noexcept
{
    if (addr_.is_ipv4()) {
        auto out = comms_[0].ip_address_and_port();
        out.m_port = addr_.port();
        return out.str();
    }
    return addr_.str();
}

SocketAddr NeighborAgent::address_pair() const noexcept
{
    if (addr_.is_ipv4()) {
        auto out = comms_[0].ip_address_and_port();
        out.m_port = addr_.port();
        return out;
    }
    return addr_;
}

bool NeighborAgent::is_dead() const noexcept
{
    return dead_;
}

void NeighborAgent::mark_as_dead() noexcept
{
    dead_ = true;
}

bool NeighborAgent::has_neighbor(const MachineID& id) const noexcept
{
    const auto loc =
        std::lower_bound(neighbors_.cbegin(), neighbors_.cend(), id);
    return loc != neighbors_.cend() && *loc == id;
}

bool NeighborAgent::add_new_neighbor(const MachineID& new_id)
{
  const auto loc = std::lower_bound
    (neighbors_.cbegin(), neighbors_.cend(), new_id);
  // Insert it if it isn't already present
  if (loc == neighbors_.cend() || *loc != new_id) {
    neighbors_.insert(loc, new_id);
    return true;
  }
  return false;
}

bool NeighborAgent::remove_neighbor(const MachineID& id)
{
  const auto loc = std::lower_bound(neighbors_.begin(), neighbors_.end(), id);
  // Neighbors that don't exist will often be reported if it's a
  // shared neighbor and it has already been removed due to the
  // goodbye message
  if (loc != neighbors_.end()) {
    // otherwise just remove it
    std::swap(*loc, neighbors_.back());
    neighbors_.pop_back();
    return true;
  }
  return false;
}

bool NeighborAgent::is_subscribed_to(const TagID& tag) const noexcept
{
    return remote_subscriptions_.find(tag) != remote_subscriptions_.cend();
}

bool NeighborAgent::add_new_subscription(TagID tag)
{
  const auto [iter, inserted] = remote_subscriptions_.emplace(tag);
  (void) iter;
  return inserted;
}

void NeighborAgent::update_time_for_next_request(bool urgent)
{
    if (urgent)
        backoff_counter_ = 0;
    else
        ++backoff_counter_;

    calc_next_request_time();
}

void NeighborAgent::calc_next_request_time() noexcept
{
    using namespace std::chrono_literals;
    static constexpr std::array<std::chrono::milliseconds, 10> backoff_times{
        20ms, 40ms, 80ms, 160ms, 320ms, 500ms, 750ms, 1000ms, 2000ms, 5000ms};
    const auto add_time = backoff_counter_ >= backoff_times.size()
                              ? backoff_times.back()
                              : backoff_times[backoff_counter_];
    request_tags_time_ = std::chrono::steady_clock::now() + add_time;
}

} // namespace internal
} // namespace skywing
