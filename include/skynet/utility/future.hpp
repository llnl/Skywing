#ifndef SKYNET_UTILITY_FUTURE_HPP
#define SKYNET_UTILITY_FUTURE_HPP

#include <future>
#include <chrono>
#include <csignal>
#include <stdexcept>

namespace skynet
{
  /** Error handling strategy
   */
  enum class ErrorPlan
  {
    // Negative numbers are non-signal handlers
    throw_on_error = -1,
    terminate_on_error = -2
  };

  /** Signal to raise when an error occurs
   */
  ErrorPlan signal_on_error(int signal)
  {
    return static_cast<ErrorPlan>(signal);
  }

  /** Exception thrown if a future errors
   */
  class FutureError : public std::exception
  {
  public:
    FutureError(std::string message)
      : message_(std::move(message))
    {}

    const char* what() const noexcept override
    {
      return message_.c_str();
    }

  private:
    // Technically this is undefined behavior since an exception isn't allowed
    // to throw on copy, but that'll only happen on memory exhaustion
    std::string message_;
  };

  // TODO: Where does this go?  Don't want it in the Future class because it
  // doesn't depend on template parameters, but also don't need it to be
  // publicly exposed.
  // Appropriately handles an error based on the current strategy
  void future_handle_error(ErrorPlan plan, const std::exception& to_throw)
  {
    switch (plan)
    {
    case ErrorPlan::throw_on_error: throw to_throw;
    case ErrorPlan::terminate_on_error: std::terminate();
    }
    // Otherwise raise the coresponding signal
    std::raise(static_cast<int>(plan));
  }

  /** Asynchronous value/task handle
   */
  template<typename T>
  class Future
  {
  public:
    /** Produces a Future from an existing std::future
     */
    explicit Future(std::future<T> future)
      : value_(std::move(future))
    {}

    /** Wait until the future is ready
     */
    void wait(ErrorPlan plan = ErrorPlan::terminate_on_error) const
    {
      error_if_invalid(plan);
      value_.wait();
    }

    /** Wait until the future is ready or the time expires
     *
     * \returns True if the value is ready
     */
    template<typename Rep, typename Period>
    bool wait_for(
      const std::chrono::duration<Rep, Period>& timeout_duration,
      ErrorPlan plan = ErrorPlan::terminate_on_error
    ) const
    {
      error_if_invalid(plan);
      return value_.wait_for(timeout_duration) == std::future_status::ready;
    }

    /** Wait until the future is ready or until the specified time
     *
     * \return True if the value is ready
     */
    template<typename Rep, typename Period>
    bool wait_until(
      const std::chrono::time_point<Rep, Period>& timeout_time,
      ErrorPlan plan = ErrorPlan::terminate_on_error
    ) const
    {
      error_if_invalid(plan);
      return value_.wait_until(timeout_time) == std::future_status::ready;
    }

    /** Determine if the value is ready
     *
     * \return True if the value is ready, false otherwise
     */
    bool poll(ErrorPlan plan = ErrorPlan::terminate_on_error) const
    {
      error_if_invalid(plan);
      return value_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }

    /** Returns the value if ready, errors otherwise
     */
    T get(ErrorPlan plan = ErrorPlan::terminate_on_error)
    {
      error_if_invalid(plan);
      if (!poll(plan))
      {
        future_handle_error(plan, FutureError("skynet::Future::get called when value was not ready."));
      }
      return value_.get();
    }

  private:
    // Error if the future is not valid
    void error_if_invalid(ErrorPlan plan) const
    {
      if (!value_.valid())
      {
        future_handle_error(plan, FutureError("Future is not valid"));
      }
    }

    // Future handle
    std::future<T> value_;
  }; // class Future
} // namespace skynet

#endif // SKYNET_UTILITY_FUTURE_HPP
