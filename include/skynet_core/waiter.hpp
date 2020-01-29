#ifndef SKYNET_WAITER_HPP
#define SKYNET_WAITER_HPP

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <type_traits>

namespace skynet
{
  template<typename ProducedType, typename IsReadyCallable, typename GetValueCallable>
  class Waiter : public IsReadyCallable, public GetValueCallable
  {
  public:
    using ValueType = ProducedType;

    Waiter(
      std::mutex& mutex_handle,
      std::condition_variable& cv_handle,
      IsReadyCallable ready,
      GetValueCallable get_value
    ) noexcept
      : IsReadyCallable{std::move(ready)}
      , GetValueCallable{std::move(get_value)}
      , mutex_{mutex_handle}
      , cv_{cv_handle}
    {}

    ProducedType get() noexcept
    {
      std::unique_lock<std::mutex> lock{mutex_};
      if (!is_ready_no_lock())
      {
        cv_.wait(lock, [this]() { return is_ready_no_lock(); });
      }
      return GetValueCallable::operator()();
    }

    void wait() noexcept
    {
      std::unique_lock<std::mutex> lock{mutex_};
      if (is_ready_no_lock()) { return; }
      cv_.wait(lock, [this]() { return is_ready_no_lock(); });
    }

    template<class Rep, class Period>
    bool wait_for(const std::chrono::duration<Rep, Period>& wait_time) noexcept
    {
      std::unique_lock<std::mutex> lock{mutex_};
      if (is_ready_no_lock()) { return true; }
      return cv_.wait_for(lock, wait_time, [this]() { return is_ready_no_lock(); });
    }

    template<class Rep, class Period>
    bool wait_until(const std::chrono::duration<Rep, Period>& end_time) noexcept
    {
      std::unique_lock<std::mutex> lock{mutex_};
      if (is_ready_no_lock()) { return true; }
      return cv_.wait_until(lock, end_time, [this]() { return is_ready_no_lock(); });
    }

    bool is_ready() noexcept
    {
      std::lock_guard<std::mutex> lock{mutex_};
      return is_ready_no_lock();
    }

    // For transforming the get type
    template<typename AdjustCallable>
    auto adjust_get_function(AdjustCallable adjust) const noexcept
    {
      // Explicitly extract the return type so it won't remove references, etc.
      using ret_type = decltype(adjust(GetValueCallable::operator()()));
      const auto adj_lambda = [adjust = std::move(adjust), getter = static_cast<const GetValueCallable&>(*this)]() -> ret_type {
        return adjust(getter());
      };
      return Waiter<decltype(adj_lambda()), IsReadyCallable, decltype(adj_lambda)>{
        mutex_,
        cv_,
        static_cast<const IsReadyCallable&>(*this),
        adj_lambda
      };
    }

  private:
    bool is_ready_no_lock() noexcept
    {
      return IsReadyCallable::operator()();
    }

    std::mutex& mutex_;
    std::condition_variable& cv_;
  }; // class Waiter

  namespace internal
  {
    struct WaiterGetNoOp
    {
      constexpr void operator()() const noexcept {}
    }; // struct WaiterGetNoOp

    template<typename IsReadyCallable, typename GetValueCallable>
    auto make_waiter(
      std::mutex& mutex,
      std::condition_variable& cv,
      IsReadyCallable ready,
      GetValueCallable get_value
    ) noexcept
      -> Waiter<decltype(get_value()), IsReadyCallable, GetValueCallable>
    {
      return Waiter<decltype(get_value()), IsReadyCallable, GetValueCallable>{
        mutex,
        cv,
        std::move(ready),
        std::move(get_value)
      };
    }

    // Overload for void returning futures
    template<typename IsReadyCallable>
    auto make_waiter(
      std::mutex& mutex,
      std::condition_variable& cv,
      IsReadyCallable ready
    ) noexcept
      -> Waiter<void, IsReadyCallable, WaiterGetNoOp>
    {
      return make_waiter(mutex, cv, std::move(ready), WaiterGetNoOp{});
    }
  } // namespace skynet::internal
} // namespace skynet

#endif // SKYNET_WAITER_HPP
