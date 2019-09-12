#include "skynet/utility/worker_thread.hpp"

namespace skynet
{
  WorkerThread::~WorkerThread()
  {
    // Force the thread to stop waiting
    kill_thread_ = true;
    work_to_be_done_.notify_all();
    thread_handle_.join();
  }

  Future<void> WorkerThread::submit_work(task_type func) noexcept
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

  void WorkerThread::thread_func() noexcept
  {
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
  }
} // namespace skynet