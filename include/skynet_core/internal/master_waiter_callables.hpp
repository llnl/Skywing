#ifndef SKYNET_INTERNAL_MASTER_WAITER_CALLABLES_HPP
#define SKYNET_INTERNAL_MASTER_WAITER_CALLABLES_HPP

// This header exists so that the Master types returned from the header can be used
// by Job

#include "skynet_core/types.hpp"

#include <vector>

namespace skynet
{
  class Master;

  namespace internal
  {
    class ReduceGroupBase;

    class MasterSubscribeIsDone
    {
    public:
      MasterSubscribeIsDone(Master& master, const std::vector<TagID>& tags) noexcept;
      bool operator()() const noexcept;

    private:
      Master* master_;
      std::vector<TagID> tags_;
    }; // class MasterSubscribeIsDone

    class MasterReduceGroupIsCreated
    {
    public:
      MasterReduceGroupIsCreated(
        Master& master,
        const TagID& group_id
      ) noexcept;
      bool operator()() const noexcept;

    private:
      Master* master_;
      TagID group_id_;
    }; // class MasterReduceGroupIsCreated

    class MasterGetReduceGroup
    {
    public:
      MasterGetReduceGroup(Master& master, const TagID& group_id) noexcept;
      ReduceGroupBase& operator()() const noexcept;

    private:
      Master* master_;
      TagID group_id_;
    }; // class MasterGetReduceGroup

    class MasterConnectionIsComplete
    {
    public:
      MasterConnectionIsComplete(
        Master& master,
        const std::string& address,
        std::uint16_t port
      ) noexcept;
      bool operator()() const noexcept;

    private:
      Master* master_;
      AddrPortPair address_;
    }; // class MasterConnectionIsComplete

    class MasterGetConnectionSuccess
    {
    public:
      MasterGetConnectionSuccess(
        Master& master,
        const std::string& address,
        std::uint16_t port
      ) noexcept;
      bool operator()() const noexcept;

    private:
      Master* master_;
      AddrPortPair address_;
    }; // class MasterGetConnectionSuccess
  } // namespace skynet::internal
} // namespace skynet

#endif // SKYNET_INTERNAL_MASTER_WAITER_CALLABLES_HPP
