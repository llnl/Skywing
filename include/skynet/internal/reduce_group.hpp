#ifndef SKYNET_INTERNAL_REDUCE_GROUP_HPP
#define SKYNET_INTERNAL_REDUCE_GROUP_HPP

#include "skynet/internal/tag_buffer.hpp"
#include "skynet/local_future.hpp"
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

  template<typename T, typename Callable>
  class SendReduceFuture;

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
      std::condition_variable data_added_to_buffers_;
      std::uint8_t expected_type_;

      template<typename T>
      friend class ::skynet::ReduceGroup;

      template<typename T, typename Callable>
      friend class ::skynet::SendReduceFuture;
    }; // class ReduceGroupBase
  } // namespace skynet::internal

  // TODO: Could make this use type-erasure (std::function) instead of a template
  //       parameter for the callable, which would allow storing it in a container and such
  //       at the cost of some efficiency.  Not sure which approach is better.
  //       (Could also add an implicit conversion to a different type of future that uses
  //       type erasure or something.)
  template<typename T, typename Callable>
  class SendReduceFuture : public Callable
  {
  public:
    T get() noexcept
    {
      std::unique_lock<std::mutex> lock{base_.buffer_mutex_};
      if (!send_is_ready_no_lock())
      {
        base_.data_added_to_buffers_.wait(lock, [this]() { return send_is_ready_no_lock(); });
      }
      return do_reduce();
    }

    template<class Rep, class Period>
    std::optional<T> wait_for_then_get_if_ready(const std::chrono::duration<Rep, Period>& wait_time) noexcept
    {
      std::unique_lock<std::mutex> lock{base_.buffer_mutex_};
      if (send_is_ready_no_lock())
      {
        return do_reduce();
      }
      else if (base_.data_added_to_buffers_.wait_for(lock, wait_time, [this]() { return send_is_ready_no_lock(); }))
      {
        return do_reduce();
      }
      else
      {
        return {};
      }
    }

    template<class Rep, class Period>
    std::optional<T> wait_until_then_get_if_ready(const std::chrono::duration<Rep, Period>& end_time) noexcept
    {
      std::unique_lock<std::mutex> lock{base_.buffer_mutex_};
      if (send_is_ready_no_lock())
      {
        return do_reduce();
      }
      else if (base_.data_added_to_buffers_.wait_until(lock, end_time, [this]() { return send_is_ready_no_lock(); }))
      {
        return do_reduce();
      }
      else
      {
        return {};
      }
    }

    bool send_is_ready() noexcept
    {
      std::unique_lock<std::mutex> lock{base_.buffer_mutex_};
      return send_is_ready_no_lock();
    }

  private:
    template<typename U>
    friend class ReduceGroup;

    // T is always an optional type, so extract out the base type
    using ValueType = typename T::value_type;

    SendReduceFuture(
      Callable c,
      internal::ReduceGroupBase& base,
      VersionID required_version,
      ValueType value,
      bool is_all_reduce
    ) noexcept
      : Callable(c)
      , base_{base}
      , required_version_{required_version}
      , value_{value}
      , is_all_reduce_{is_all_reduce}
    {}

    bool send_is_ready_no_lock() noexcept
    {
      // Either no children, left child only, or both children
      // If left child is empty there are no children; always ready
      if (base_.tag_neighbors_.left_child().empty())
      {
        return true;
      }
      else if (base_.tag_neighbors_.right_child().empty())
      {
        // Left child only
        return base_.data_buffers_[1].has_data(required_version_);
      }
      else
      {
        // Both children
        return
          base_.data_buffers_[1].has_data(required_version_) &&
          base_.data_buffers_[2].has_data(required_version_);
      }
    }

    T do_reduce() noexcept
    {
      // The base type is an optional, strip that type while working in here
      const ValueType reduce_result = [&]() {
        // Three different options - 2 children, left child only, no children
        if (base_.tag_neighbors_.left_child().empty())
        {
          // no children, just propagate value to parent
          return value_;
        }
        // Either one or two children, left child is always present
        const auto left_val = std::get<ValueType>(base_.data_buffers_[1].get(base_.last_sent_version_));
        if (base_.tag_neighbors_.right_child().empty())
        {
          // One child, just apply op with value and propagate value to parent
          const auto reduce_value = Callable::operator()(left_val, value_);
          return reduce_value;
        }
        // Both children
        const auto right_val = std::get<ValueType>(base_.data_buffers_[2].get(base_.last_sent_version_));
        // Do op(op(left, value), right) so order of evaluation is always the same
        // Also if there are no parents then this will have the final reduce value
        const auto reduce_value = Callable::operator()(
          Callable::operator()(left_val, value_),
          right_val
        );
        return reduce_value;
      }();
      base_.send_value_to_parent(reduce_result, base_.last_sent_version_);
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
    VersionID required_version_;
    ValueType value_;
    bool is_all_reduce_;
  };

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
      return SendReduceFuture<std::optional<T>, Callable>{std::move(reduce_op), base_, version, value, false};
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
