#ifndef SKYNET_WORKERTHREAD_HPP__
#define SKYNET_WORKERTHREAD_HPP__

#include <function>
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

    // Non-copyable, but movable
    WorkerThread(const WorkerThread&) = delete;
    WorkerThread(WorkerThread&&) = default;
    WorkerThread& operator=(const WorkerThread&) = delete;
    WorkerThread& operator=(WorkerThread&&) = default;

    /** \brief Cleanly exits the WorkerThread
     *
     * Although the thread cleanly exits, there may still be work in the queue that
     * has yet to finish.
     */
    ~WorkerThread()
    {
      kill_thread_ = true;
      // Will force the thread to stop waiting
      work_to_be_done_.notify_all();
      thread_handle_.join();
    }

    /** \brief Submits a function to be run on the thread
     *
     * \param func The function to run; should return false if it needs to be
     *             run again, and true if the work is complete
     */
    Future<void> submit_work(task_type&& func)
    {
      // make a wrapper so that a future can be returned
      std::promise<void> promise;
      std::future<void> to_ret = promise.get_future();
      auto wrapper = [func = std::move(func), promise = std::move(promise)]() mutable {
        if (func())
        {
          promise.set_value();
          return true;
        }
        return false;
      };
      {
        std::unique_lock<std::mutex> lock(task_mut_);
        pending_tasks.push_back(std::move(wrapper));
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
    auto wrap_and_submit_work(Callable&& func) -> Future<std::tuple_element_t<0, std::result_of_t<func()>>>
    {
      using value_type = std::tuple_element_t<0, std::result_of_t<func()>>;
      std::promise<value_type> promise;
      std::future<value_type> to_ret = promise.get_future();
      auto wrapper = [func = std::forward<Callable>(func), promise = std::move(promise)]() mutable {
        const auto value_pair = func();
        if (std::get<0>(value_pair))
        {
          promise.set_value(std::get<1>(value_pair));
          return true;
        }
        return false;
      };
      {
        std::unique_lock<std::mutex> lock(task_mut_);
        pending_tasks.push_back(std::move(wrapper));
      }
      work_to_be_done_.notify_all();
      return Future<value_type>(std::move(to_ret));
    }

  private:
    // The function run on the thread
    static void thread_func()
    {
      while (true)
      {
        // Wait for work to be available / wait a small bit if work is pending
        {
          std::unique_lock<std::mutex> lock(task_mut_);
          // wait "forever" if there isn't already work in the queue
          const auto time_to_wait = current_tasks_.empty()
            ? decltype(work_interval)(LLONG_MAX)
            : work_interval;
          work_to_be_done_.wait_for(lock, [this]() {
            return !pending_tasks.empty();
          });
          // Kill the thread if requested
          if (kill_thread_)
          {
            return;
          }
          // Take the new tasks and put them at the end of the tasks to perform
          std::move(
            pending_tasks.begin(),
            pending_tasks.end(),
            std::back_inserter(current_tasks_)
          );
          pending_tasks.clear();
        }
        // Start processing the work now
        std::vector<current_tasks_::iterator> to_remove;
        // Run each function and if it's done mark it for removal
        for (auto it = current_tasks_.begin(); it != current_tasks_.end(); ++it)
        {
          auto& task = *it;
          if (task())
          {
            to_remove.push_back(it);
          }
        }
        // Remove the finished tasks
        for (const auto& to_ret : to_remove)
        {
          using std::swap;
          swap(*to_ret, current_tasks.back());
          current_tasks.pop_back();
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

    // The amount of time to wait between running through threads if work remains
    static constexpr std::chrono::microseconds work_interval(10);
  }; // class WorkerThread
} // namespace skynet

#endif // SKYNET_WORKERTHREAD_HPP__
