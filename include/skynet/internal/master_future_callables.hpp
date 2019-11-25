#ifndef SKYNET_INTERNAL_MASTER_FUTURE_CALLABLES_HPP
#define SKYNET_INTERNAL_MASTER_FUTURE_CALLABLES_HPP

// This header exists so that the Master types returned from the header can be used
// by Job

#include "skynet/types.hpp"

#include <vector>

namespace skynet
{
  class Master;
  namespace internal
  {
    class MasterSubscribeIsDone
    {
    public:
      MasterSubscribeIsDone(Master& master, const std::vector<TagID>& tags) noexcept;
      bool operator()() const noexcept;

    private:
      Master* master_;
      const std::vector<TagID> tags_;
    }; // class MasterSubscribeIsDone
  } // namespace skynet::internal
} // namespace skynet

#endif // SKYNET_INTERNAL_MASTER_FUTURE_CALLABLES_HPP
