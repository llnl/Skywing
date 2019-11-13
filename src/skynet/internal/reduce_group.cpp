#include "skynet/internal/reduce_group.hpp"

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
      return false;
    }
    for (std::size_t i = 0; i < data_buffers_.size(); ++i)
    {
      if (tag == tag_neighbors_.tags[i])
      {
        data_buffers_[i].add(std::move(value), version);
        return true;
      }
    }
    return false;
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
} // namespace skynet::internal
