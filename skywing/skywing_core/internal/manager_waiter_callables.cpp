#include "skywing_core/internal/manager_waiter_callables.hpp"

#include "skywing_core/manager.hpp"

namespace skywing::internal
{
ManagerSubscribeIsDone::ManagerSubscribeIsDone(
    Manager& manager, const std::vector<TagID>& tags) noexcept
    : manager_{&manager}, tags_{tags}
{}

bool ManagerSubscribeIsDone::operator()() const noexcept
{
    return manager_->subscribe_is_done(tags_);
}

ManagerConnectionIsComplete::ManagerConnectionIsComplete(
    Manager& manager, SocketAddr const& address) noexcept
    : manager_{&manager}, address_{address}
{}

bool ManagerConnectionIsComplete::operator()() const noexcept
{
    return manager_->conn_is_complete(address_);
}

ManagerGetConnectionSuccess::ManagerGetConnectionSuccess(
    Manager& manager, SocketAddr const& address) noexcept
    : manager_{&manager}, address_{address}
{}

bool ManagerGetConnectionSuccess::operator()() const noexcept
{
    return manager_->addr_is_connected(address_);
}

ManagerIPSubscribeComplete::ManagerIPSubscribeComplete(
    Manager& manager,
    const SocketAddr& address,
    const std::vector<TagID>& tags,
    bool is_self_sub) noexcept
    : manager_{&manager},
      address_{address},
      tags_{tags},
      is_self_sub_{is_self_sub}
{}

bool ManagerIPSubscribeComplete::operator()() const noexcept
{
    if (is_self_sub_) {
        return true;
    }
    // Wait first to see if the connection has finished processing
    return manager_->conn_is_complete(address_);
}

ManagerIPSubscribeSuccess::ManagerIPSubscribeSuccess(
    Manager& manager,
    const SocketAddr& address,
    const std::vector<TagID>& tags,
    bool is_self_sub) noexcept
    : manager_{&manager},
      address_{address},
      tags_{tags},
      is_self_sub_{is_self_sub}
{}

bool ManagerIPSubscribeSuccess::operator()() const noexcept
{
    return is_self_sub_
           || (manager_->addr_is_connected(address_)
               && manager_->subscribe_is_done(tags_));
}

} // namespace skywing::internal
