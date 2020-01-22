#ifndef SKYNET_INTERNAL_FUTURE_HPP
#define SKYNET_INTERNAL_FUTURE_HPP

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <type_traits>

namespace skynet
{
  template<typename ProducedType, typename IsReadyCallable, typename GetValueCallable>
  class Future : public IsReadyCallable, public GetValueCallable
  {
  public:
    Future(
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
      const auto adj_lambda = [adjust = std::move(adjust), getter = static_cast<const GetValueCallable&>(*this)]() {
        return adjust(getter());
      };
      return Future<decltype(adj_lambda()), IsReadyCallable, decltype(adj_lambda)>{
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
  }; // class Future

  namespace internal
  {
    struct FutureGetNoOp
    {
      constexpr void operator()() const noexcept {}
    }; // struct FutureGetNoOp

    // This would be in internal even if Future isn't, however
    template<typename IsReadyCallable, typename GetValueCallable>
    auto make_future(
      std::mutex& mutex,
      std::condition_variable& cv,
      IsReadyCallable ready,
      GetValueCallable get_value
    ) noexcept
      -> Future<decltype(get_value()), IsReadyCallable, GetValueCallable>
    {
      return Future<decltype(get_value()), IsReadyCallable, GetValueCallable>{
        mutex,
        cv,
        std::move(ready),
        std::move(get_value)
      };
    }

    // Overload for void returning futures
    template<typename IsReadyCallable>
    auto make_future(
      std::mutex& mutex,
      std::condition_variable& cv,
      IsReadyCallable ready
    ) noexcept
      -> Future<void, IsReadyCallable, FutureGetNoOp>
    {
      return make_future(mutex, cv, std::move(ready), FutureGetNoOp{});
    }
  } // namespace skynet::internal
} // namespace skynet

#endif // SKYNET_INTERNAL_FUTURE_HPP
