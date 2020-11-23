#include "skynet_core/internal/master_waiter_callables.hpp"

#include "skynet_core/master.hpp"

namespace skynet::internal {
MasterSubscribeIsDone::MasterSubscribeIsDone(Master& master, const std::vector<TagID>& tags) noexcept
  : master_{&master}, tags_{tags}
{}

bool MasterSubscribeIsDone::operator()() const noexcept
{
  return Master::WaiterAccessor::subscribe_is_done(*master_, tags_);
}

MasterReduceGroupIsCreated::MasterReduceGroupIsCreated(Master& master, const TagID& group_id) noexcept
  : master_{&master}, group_id_{group_id}
{}

bool MasterReduceGroupIsCreated::operator()() const noexcept
{
  return Master::WaiterAccessor::reduce_group_is_created(*master_, group_id_);
}

MasterGetReduceGroup::MasterGetReduceGroup(Master& master, const TagID& group_id) noexcept
  : master_{&master}, group_id_{group_id}
{}

ReduceGroupBase& MasterGetReduceGroup::operator()() const noexcept
{
  return Master::WaiterAccessor::get_reduce_group(*master_, group_id_);
}

MasterConnectionIsComplete::MasterConnectionIsComplete(
  Master& master, const std::string& address, std::uint16_t port) noexcept
  : master_{&master}, address_{address, port}
{}

bool MasterConnectionIsComplete::operator()() const noexcept
{
  return Master::WaiterAccessor::conn_is_complete(*master_, address_);
}

MasterGetConnectionSuccess::MasterGetConnectionSuccess(
  Master& master, const std::string& address, std::uint16_t port) noexcept
  : master_{&master}, address_{address, port}
{}

bool MasterGetConnectionSuccess::operator()() const noexcept
{
  return Master::WaiterAccessor::conn_get_success(*master_, address_);
}

MasterIPSubscribeComplete::MasterIPSubscribeComplete(
  Master& master, const AddrPortPair& address, const std::vector<TagID>& tags) noexcept
  : master_{&master}, address_{address}, tags_{tags}
{}

bool MasterIPSubscribeComplete::operator()() const noexcept
{
  // Wait first to see if the connection has finished processing
  if (conn_pending_) {
    if (Master::WaiterAccessor::conn_is_complete(*master_, address_)) { conn_pending_ = false; }
    else {
      return false;
    }
  }
  // If the connection has finished processing, this is complete if either
  // the connection has failed or the subscription has completed
  if (!conn_pending_) {
    if (!Master::WaiterAccessor::conn_get_success(*master_, address_)) { return false; }
    return Master::WaiterAccessor::subscribe_is_done(*master_, tags_);
  }
  // This won't ever actually be hit, just need it to quiet an erroneous warning
  return false;
}

MasterIPSubscribeSuccess::MasterIPSubscribeSuccess(
  Master& master, const AddrPortPair& address, const std::vector<TagID>& tags) noexcept
  : master_{&master}, address_{address}, tags_{tags}
{}

bool MasterIPSubscribeSuccess::operator()() const noexcept
{
  return Master::WaiterAccessor::conn_get_success(*master_, address_)
      && Master::WaiterAccessor::conn_get_success(*master_, address_);
}

} // namespace skynet::internal
