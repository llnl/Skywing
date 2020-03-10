#ifndef SKYNET_WAITER_HPP
#define SKYNET_WAITER_HPP

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <type_traits>

#include "skynet_core/types.hpp"

namespace skynet
{
  template<typename IsReadyCallable, typename GetValueCallable, typename... Continuations>
  class Waiter : public IsReadyCallable, public GetValueCallable, public Continuations...
  {
  public:
    /// The return type of get()
    using ValueType = decltype(std::declval<GetValueCallable>()());

    Waiter(
      std::mutex& mutex_handle,
      std::condition_variable& cv_handle,
      IsReadyCallable ready,
      GetValueCallable get_value,
      Continuations... continuations
    ) noexcept
      : IsReadyCallable{std::move(ready)}
      , GetValueCallable{std::move(get_value)}
      , Continuations{std::move(continuations)}...
      , mutex_{mutex_handle}
      , cv_{cv_handle}
    {}

    decltype(auto) get() noexcept
    {
      // Have the value be returned from a lambda so that no temporaries are made
      // but the lock will be released before calling the continuations
      const auto get_value = [&]() noexcept -> decltype(auto) {
        std::unique_lock<std::mutex> lock{mutex_};
        if (!is_ready_no_lock())
        {
          cv_.wait(lock, [this]() noexcept { return is_ready_no_lock(); });
        }
        return GetValueCallable::operator()();
      };
      if constexpr (sizeof...(Continuations) == 0)
      {
        return get_value();
      }
      else
      {
        return handle_continuations(get_value, static_cast<Continuations&>(*this)...);
      }
    }

    void wait() noexcept
    {
      std::unique_lock<std::mutex> lock{mutex_};
      if (is_ready_no_lock()) { return; }
      cv_.wait(lock, [this]() noexcept { return is_ready_no_lock(); });
    }

    template<class Rep, class Period>
    bool wait_for(const std::chrono::duration<Rep, Period>& wait_time) noexcept
    {
      std::unique_lock<std::mutex> lock{mutex_};
      if (is_ready_no_lock()) { return true; }
      return cv_.wait_for(lock, wait_time, [this]() noexcept { return is_ready_no_lock(); });
    }

    template<class Rep, class Period>
    bool wait_until(const std::chrono::time_point<Rep, Period>& end_time) noexcept
    {
      std::unique_lock<std::mutex> lock{mutex_};
      if (is_ready_no_lock()) { return true; }
      return cv_.wait_until(lock, end_time, [this]() noexcept { return is_ready_no_lock(); });
    }

    bool is_ready() noexcept
    {
      std::lock_guard<std::mutex> lock{mutex_};
      return is_ready_no_lock();
    }

    // For transforming the get type
    template<typename... NewContinuations>
    auto then(NewContinuations... continuations) && noexcept
    {
      using ret_type = Waiter<
        IsReadyCallable,
        GetValueCallable,
        Continuations...,
        NewContinuations...
      >;
      return ret_type{
        mutex_,
        cv_,
        std::move(static_cast<IsReadyCallable&>(*this)),
        std::move(static_cast<GetValueCallable&>(*this)),
        std::move(static_cast<Continuations&>(*this))...,
        std::move(continuations...)
      };
    }

  private:
    bool is_ready_no_lock() noexcept
    {
      return IsReadyCallable::operator()();
    }

    // The continuations are in the reverse order that they need to be called
    // so have to build up a function to return the result
    template<typename BuildUp, typename Next, typename... Rest>
    decltype(auto) handle_continuations(
      const BuildUp& build_up,
      Next& next,
      Rest&... rest
    ) noexcept
    {
      const auto next_call = [&]() noexcept -> decltype(auto) {
        // Can't pass void so have to make a wrapper
        if constexpr (std::is_same_v<decltype(build_up()), void>)
        {
          return [&]() noexcept -> decltype(auto) {
            build_up();
            return next();
          };
        }
        else
        {
          return [&]() noexcept -> decltype(auto) {
            return next(build_up());
          };
        }
      };
      if constexpr (sizeof...(Rest) == 0)
      {
        return next_call()();
      }
      else
      {
        return handle_continuations(next_call(), rest...);
      }
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

    template<typename IsReadyCallable, typename GetValueCallable, typename... Continuations>
    auto make_waiter(
      std::mutex& mutex,
      std::condition_variable& cv,
      IsReadyCallable ready,
      GetValueCallable get_value,
      Continuations... continuations
    ) noexcept
      -> Waiter<IsReadyCallable, GetValueCallable, Continuations...>
    {
      return Waiter<IsReadyCallable, GetValueCallable, Continuations...>{
        mutex,
        cv,
        std::move(ready),
        std::move(get_value),
        std::move(continuations)...
      };
    }

    // Overload for void returning futures
    template<typename IsReadyCallable>
    auto make_waiter(
      std::mutex& mutex,
      std::condition_variable& cv,
      IsReadyCallable ready
    ) noexcept
      -> Waiter<IsReadyCallable, WaiterGetNoOp>
    {
      return make_waiter(mutex, cv, std::move(ready), WaiterGetNoOp{});
    }
  } // namespace skynet::internal

  /** \brief Class used to wait on multiple waiters.  Generally created using
   * when_all instead of directly
   */
  template<typename... Waiters>
  class AllWaiter
  {
  public:
    explicit AllWaiter(Waiters&... waiters) noexcept
      : to_wait_on_{waiters...}
    {}

    auto get() noexcept
      -> std::tuple<internal::WrapVoidValue<typename Waiters::ValueType>...>
    {
      return for_each_waiter(
        [](auto& waiter) noexcept -> typename decltype(waiter)::ValueType {
          return waiter.get();
        }
      );
    }

    void wait() noexcept
    {
      for_each_waiter([](auto& waiter) noexcept { waiter.wait(); });
    }

    template<class Rep, class Period>
    bool wait_until(const std::chrono::time_point<Rep, Period>& end_time) noexcept
    {
      bool value_available = true;
      for_each_waiter([&](auto& waiter) noexcept {
        value_available = value_available && waiter.wait_unti(end_time);
      });
      return value_available;
    }

    template<class Rep, class Period>
    bool wait_for(const std::chrono::duration<Rep, Period>& wait_time) noexcept
    {
      const auto end_time = std::chrono::steady_clock::now() + wait_time;
      return wait_until(end_time);
    }

  private:
    template<typename Callable, std::size_t... Is>
    auto for_each_waiter_impl(Callable&& c, std::index_sequence<Is...>) noexcept
      -> std::tuple<internal::WrapVoidValue<typename Waiters::ValueType>...>
    {
      return {c(std::get<Is>(to_wait_on_))...};
    }

    template<typename Callable>
    auto for_each_waiter(Callable&& c) noexcept
      -> std::tuple<internal::WrapVoidValue<typename Waiters::ValueType>...>
    {
      return for_each_waiter_impl(std::forward<Callable>(c), std::index_sequence_for<Waiters...>{});
    }

    std::tuple<Waiters...> to_wait_on_;
  }; // class AllWaiter

  /** \brief Returns an AllWaiter that can be used to wait for when multiple
   * waiters complete and return a tuple of the values, with void values represented
   * as VoidWrapper instead
   */
  template<typename... Waiters>
  AllWaiter<Waiters...> when_all(Waiters&... waiters) noexcept
  {
    return AllWaiter{waiters...};
  }
} // namespace skynet

#endif // SKYNET_WAITER_HPP
