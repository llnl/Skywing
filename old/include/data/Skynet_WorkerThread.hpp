#ifndef SKYNET_WORKERTHREAD_HPP__
#define SKYNET_WORKERTHREAD_HPP__

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

#include "Skynet_Future.hpp"

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
    ~WorkerThread()
    {
      // Force the thread to stop waiting
      kill_thread_ = true;
      work_to_be_done_.notify_all();
      thread_handle_.join();
    }

    /** \brief Submits a function to be run on the thread
     *
     * \param func The function to run; should return false if it needs to be
     *             run again, and true if the work is complete
     */
    Future<void> submit_work(task_type func)
    {
      // make a wrapper so that a future can be returned
      // std::function doesn't allow move-only types so do this dumb work around
      auto promise = std::make_shared<std::promise<void>>();
      std::future<void> to_ret = promise->get_future();
      {
        std::unique_lock<std::mutex> lock(task_mut_);
        pending_tasks_.emplace_back([func, promise]() mutable {
          if (func())
          {
            promise->set_value();
            return true;
          }
          return false;
        });
      }
      work_to_be_done_.notify_all();
      return Future<void>(std::move(to_ret));
    }

    /** \brief Wraps a value returning callable to be used
     *
     * \param func The callable to run, should return a pair-like object of
     *             a bool and a value where the bool signals if the value is ready
     */
    template<typename Callable>
    auto wrap_and_submit_work(Callable&& func) -> Future<std::tuple_element_t<0, decltype(func())>>
    {
      using value_type = std::tuple_element_t<0, decltype(func())>;
      // shared_ptr for same reason as above
      auto promise = std::make_shared<std::promise<value_type>>();
      std::future<value_type> to_ret = promise->get_future();
      {
        std::unique_lock<std::mutex> lock(task_mut_);
        pending_tasks_.emplace_back([func = std::forward<Callable>(func), promise]() mutable {
          const auto value_pair = func();
          if (std::get<0>(value_pair))
          {
            promise->set_value(std::get<1>(value_pair));
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
    void thread_func()
    {
      // The amount of time to wait between running through threads if work remains
      static constexpr std::chrono::microseconds work_interval(10);
      while (true)
      {
        // Wait for work to be available / wait a small bit if work is pending
        {
          std::unique_lock<std::mutex> lock(task_mut_);
          // wait "forever" if there isn't already work in the queue
          const auto time_to_wait = current_tasks_.empty()
            ? static_cast<decltype(work_interval)>(LLONG_MAX)
            : work_interval;
          work_to_be_done_.wait_for(lock, time_to_wait, [this]() {
            return !pending_tasks_.empty() || kill_thread_;
          });
          // Kill the thread if requested
          if (kill_thread_)
          {
            return;
          }
          // Take the new tasks and put them at the end of the tasks to perform
          std::move(
            pending_tasks_.begin(),
            pending_tasks_.end(),
            std::back_inserter(current_tasks_)
          );
          pending_tasks_.clear();
        }
        // Start processing the work now
        for (auto it = current_tasks_.begin(); it != current_tasks_.end(); ++it)
        {
          auto& task = *it;
          if (task())
          {
            // remove it by swapping it to the back and popping it
            using std::swap;
            swap(task, current_tasks_.back());
            current_tasks_.pop_back();
            // The iterator then needs to be adjusted as well
            --it;
          }
        }
      }
    }

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

#endif // SKYNET_WORKERTHREAD_HPP__
