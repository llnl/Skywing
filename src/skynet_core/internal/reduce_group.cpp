#include "skynet_core/internal/reduce_group.hpp"

#include "skynet_core/internal/utility/logging.hpp"
#include "skynet_core/master.hpp"

#include "gsl/span"

namespace skynet::internal
{
  ReduceGroupBase::ReduceGroupBase(
    const ReduceGroupNeighbors& tag_neighbors,
    Master& master,
    const TagID& group_id,
    const TagID& produced_tag,
    const gsl::span<const std::uint8_t> expected_types
  ) noexcept
    : tag_neighbors_{tag_neighbors}
    , master_{&master}
    , group_id_{group_id}
    , produced_tag_{produced_tag}
    , expected_types_{expected_types}
  {}

  // Adds data to the corresponding buffer, returning false if an error occurred
  bool ReduceGroupBase::add_data(const TagID& tag, gsl::span<const PublishValueVariant> value, const VersionID version) noexcept
  {
    const auto comparer = [](const PublishValueVariant& lhs, const std::uint8_t rhs) noexcept {
      return lhs.index() == rhs;
    };
    if (!std::equal(value.cbegin(), value.cend(), expected_types_.cbegin(), expected_types_.cend(), comparer))
    {
      SKYNET_WARN_LOG(
        "\"{}\" rejected data for reduce group \"{}\" for tag \"{}\" version {} due to wrong type",
        master_->id(),
        group_id_,
        tag,
        version
      );
      return false;
    }
    for (std::size_t i = 0; i < tag_neighbors_.tags.size(); ++i)
    {
      if (tag == tag_neighbors_.tags[i])
      {
        SKYNET_TRACE_LOG(
          "\"{}\" added data for reduce group \"{}\" for tag \"{}\" version {}",
          master_->id(),
          group_id_,
          tag,
          version
        );
        {
          std::lock_guard<std::mutex> lock{buffer_mutex_};
          add_data_index(i, value, version);
          if (i == 0)
          {
            send_value_to_children(value, version);
          }
          process_pending_reduce_ops();
        }
        future_info_cv_.notify_all();
        return true;
      }
    }
    SKYNET_WARN_LOG(
      "\"{}\" rejected data for reduce group \"{}\" for tag \"{}\" version {} due to not matching any buffer",
      master_->id(),
      group_id_,
      tag,
      version
    );
    return false;
  }

  void ReduceGroupBase::propagate_disconnection(const MachineID& initiating_machine, ReductionDisconnectID id) noexcept
  {
    {
      std::lock_guard g{buffer_mutex_};
      const bool should_act_on = [&]() {
        const auto iter = last_heard_disconnect_.find(initiating_machine);
        if (iter == last_heard_disconnect_.cend())
        {
          last_heard_disconnect_.try_emplace(iter, initiating_machine, id);
          return true;
        }
        else if (iter->second == id)
        {
          return false;
        }
        else
        {
          iter->second = id;
          return true;
        }
      }();
      if (!should_act_on)
      {
        return;
      }
      // Mark all futures invalid until rebuilding happens
      is_valid = false;
      ++conn_counter;
      send_disconnection(initiating_machine, id);
    }
    future_info_cv_.notify_all();
  }

  void ReduceGroupBase::report_disconnection() noexcept
  {
    {
      std::lock_guard g{buffer_mutex_};
      is_valid = false;
      ++conn_counter;
      send_disconnection(master_->id(), prng());
    }
    future_info_cv_.notify_all();
  }

  // Returns true if this handle to the group returns a value on reduce
  bool ReduceGroupBase::returns_value_on_reduce() const noexcept
  {
    return tag_neighbors_.parent().empty();
  }

  const ReduceGroupNeighbors& ReduceGroupBase::tag_neighbors() const noexcept
  {
    return tag_neighbors_;
  }

  const TagID& ReduceGroupBase::produced_tag() const noexcept
  {
    return produced_tag_;
  }

  const TagID& ReduceGroupBase::group_id() const noexcept
  {
    return group_id_;
  }

  auto ReduceGroupBase::rebuild() noexcept
    -> Waiter<void, internal::MasterReduceGroupIsCreated, internal::WaiterGetNoOp>
  {
    // Reset the buffers
    last_sent_version_ = tag_no_data;
    is_valid = true;
    do_reset_buffers();
    return Master::ReduceGroupAccessor::rebuild_reduce_group(*master_, group_id_);
  }

  void ReduceGroupBase::send_value_to_parent(gsl::span<const PublishValueVariant> value_to_send, const VersionID version) noexcept
  {
    Master::ReduceGroupAccessor::send_reduce_data_to_parent(
      *master_,
      group_id_,
      internal::make_submit_reduce_value(
        group_id_,
        version,
        produced_tag_,
        value_to_send
      )
    );
  }


  void ReduceGroupBase::send_value_to_children(gsl::span<const PublishValueVariant> value_to_send, VersionID version) noexcept
  {
    Master::ReduceGroupAccessor::send_reduce_data_to_children(
      *master_,
      group_id_,
      internal::make_submit_reduce_value(
        group_id_,
        version,
        produced_tag_,
        value_to_send
      )
    );
  }

  void ReduceGroupBase::send_disconnection(const MachineID& initiating_machine, ReductionDisconnectID disconn_id) noexcept
  {
    using func_ptr = decltype(&Master::ReduceGroupAccessor::send_reduce_data_to_children);
    constexpr std::array<func_ptr, 2> senders = {
      &Master::ReduceGroupAccessor::send_reduce_data_to_children,
      &Master::ReduceGroupAccessor::send_reduce_data_to_parent
    };
    for (const auto& sender: senders)
    {
      sender(
        *master_,
        group_id_,
        internal::make_report_reduce_disconnection(
          group_id_,
          initiating_machine,
          disconn_id
        )
      );
    }
  }
} // namespace skynet::internal
