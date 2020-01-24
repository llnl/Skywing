#ifndef SKYNET_INTERNAL_REDUCE_GROUP_HPP
#define SKYNET_INTERNAL_REDUCE_GROUP_HPP

#include "skynet_core/internal/master_waiter_callables.hpp"
#include "skynet_core/internal/tag_buffer.hpp"
#include "skynet_core/waiter.hpp"
#include "skynet_core/types.hpp"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <random>
#include <type_traits>
#include <unordered_map>

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
    // TODO: The public things in here shouldn't be public but accessable
    // only by the master (and ReduceGroup)
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
      bool add_data(const TagID& tag, PublishValueVariant value, VersionID version) noexcept;

      // Handle and propagate a disconnection notice from another machine
      void propagate_disconnection(const MachineID& initiating_machine, ReductionDisconnectID id) noexcept;

      // Create a disconnection notice and send it to connected machines
      void report_disconnection() noexcept;

      // Returns true if this handle to the group returns a value on reduce
      bool returns_value_on_reduce() const noexcept;

      const ReduceGroupNeighbors& tag_neighbors() const noexcept;

      const TagID& produced_tag() const noexcept;

      // Rebuilds a reduce group after it fails due to a disconnection
      auto rebuild() noexcept
        -> Waiter<void, internal::MasterReduceGroupIsCreated, internal::FutureGetNoOp>;

    private:
      // Sends a value to the parent
      void send_value_to_parent(const PublishValueVariant& value_to_send, VersionID version) noexcept;

      // Sends a value to the children
      void send_value_to_children(const PublishValueVariant& value_to_send, VersionID version) noexcept;

      // Sends a disconnection notice to all parents and children
      void send_disconnection(const MachineID& initiating_machine, ReductionDisconnectID disconn_id) noexcept;

      // Adds data without locking and using an index
      void add_data_index(std::size_t index, PublishValueVariant value, VersionID version) noexcept;

      // Process any pending reduce operations, removing them if finished
      void process_pending_reduce_ops() noexcept;

      using DataBuffer = FifoTagBuffer<PublishValueVariant>;
      struct PendingReduce
      {
        VersionID required_version;
        PublishValueVariant value;
        std::function<PublishValueVariant(PublishValueVariant, PublishValueVariant)> operation;
        bool is_all_reduce;
      };
      std::unordered_map<MachineID, ReductionDisconnectID> last_heard_disconnect;
      std::vector<PendingReduce> pending_reduces_;
      std::array<DataBuffer, 3> data_buffers_;
      ReduceGroupNeighbors tag_neighbors_;
      Master* master_;
      TagID group_id_;
      TagID produced_tag_;
      VersionID last_sent_version_ = tag_no_data;
      std::mutex buffer_mutex_;
      std::condition_variable future_info_cv_;
      // PRNG for disconnection ID's
      // This doesn't need to be a very good PRNG since it'll seldom be used and
      // it's only to avoid extremely rare collisions
      // Use minstd_rand just for its small size (8 bytes)
      // This also has the nice property where the next number will always be
      // different from the next number, so the previous number doesn't have
      // to be stored
      std::minstd_rand prng{std::random_device{}()};

      std::uint8_t expected_type_;
      bool is_valid = true;
      // Internal counter so that earlier-made futures know to error
      std::uint16_t conn_counter = 0;

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
      Callable reduce_op
    ) noexcept
    {
      return reduce_impl<false>(value, std::move(reduce_op));
    }

    template<typename Callable>
    auto allreduce(
      const T& value,
      Callable reduce_op
    ) noexcept
    {
      return reduce_impl<true>(value, std::move(reduce_op));
    }

    bool returns_value_on_reduce() const noexcept
    {
      return base_.returns_value_on_reduce();
    }

    auto rebuild() noexcept
    {
      return base_.rebuild();
    }

  private:
    // Templated because the return type will be different if it's an allreduce
    template<bool IsAllReduce, typename Callable>
    auto reduce_impl(
      const T& value,
      Callable reduce_op
    ) noexcept
    {
      std::lock_guard lock{base_.buffer_mutex_};
      const auto required_version = base_.last_sent_version_ + 1;
      base_.pending_reduces_.push_back({
        required_version,
        value,
        [reduce_op](PublishValueVariant lhs, PublishValueVariant rhs) -> PublishValueVariant {
          assert(std::get_if<T>(&lhs));
          assert(std::get_if<T>(&rhs));
          return reduce_op(*std::get_if<T>(&lhs), *std::get_if<T>(&rhs));
        },
        IsAllReduce
      });
      base_.process_pending_reduce_ops();
      const auto conn_id = base_.conn_counter;
      using produced_type = std::conditional_t<IsAllReduce, std::optional<T>, ReduceResult<T>>;
      // As the produced type is different,
      return internal::make_waiter(
        base_.buffer_mutex_,
        base_.future_info_cv_,
        [this, required_version, conn_id]() noexcept {
          if (conn_id < base_.conn_counter || !base_.is_valid)
          {
            return true;
          }
          if constexpr (IsAllReduce)
          {
            return base_.data_buffers_[0].has_data(required_version);
          }
          else
          {
            return
              base_.last_sent_version_ != internal::tag_no_data &&
              base_.last_sent_version_ >= required_version;
          }
        },
        [this, required_version, conn_id]() noexcept -> produced_type {
          const bool error_occurred = (conn_id < base_.conn_counter || !base_.is_valid);
          const auto make_error = []() {
            if constexpr (IsAllReduce)
            {
              return produced_type{};
            }
            else
            {
              return ReduceDisconnection{};
            }
          };
          if (IsAllReduce || returns_value_on_reduce())
          {
            // If there's a value return it regardless of if there's an error
            if (base_.data_buffers_[0].has_data(required_version))
            {
              // Value is present
              const auto value = base_.data_buffers_[0].get(required_version);
              assert(std::get_if<T>(&value));
              return *std::get_if<T>(&value);
            }
            else
            {
              return make_error();
            }
          }
          else if (error_occurred)
          {
            return make_error();
          }
          else if constexpr(!IsAllReduce)
          {
            // Normal reduce - no error occurred, but no value to return
            return ReduceNoValue{};
          }
        }
      );
    }

    internal::ReduceGroupBase& base_;
  }; // class ReduceGroup
} // namespace skynet

#endif // SKYNET_INTERNAL_REDUCE_GROUP_HPP
