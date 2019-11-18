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

namespace skynet
{
  template<typename T>
  class ReduceGroup;

  namespace internal
  {
    // TODO: Better control exposure of these things and such, since client code currently
    // sees this object
    // Can probably just have a function that takes one of these to do the reduce rather than
    // having the reduce object returned to the client.
    class ReduceGroupBase
    {
    public:
      explicit ReduceGroupBase(
        const ReduceGroupNeighbors& tag_neighbors,
        Master& master,
        const TagID& group_id,
        const TagID& produced_tag,
        const std::uint8_t expected_type
      ) noexcept;

      // Adds data to the corresponding buffer, returning false if an error occurred
      bool add_data(const TagID& tag, PublishValueVariant value, const VersionID version) noexcept;

      // Returns true if this handle to the group returns a value on reduce
      bool returns_value_on_reduce() const noexcept;

      const ReduceGroupNeighbors& tag_neighbors() const noexcept;

      // Sends a value to the parent
      void send_value_to_parent(const PublishValueVariant& value_to_send, VersionID version) noexcept;

    private:
      using DataBuffer = FifoTagBuffer<PublishValueVariant>;
      std::array<DataBuffer, 3> data_buffers_;
      ReduceGroupNeighbors tag_neighbors_;
      Master* master_;
      TagID group_id_;
      TagID produced_tag_;
      VersionID last_sent_version_ = tag_default_version;
      std::uint8_t expected_type_;

      template<typename T>
      friend class ::skynet::ReduceGroup;
    }; // class ReduceGroupBase
  } // namespace skynet::internal

  template<typename T>
  class ReduceGroup
  {
  public:
    ReduceGroup(internal::ReduceGroupBase& base) noexcept
      : base_{base}
    {}

    // TODO: This is currently blocking, going to end up changing the interfaces and such later,
    // so fix this when that's done
    template<typename Callable>
    std::optional<T> reduce(
      const T& value,
      Callable reduce_op,
      VersionID version = internal::tag_default_version
    ) noexcept
    {
      base_.last_sent_version_ = internal::detail::updated_version(base_.last_sent_version_, version);
      const T reduce_result = [&]() {
        // Three different options - 2 children, left child only, no children
        if (base_.tag_neighbors_.left_child().empty())
        {
          // no children, just propagate value to parent
          base_.send_value_to_parent(value, base_.last_sent_version_);
          return value;
        }
        // Either one or two children, left child is always present
        auto left_fut = internal::make_local_future(
          [&]() { return base_.data_buffers_[1].has_data(base_.last_sent_version_); },
          [&]() { return std::get<T>(base_.data_buffers_[1].get(base_.last_sent_version_)); }
        );
        if (base_.tag_neighbors_.right_child().empty())
        {
          // One child, just apply op with value and propagate value to parent
          const auto reduce_value = reduce_op(left_fut.get(), value);
          base_.send_value_to_parent(reduce_value, base_.last_sent_version_);
          return reduce_value;
        }
        // Both children
        auto right_fut = internal::make_local_future(
          [&]() { return base_.data_buffers_[2].has_data(base_.last_sent_version_); },
          [&]() { return std::get<T>(base_.data_buffers_[2].get(base_.last_sent_version_)); }
        );
        // Do op(op(left, value), right) so order of evaluation is always the same
        // Also if there are no parents then this will have the final reduce value
        const auto reduce_value = reduce_op(reduce_op(left_fut.get(), value), right_fut.get());
        base_.send_value_to_parent(reduce_value, base_.last_sent_version_);
        return reduce_value;
      }();
      // Return the result if applicable
      if (base_.returns_value_on_reduce())
      {
        return reduce_result;
      }
      else
      {
        return {};
      }
    }

    bool returns_value_on_reduce() const noexcept
    {
      return base_.returns_value_on_reduce();
    }

  private:
    internal::ReduceGroupBase& base_;
  }; // class ReduceGroup
} // namespace skynet

#endif // SKYNET_INTERNAL_REDUCE_GROUP_HPP
