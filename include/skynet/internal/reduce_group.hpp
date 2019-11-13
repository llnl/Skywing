#ifndef SKYNET_INTERNAL_REDUCE_GROUP_HPP
#define SKYNET_INTERNAL_REDUCE_GROUP_HPP

#include "skynet/internal/tag_buffer.hpp"
#include "skynet/local_future.hpp"
#include "skynet/types.hpp"

#include <cassert>
#include <optional>

namespace skynet
{
  class Master;
} // namespace skynet

namespace skynet::internal
{
  // TODO: Move into seperate .cpp
  class ReduceGroupBase
  {
  public:
    explicit ReduceGroupBase(
      const std::uint8_t expected_type,
      const ReduceGroupNeighbors& tag_neighbors,
      Master& master
    ) noexcept;

    // Adds data to the corresponding buffer, returning false if an error occurred
    bool add_data(const TagID& tag, PublishValueVariant value, const VersionID version) noexcept;

    // Returns true if this handle to the group returns a value on reduce
    bool returns_value_on_reduce() const noexcept;

    const ReduceGroupNeighbors& tag_neighbors() const noexcept;

    // Sends a value to the parent
    void send_value_to_parent(const PublishValueVariant& value_to_send, VersionID version) noexcept;

  protected:
    using DataBuffer = FifoTagBuffer<PublishValueVariant>;
    std::array<DataBuffer, 3> data_buffers_;
    ReduceGroupNeighbors tag_neighbors_;
    Master* master_;
    std::uint8_t expected_type_;
  }; // class ReduceGroupBase

  template<typename T>
  class ReduceGroup : public ReduceGroupBase
  {
  public:
    // TODO: This is currently blocking, going to end up changing the interfaces and such later,
    // so fix this when that's done
    template<typename Callable>
    std::optional<T> reduce(
      const T& value,
      Callable reduce_op,
      VersionID version = tag_default_version
    ) noexcept
    {
      // Three different options - 2 children, left child only, no children
      if (this->tag_neighbors_.left_child().empty())
      {
        // no children, just propagate value to parent
        send_value_to_parent(value, version);
        return {};
      }
      // Either one or two children, left child is always present
      auto left_fut = make_local_future(
        [&]() { return this->data_buffers_[1].has_data(version); },
        [&]() { return std::get<T>(this->data_buffers_[1].get(version)); }
      );
      if (this->tag_neighbors_.right_child().empty())
      {
        // One child, just apply op with value and propagate value to parent
        send_value_to_parent(reduce_op(left_fut.get(), value), version);
        return {};
      }
      // Both children
      auto right_fut = make_local_future(
        [&]() { return this->data_buffers_[2].has_data(version); },
        [&]() { return std::get<T>(this->data_buffers_[2].get(version)); }
      );
      // Do op(op(left, value), right) so order of evaluation is always the same
      // Also if there are no parents then this will have the final reduce value
      const auto reduce_value = reduce_op(reduce_op(left_fut.get(), value), right_fut.get());
      if (this->returns_value_on_reduce())
      {
        return reduce_value;
      }
      else
      {
        send_value_to_parent(reduce_value, version);
        return {};
      }
    }
  }; // class ReduceGroup
} // namespace skynet::internal

#endif // SKYNET_INTERNAL_REDUCE_GROUP_HPP
