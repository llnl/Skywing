#ifndef STOPPING_CRITERION_HPP
#define STOPPING_CRITERION_HPP

#include "skynet_core/job.hpp"
#include "skynet_core/master.hpp"
#include <chrono>

// This is a recursive template which compares arguments in pairs used for comparing varying criterion for in arbitrary order to determine if the method should stop.
// This follow the logic that if all true -> return true and one false -> return false. 
// Note this probably does not work for overloaded operators for related data types, i.e., comparing chrono::duration_cast<chrono::milliseconds> and chrono::milliseconds does not work. 
template<typename T>
bool should_stop(T a, T b) {

  return a < b;
}

template<typename T, typename... Args>
bool should_stop(T a, T b, Args... args) 
{
  return a < b && should_stop(args...);
}

// For the use case where the method just needs to broadcast. Also for trouble shooting.
bool should_stop() {

  return true;
}


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
