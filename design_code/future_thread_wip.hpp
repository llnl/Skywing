#include <function>
#include <chrono>
#include <vector>

#include <thread>
#include <condition_variable>
#include <mutex>
#include <atomic>
#include <iterator>
#include <climits>

namespace skynet
{
  /** \brief Class to use for a working thread for doing work with skynet::Future
   *
   */
  class FutureThread
  {
  public:
    /// The type of tasks to submit; returns true on completion, false to be resubmitted
    using task_type = std::function<bool()>;

    // Non-copyable, but movable
    FutureThread(const FutureThread&) = delete;
    FutureThread(FutureThread&&) = default;
    FutureThread& operator=(const FutureThread&) = delete;
    FutureThread& operator=(FutureThread&&) = default;

    /** \brief Cleanly exits the FutureThread
     *
     * Although the thread cleanly exits, there may still be work in the queue that
     * has yet to finish.
     */
    ~FutureThread()
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
    void submit_work(task_type&& func)
    {
      {
        std::unique_lock<std::mutex> lock(task_mut_);
        pending_tasks.push_back(std::move(func));
      }
      work_to_be_done_.notify_all();
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
            ? std::chrono::milliseconds(LLONG_MAX)
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
    static constexpr std::chrono::milliseconds work_interval(1);
  }; // class FutureThread
} // namespace skynet
