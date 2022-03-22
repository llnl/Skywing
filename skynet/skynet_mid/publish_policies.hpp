#ifndef UPDATE_NBRS_CRITERION_HPP
#define UPDATE_NBRS_CRITERION_HPP

#include "skynet_core/job.hpp"
#include "skynet_core/master.hpp"
#include <cmath>
#include <iostream>

/** @brief A PublishPolicy that always publishes.
 */ 
class AlwaysPublish
{
public:
  
  AlwaysPublish()
  {}

  template<typename ValueType>
  bool operator()([[maybe_unused]] const ValueType& new_vals,
                  [[maybe_unused]] const ValueType& old_vals)
  { return true; }
}; // class AlwaysPublish




/** @brief a PublishPolicy that publishes if the Linf norm of the
 *  value differences is at least a threshold.
 */
template<typename scalar_t = double>
class PublishOnLinfShift
{
public:

  PublishOnLinfShift(scalar_t thresh)
    : linf_threshold_(thresh)
  {}

  template<typename ValueType>
  bool operator()(const ValueType& new_vals, const ValueType& old_vals)
  {
    auto new_v_iter = new_vals.cbegin();
    auto old_v_iter = old_vals.cbegin();
    while (new_v_iter != new_vals.cend())
    {
      scalar_t curr_diff = std::abs(*new_v_iter - *old_v_iter);
      if (curr_diff > linf_threshold_)
        return true;
      new_v_iter++;
      old_v_iter++;
    }
    return false;
  }

private:
  scalar_t linf_threshold_;
}; // class PublishOnLinfShift



/** @brief A PublishPolicy that publishes if a ratio of values has
 * shifted by a threshold.
 *
 * @tparam scalar_t The same scalar type as used in the PushSumProcessor.
 */
template<typename scalar_t = double>
class PublishOnRatioShift
{
public:

  PublishOnRatioShift(scalar_t thresh, size_t ind1, size_t ind2)
    : shift_threshold_(thresh),
      ind1_(ind1), ind2_(ind2)
  {  }

  template<typename ValueType>
  bool operator()(const ValueType& new_vals, const ValueType& old_vals)
  {
    scalar_t new_ratio = new_vals[ind1_] / new_vals[ind2_];
    scalar_t old_ratio = old_vals[ind1_] / old_vals[ind2_];
    return (new_ratio - old_ratio) > shift_threshold_ || (old_ratio - new_ratio) > shift_threshold_;
  }

private:
  scalar_t shift_threshold_;
  size_t ind1_, ind2_;
}; // class PublishRatioShift

#endif // UPDATE_NBRS_CRITERION_HPP
