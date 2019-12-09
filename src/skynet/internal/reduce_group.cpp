#include "skynet/internal/reduce_group.hpp"

#include "skynet/internal/utility/logging.hpp"
#include "skynet/master.hpp"

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
          process_pending_reduce_ops();
        }
        data_added_to_buffers_cv_.notify_all();
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

  void ReduceGroupBase::add_data_index(const std::size_t index, PublishValueVariant value, const VersionID version) noexcept
  {
    assert(index < 3);
    // If the result was added to the parent buffer then it is the result of a reduce
    // and should be propagated to the children
    if (index == 0)
    {
      send_value_to_children(value, version);
    }
    data_buffers_[index].add(std::move(value), version);
  }

  // Returns true if this handle to the group returns a value on reduce
  bool ReduceGroupBase::returns_value_on_reduce() const noexcept
  {
    return tag_neighbors_.parent().empty();
  }

  void ReduceGroupBase::process_pending_reduce_ops() noexcept
  {
    const auto reduce_is_ready = [&](const VersionID required_version) noexcept{
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
        send_value_to_children(reduce_result, iter->required_version);
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

  void ReduceGroupBase::send_value_to_parent(const PublishValueVariant& value_to_send, const VersionID version) noexcept
  {
    Master::ReduceGroupAccessor::send_reduce_value_to_parent(
      *master_,
      group_id_,
      version,
      produced_tag_,
      value_to_send
    );
  }


  void ReduceGroupBase::send_value_to_children(const PublishValueVariant& value_to_send, VersionID version) noexcept
  {
    Master::ReduceGroupAccessor::send_reduce_value_to_children(
      *master_,
      group_id_,
      version,
      produced_tag_,
      value_to_send
    );
  }
} // namespace skynet::internal
