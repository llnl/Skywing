#ifndef SKYNET_UTILITY_WORKERTHREAD_HPP
#define SKYNET_UTILITY_WORKERTHREAD_HPP

#include <functional>
#include <chrono>
#include <vector>

#include <thread>
#include <condition_variable>
#include <mutex>
#include <atomic>
#include <iterator>
#include <climits>
#include <tuple>
#include <type_traits>
#include <memory>

#include "future.hpp"
#include "optional.hpp"

namespace skynet
{
  /** \brief Class to use for a working thread for doing work with skynet::Future
   *
   */
  class WorkerThread
  {
  public:
    /// The type of tasks to submit; returns true on completion, false to be resubmitted
    using task_type = std::function<bool()>;

    WorkerThread()
    {
      // can't set up the thread handle until the object has finished constructing
      thread_handle_ = std::thread(&WorkerThread::thread_func, this);
    }

    // Non-copyable, non-movable
    WorkerThread(const WorkerThread&) = delete;
    WorkerThread(WorkerThread&&) = delete;
    WorkerThread& operator=(const WorkerThread&) = delete;
    WorkerThread& operator=(WorkerThread&&) = delete;

    /** \brief Cleanly exits the WorkerThread
     *
     * Although the thread cleanly exits, there may still be work in the queue that
     * has yet to finish.
     */
    ~WorkerThread();

    /** \brief Submits a function to be run on the thread
     *
     * \param func The function to run; should return false if it needs to be
     *             run again, and true if the work is complete
     */
    Future<void> submit_work(task_type func) noexcept;

    /** \brief Wraps a value returning callable to be used
     *
     * \param func The callable to run, should return a skynet::Optional
     */
    template<typename Callable>
    auto wrap_and_submit_work(Callable&& func) noexcept -> Future<typename decltype(func())::value_type>
    {
      using value_type = typename decltype(func())::value_type;
      // shared_ptr as std::function needs to be copyable, but std::promise is move-only
      auto promise = std::make_shared<std::promise<value_type>>();
      std::future<value_type> to_ret = promise->get_future();
      {
        std::unique_lock<std::mutex> lock(task_mut_);
        pending_tasks_.emplace_back([func = std::forward<Callable>(func), promise]() mutable {
          const auto value = func();
          if (value)
          {
            promise->set_value(*value);
            return true;
          }
          return false;
        });
      }
      work_to_be_done_.notify_all();
      return Future<value_type>(std::move(to_ret));
    }

  private:
    // The function run on the thread
    void thread_func() noexcept;

    // The tasks that are currently being worked on
    std::vector<task_type> current_tasks_;

    // Shared vector for future tasks to execute
    std::vector<task_type> pending_tasks_;

    // Mutex for protecting the pending tasks
    std::mutex task_mut_;

    // Thread handle for the worker thread
    std::thread thread_handle_;

    // For notifying that there is work to be done
    std::condition_variable work_to_be_done_;

    // For notifying the thread to finish processing
    std::atomic<bool> kill_thread_{false};
  }; // class WorkerThread
} // namespace skynet

#endif // SKYNET_UTILITY_WORKERTHREAD_HPP
