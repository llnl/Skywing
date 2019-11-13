#ifndef SKYNET_LOCAL_FUTURE_HPP
#define SKYNET_LOCAL_FUTURE_HPP

#include <cassert>
#include <chrono>
#include <thread>

namespace skynet
{
  namespace internal
  {
    /** \brief A do-nothing callable, used for when there's only a ready function,
     * but no get function.
     */
    struct NoOpCallable
    {
      constexpr void operator()() const noexcept {}
    }; // struct NoOpCallable

    inline static constexpr std::chrono::microseconds default_poll_freq{10000};
  } // namespace skynet::internal

  /** \brief A class for "local" futures.  These futures live on the stack,
   * do nothing when destructed and are more wrappers for function pairs
   * where one function determines if a result is ready (and potentially
   * starts the work), and another that returns the result (if any).
   */
  template<typename ReadyCallable, typename GetCallable>
  class LocalFuture : public ReadyCallable, public GetCallable
  {
  public:
    LocalFuture() = delete;

    // Templated constructor so that forwarding can be used
    template<typename ConReadyCallable, typename ConGetCallable>
    LocalFuture(
      ConReadyCallable&& r,
      ConGetCallable&& g
    ) noexcept
      : ReadyCallable(std::forward<ConReadyCallable>(r))
      , GetCallable(std::forward<ConGetCallable>(g))
    {}

    /** \brief Wait until the value is ready
     */
    template<
      typename Rep = decltype(internal::default_poll_freq)::rep,
      typename Period = decltype(internal::default_poll_freq)::period
    >
    void wait(const std::chrono::duration<Rep, Period> poll_freq = internal::default_poll_freq) noexcept
    {
      while (!ReadyCallable::operator()())
      {
        std::this_thread::sleep_for(poll_freq);
      }
    }

    /** \brief Wait until the value is ready and then retrieve it
     */
    auto get() noexcept
    {
      wait();
      return GetCallable::operator()();
    }

    /** \brief Get the value from the function
     *
     * \pre The value is ready to be retrieved
     */
    auto unsafe_get() noexcept
    {
      assert(ReadyCallable::operator()());
      return GetCallable::operator()();
    }
  }; // class LocalFuture

  namespace internal
  {
    /** Create a local future with the given Callables
     */
    template<typename ReadyCallable, typename GetCallable>
    LocalFuture<ReadyCallable, GetCallable> make_local_future(ReadyCallable&& r, GetCallable&& g) noexcept
    {
      return LocalFuture<ReadyCallable, GetCallable>(
        std::forward<ReadyCallable>(r),
        std::forward<GetCallable>(g)
      );
    }

    /** Create a local future that has no get callable
     */
    template<typename ReadyCallable>
    LocalFuture<ReadyCallable, internal::NoOpCallable> make_local_future(ReadyCallable&& r) noexcept
    {
      return LocalFuture<ReadyCallable, internal::NoOpCallable>(
        std::forward<ReadyCallable>(r),
        internal::NoOpCallable{}
      );
    }
  } // namespace skynet::internal
} // namespace skynet

#endif // SKYNET_LOCAL_FUTURE_HPP
