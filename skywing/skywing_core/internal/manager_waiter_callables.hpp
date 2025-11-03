#ifndef SKYWING_INTERNAL_MANAGER_WAITER_CALLABLES_HPP
#define SKYWING_INTERNAL_MANAGER_WAITER_CALLABLES_HPP

// This header exists so that the Manager types returned from the header can be
// used by Job

#include <vector>

#include "skywing_core/types.hpp"

namespace skywing
{
class Manager;

namespace internal
{

class ManagerSubscribeIsDone
{
public:
    ManagerSubscribeIsDone(Manager& manager,
                           const std::vector<TagID>& tags) noexcept;
    bool operator()() const noexcept;

private:
    Manager* manager_;
    std::vector<TagID> tags_;
}; // class ManagerSubscribeIsDone

class ManagerConnectionIsComplete
{
public:
    ManagerConnectionIsComplete(Manager& manager,
                                SocketAddr const& address) noexcept;
    bool operator()() const noexcept;

private:
    Manager* manager_;
    SocketAddr address_;
}; // class ManagerConnectionIsComplete

class ManagerGetConnectionSuccess
{
public:
    ManagerGetConnectionSuccess(Manager& manager,
                                SocketAddr const& address) noexcept;
    bool operator()() const noexcept;

private:
    Manager* manager_;
    SocketAddr address_;
}; // class ManagerGetConnectionSuccess

class ManagerIPSubscribeComplete
{
public:
    ManagerIPSubscribeComplete(Manager& manager,
                               const SocketAddr& address,
                               const std::vector<TagID>& tags,
                               bool is_self_sub) noexcept;
    bool operator()() const noexcept;

private:
    Manager* manager_;
    SocketAddr address_;
    std::vector<TagID> tags_;
    bool is_self_sub_;
};

class ManagerIPSubscribeSuccess
{
public:
    ManagerIPSubscribeSuccess(Manager& manager,
                              const SocketAddr& address,
                              const std::vector<TagID>& tags,
                              bool is_self_sub) noexcept;
    bool operator()() const noexcept;

private:
    Manager* manager_;
    SocketAddr address_;
    std::vector<TagID> tags_;
    bool is_self_sub_;
};
} // namespace internal
} // namespace skywing

#endif // SKYWING_INTERNAL_MANAGER_WAITER_CALLABLES_HPP
