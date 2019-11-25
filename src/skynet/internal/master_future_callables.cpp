#include "skynet/internal/master_future_callables.hpp"

#include "skynet/master.hpp"

namespace skynet::internal
{
  MasterSubscribeIsDone::MasterSubscribeIsDone(Master& master, const std::vector<TagID>& tags) noexcept
    : master_{&master}
    , tags_{tags}
  {}

  bool MasterSubscribeIsDone::operator()() const noexcept
  {
    return Master::FutureAccessor::subscribe_is_done(*master_, tags_);
  }
} // namespace skynet::internal
