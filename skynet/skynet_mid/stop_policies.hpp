#ifndef STOPPING_CRITERION_HPP
#define STOPPING_CRITERION_HPP

#include "skynet_core/job.hpp"
#include "skynet_core/master.hpp"
#include <chrono>

/* This file contains a number of common iterative methods StopPolicy
   (stopping criteria) options.
 */

/** @brief StopPolicy that stops after a given amount of time has
    passed.
 */
class StopAfterTime
{
public:

  template<typename Duration>
  StopAfterTime(Duration d)
    : max_run_time_(d)
  {}

  template<typename CallerT>
  bool operator()(const CallerT& caller)
  {
    return caller.run_time() < max_run_time_;
  }

private:
  std::chrono::milliseconds max_run_time_;
  
}; // class StopAfterTime


#endif
