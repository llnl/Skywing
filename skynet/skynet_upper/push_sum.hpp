#ifndef SKYNET_UPPER_PUSH_SUM_HPP
#define SKYNET_UPPER_PUSH_SUM_HPP

#include "skynet_core/job.hpp"
#include "skynet_core/master.hpp"
#include "skynet_upper/asynchronous_iterative.hpp"
#include "skynet_upper/stopping_criterion.hpp"
#include "skynet_upper/update_nbrs_criterion.hpp"

/**
 * Push Sum is an asynchronous distributed averaging algorithm that
 * converges to the average of initial values as long as information
 * delay is no longer than exponential in time.
 * This algorithm still converges even if there packet loss, it just
 * doesn't necessarily converge to the average in this case, although
 * it still stagnate to SOME convex combination of initial values.
 * 
 * @param[in] x_value initial value for distributed averaging. 
 * @param[out] x_value/y_value obtained from return_solution(), so the solution is a ratio.
 * 
 */

using namespace skynet;

template<typename S>
class PushSum
{
public:
  using scalar_t = S;
  using ValueType = std::vector<scalar_t>;
  using ValueTag = skynet::PublishTag<ValueType>;

  template<typename Range>
  PushSum(size_t size_of_system,
          size_t number_of_neighbors,
          scalar_t starting_value,
          Range& tags)
    : size_of_system_(size_of_system),
    number_of_neighbors_(number_of_neighbors),
    x_value_(starting_value)
  {
    for (const auto& tag : tags)
    {
      rho_x_[tag] = 0.0;
      rho_y_[tag] = 0.0;
      rho_x_previous_[tag] = 0.0;
      rho_y_previous_[tag] = 0.0;
    }
    in_nodes_plus_one_ = number_of_neighbors_ + 1.0;
    // Local weights -> This is the information passed to neighbors.
    sigma_x_ = sigma_x_ + (x_value_ / in_nodes_plus_one_);
    sigma_y_ = sigma_y_ + (y_value_ / in_nodes_plus_one_);
  }

  ValueType get_init_publish_values()
  { return {sigma_x_, sigma_y_, new_information_count_}; }

  template<typename IterativeWrapper>
  void process_update(const ValueTag& nbr_tag, const ValueType& nbr_values,
                      const IterativeWrapper& wrapper)
  {
    if (nbr_tag == wrapper.my_tag())
      return;

    new_information_count_ +=1.0;

    rho_x_previous_[nbr_tag] = rho_x_[nbr_tag];
    rho_y_previous_[nbr_tag] = rho_y_[nbr_tag];
    rho_x_[nbr_tag] = nbr_values[0];
    rho_y_[nbr_tag] = nbr_values[1];

    x_value_ = x_value_ + rho_x_[nbr_tag] - rho_x_previous_[nbr_tag];
    y_value_ = y_value_ + rho_y_[nbr_tag] - rho_y_previous_[nbr_tag];

    // This is the 'wake up' portion followed by broadcast (push_sum theory relevant)
    sigma_x_ = sigma_x_ + (x_value_ / in_nodes_plus_one_);
    sigma_y_ = sigma_y_ + (y_value_ / in_nodes_plus_one_);

    x_value_ = x_value_ / in_nodes_plus_one_;
    y_value_ = y_value_ / in_nodes_plus_one_;

    // Checks the max neighbor iterations for terminal checks.
    if (nbr_values[2] > max_neighbor_received_)
    {
      max_neighbor_received_ = nbr_values[2];
    }
  }

  ValueType prepare_for_publication(ValueType vals_to_publish)
  {
    vals_to_publish[0] = sigma_x_;
    vals_to_publish[1] = sigma_y_;
    vals_to_publish[2] = new_information_count_;
    return vals_to_publish;
  }

  scalar_t return_solution() const
  {
    scalar_t consensus_value = x_value_/y_value_;
    return consensus_value;
  }

 double return_new_information_count() const
  {
    return new_information_count_;
  }
  
private:
  size_t size_of_system_;
  size_t number_of_neighbors_;
  double new_information_count_ = 0.0;
  double max_neighbor_received_ = 0.0;
  scalar_t x_value_;
  scalar_t y_value_ = 1.0;
  // this is the number of neighbors plus one needed for the update
  // rule
  scalar_t in_nodes_plus_one_;
   // Local weights.
  scalar_t sigma_x_;
  scalar_t sigma_y_;
  // stores iterate information
  using tag_val_map = std::unordered_map<ValueTag, scalar_t, skynet::internal::hash<ValueTag>>;
  tag_val_map rho_x_;
  tag_val_map rho_y_;
  tag_val_map rho_x_previous_;
  tag_val_map rho_y_previous_;
};  // class PushSum



template<typename scalar_t>
class UpdateIfConsensusShift
{
public:
  template<typename CallerT>
  UpdateIfConsensusShift(const CallerT& caller)
    : shift_threshold_(1e-4)
  {
    (void)caller;
  }

  template<typename ValueType>
  bool operator()(const ValueType& new_vals, const ValueType& old_vals)
  {
    scalar_t new_ratio = new_vals[0] / new_vals[1];
    scalar_t old_ratio = old_vals[0] / old_vals[1];
    return (new_ratio - old_ratio) > shift_threshold_ || (old_ratio - new_ratio) > shift_threshold_;
  }

private:
  scalar_t shift_threshold_;
}; // class UpdateIfConsensusShift

template<typename Range>
auto create_push_sum(int size_of_system,
                     int number_of_neighbors,
                     double starting_value,
                     MasterHandle handle,
                     Job& job,
                     const typename PushSum<double>::ValueTag& produced_tag,
                     const Range tags) noexcept
{
  using AsynchT = AsynchronousIterative<PushSum<double>, UpdateIfConsensusShift<double>, StopAfterTime>;
  return create_asynchronous_iterative<AsynchT, Range>
    (handle, job, produced_tag, tags, size_of_system, number_of_neighbors, starting_value);
}

// // This is the continuation that makes this class possible as this
// // implementation depends upon the asynchronous_iterative class.
// template<typename... Args>
// auto create_push_sum(int machine_number, int size_of_system, int number_of_neighbors, double starting_value, ValueTag myTag, std::vector<ValueTag> tags,  Args&&... args) noexcept
// {
//   return create_asynchronous_iterative(std::forward<Args>(args)...).then([=](std::optional<AsynchronousIterative<std::vector<double>>> it) -> std::optional<PushSum> {
//      if (it) { return PushSum(machine_number, size_of_system, number_of_neighbors, starting_value, myTag, tags, *it);}
//      else    { return {}; }
//   });
// }

#endif
