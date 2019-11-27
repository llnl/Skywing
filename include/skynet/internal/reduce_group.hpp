#ifndef SKYNET_INTERNAL_REDUCE_GROUP_HPP
#define SKYNET_INTERNAL_REDUCE_GROUP_HPP

#include "skynet/internal/tag_buffer.hpp"
#include "skynet/internal/future.hpp"
#include "skynet/types.hpp"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>
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

      const TagID& produced_tag() const noexcept;

    private:
      // Sends a value to the parent
      void send_value_to_parent(const PublishValueVariant& value_to_send, VersionID version) noexcept;

      using DataBuffer = FifoTagBuffer<PublishValueVariant>;
      std::array<DataBuffer, 3> data_buffers_;
      ReduceGroupNeighbors tag_neighbors_;
      Master* master_;
      TagID group_id_;
      TagID produced_tag_;
      VersionID last_sent_version_ = tag_default_version;
      std::mutex buffer_mutex_;
      std::condition_variable data_added_to_buffers_cv_;
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

    template<typename Callable>
    auto reduce(
      const T& value,
      Callable reduce_op,
      VersionID version = internal::tag_default_version
    ) noexcept
    {
      return internal::make_future(
        base_.buffer_mutex_,
        base_.data_added_to_buffers_cv_,
        [this, version]() { return reduce_is_ready(version); },
        [this, value, version, reduce_op=std::move(reduce_op)]() {
          return do_reduce(value, std::move(reduce_op), version);
        }
      );
    }

    bool returns_value_on_reduce() const noexcept
    {
      return base_.returns_value_on_reduce();
    }

  private:
    bool reduce_is_ready(const VersionID version) noexcept
    {
      const auto required_version = internal::updated_version(base_.last_sent_version_, version);
      if (base_.tag_neighbors_.left_child().empty())
      {
        return true;
      }
      else if (base_.tag_neighbors_.right_child().empty())
      {
        // Left child only
        return base_.data_buffers_[1].has_data(required_version);
      }
      else
      {
        // Both children
        return
          base_.data_buffers_[1].has_data(required_version) &&
          base_.data_buffers_[2].has_data(required_version);
      }
    }

    template<typename ProdType, typename ReduceCallable>
    std::optional<ProdType> do_reduce(const ProdType& value, ReduceCallable reduce_op, const VersionID version) noexcept
    {
      const auto required_version = internal::updated_version(base_.last_sent_version_, version);
      base_.last_sent_version_ = required_version;
      // The base type is an optional, strip that type while working in here
      const ProdType reduce_result = [&]() -> ProdType {
        // Three different options - 2 children, left child only, no children
        if (base_.tag_neighbors_.left_child().empty())
        {
          // no children, just propagate value to parent
          return value;
        }
        // Either one or two children, left child is always present
        const auto left_val = std::get<ProdType>(base_.data_buffers_[1].get(required_version));
        if (base_.tag_neighbors_.right_child().empty())
        {
          // One child, just apply op with value and propagate value to parent
          const auto reduce_value = reduce_op(left_val, value);
          return reduce_value;
        }
        // Both children
        const auto right_val = std::get<ProdType>(base_.data_buffers_[2].get(required_version));
        // Do op(op(left, value), right) so order of evaluation is always the same
        // Also if there are no parents then this will have the final reduce value
        const auto reduce_value = reduce_op(reduce_op(left_val, value), right_val);
        return reduce_value;
      }();
      base_.send_value_to_parent(reduce_result, required_version);
      // Return the result if applicable
      if (base_.returns_value_on_reduce())
      {
        // TODO: Send result to children if it's an all reduce operation
        return reduce_result;
      }
      else
      {
        return {};
      }
    }

    internal::ReduceGroupBase& base_;
  }; // class ReduceGroup
} // namespace skynet

#endif // SKYNET_INTERNAL_REDUCE_GROUP_HPP
