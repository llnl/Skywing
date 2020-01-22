#include "skynet_core/internal/reduce_group.hpp"

#include "skynet_core/internal/utility/logging.hpp"
#include "skynet_core/master.hpp"

namespace skynet::internal
{
  ReduceGroupBase::ReduceGroupBase(
    const ReduceGroupNeighbors& tag_neighbors,
    Master& master,
    const TagID& group_id,
    const TagID& produced_tag,
    const std::uint8_t expected_type
  ) noexcept
    : tag_neighbors_{tag_neighbors}
    , master_{&master}
    , group_id_{group_id}
    , produced_tag_{produced_tag}
    , expected_type_{expected_type}
  {}

  // Adds data to the corresponding buffer, returning false if an error occurred
  bool ReduceGroupBase::add_data(const TagID& tag, PublishValueVariant value, const VersionID version) noexcept
  {
    if (value.index() != expected_type_)
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
    for (std::size_t i = 0; i < data_buffers_.size(); ++i)
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
          add_data_index(i, std::move(value), version);
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
        const auto iter = last_heard_disconnect.find(initiating_machine);
        if (iter == last_heard_disconnect.cend())
        {
          last_heard_disconnect.try_emplace(iter, initiating_machine, id);
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

  void ReduceGroupBase::add_data_index(const std::size_t index, PublishValueVariant value, const VersionID version) noexcept
  {
    assert(index < 3);
    data_buffers_[index].add(std::move(value), version);
  }

  // Returns true if this handle to the group returns a value on reduce
  bool ReduceGroupBase::returns_value_on_reduce() const noexcept
  {
    return tag_neighbors_.parent().empty();
  }

  void ReduceGroupBase::process_pending_reduce_ops() noexcept
  {
    const auto reduce_is_ready = [&](const VersionID required_version) noexcept {
      if (tag_neighbors_.left_child().empty())
      {
        return true;
      }
      else if (tag_neighbors_.right_child().empty())
      {
        // Left child only
        return data_buffers_[1].has_data(required_version);
      }
      else
      {
        // Both children
        return
          data_buffers_[1].has_data(required_version) &&
          data_buffers_[2].has_data(required_version);
      }
    };
    // Process the reductions in order, until one fails to complete
    for (auto iter = pending_reduces_.begin(); iter != pending_reduces_.end(); iter = pending_reduces_.erase(iter))
    {
      if (!is_valid)
      {
        continue;
      }
      if (!reduce_is_ready(iter->required_version))
      {
        return;
      }
      last_sent_version_ = iter->required_version;
      const PublishValueVariant reduce_result = [&]() -> PublishValueVariant {
        // Three different options - 2 children, left child only, no children
        if (tag_neighbors_.left_child().empty())
        {
          // no children, just propagate value to parent
          return iter->value;
        }
        // Either one or two children, left child is always present
        const auto left_val = data_buffers_[1].get(iter->required_version);
        if (tag_neighbors_.right_child().empty())
        {
          // One child, just apply op with value and propagate value to parent
          const auto reduce_value = iter->operation(left_val, iter->value);
          return reduce_value;
        }
        // Both children
        const auto right_val = data_buffers_[2].get(iter->required_version);
        // Do op(op(left, value), right) so order of evaluation is always the same
        // Also if there are no parents then this will have the final reduce value
        const auto reduce_value = iter->operation(iter->operation(left_val, iter->value), right_val);
        return reduce_value;
      }();
      // Put the result in the buffer so the result can be retrieved if this is the root
      // Otherwise, send the result to the parent
      if (returns_value_on_reduce())
      {
        add_data_index(0, reduce_result, iter->required_version);
        if (iter->is_all_reduce)
        {
          send_value_to_children(reduce_result, iter->required_version);
        }
      }
      else
      {
        send_value_to_parent(reduce_result, iter->required_version);
      }
    }
  }

  const ReduceGroupNeighbors& ReduceGroupBase::tag_neighbors() const noexcept
  {
    return tag_neighbors_;
  }

  const TagID& ReduceGroupBase::produced_tag() const noexcept
  {
    return produced_tag_;
  }

  auto ReduceGroupBase::rebuild() noexcept
    -> Future<void, internal::MasterReduceGroupIsCreated, internal::FutureGetNoOp>
  {
    // Reset the buffers
    last_sent_version_ = tag_no_data;
    is_valid = true;
    return Master::ReduceGroupAccessor::rebuild_reduce_group(*master_, group_id_);
  }

  void ReduceGroupBase::send_value_to_parent(const PublishValueVariant& value_to_send, const VersionID version) noexcept
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


  void ReduceGroupBase::send_value_to_children(const PublishValueVariant& value_to_send, VersionID version) noexcept
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
