#ifndef SKYNET_UPPER_PUSH_SUM_HPP
#define SKYNET_UPPER_PUSH_SUM_HPP

#include "skynet_core/job.hpp"
#include "skynet_core/master.hpp"

using namespace skynet;

/**
 * Push Sum is an asynchronous distributed averaging algorithm that
 * converges to the average of initial values as long as information
 * delay is no longer than exponential in time.
 * This algorithm still converges even if there packet loss, it just
 * doesn't necessarily converge to the average in this case, although
 * it still stagnate to SOME convex combination of initial values.
 * 
 * @tparam S The scalar type used in the average, e.g. double.
 *
 * @param[in] x_value initial value for distributed averaging. 
 * @param[out] x_value/y_value obtained from return_solution(), so the solution is a ratio.
 * 
 */
template<typename S = double>
class PushSumProcessor
{
public:
  using scalar_t = S;
  using ValueType = std::vector<scalar_t>;
  using ValueTag = skynet::PublishTag<ValueType>;

  /**
   * @param number_of_neighbors Number of neighboring agents.
   * @param starting_values This agent's contribution to the average.
   * @param tags The <em>assumed already subscribed</em> tags of neighbors.
   */
  template<typename Range>
  PushSumProcessor(size_t number_of_neighbors,
                   scalar_t starting_value,
                   Range& tags)
    : number_of_neighbors_(number_of_neighbors),
      x_value_(starting_value)
  {
    // TODO do we need these tags in the constructor? Maybe we can
    // just initialize on-the-go during processing.
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

  /** @brief Perform the push-sum update computation.
   *
   * @param nbr_tags The tags of the updated values. Each tag is an element of this->tags_.
   * @param nbr_values The new values from the neighbors.
   * @param caller The iterative wrapper calling this method.
   */
  template<typename IterativeWrapper>
  void process_update(const std::vector<ValueTag>& nbr_tags,
                      const std::vector<ValueType>& nbr_values,
                      const IterativeWrapper& wrapper)
  {
    for (size_t i = 0; i < nbr_tags.size(); i++)
    {
      if (nbr_tags[i] == wrapper.my_tag())
        continue;

      const ValueTag& nbr_tag = nbr_tags[i];
      const ValueType& nbr_value = nbr_values[i];

      rho_x_previous_[nbr_tag] = rho_x_[nbr_tag];
      rho_y_previous_[nbr_tag] = rho_y_[nbr_tag];
      rho_x_[nbr_tag] = nbr_value[0];
      rho_y_[nbr_tag] = nbr_value[1];

      x_value_ = x_value_ + rho_x_[nbr_tag] - rho_x_previous_[nbr_tag];
      y_value_ = y_value_ + rho_y_[nbr_tag] - rho_y_previous_[nbr_tag];

      // This is the 'wake up' portion followed by broadcast (push_sum theory relevant)
      sigma_x_ = sigma_x_ + (x_value_ / in_nodes_plus_one_);
      sigma_y_ = sigma_y_ + (y_value_ / in_nodes_plus_one_);

      x_value_ = x_value_ / in_nodes_plus_one_;
      y_value_ = y_value_ / in_nodes_plus_one_;
      
      // Checks the max neighbor iterations for terminal checks.
      // TODO: is this needed?
      if (nbr_value[2] > max_neighbor_received_)
      {
        max_neighbor_received_ = nbr_value[2];
      }
      new_information_count_ +=1.0;
    }
  }

  /** @brief Prepare values to send to neighbors.
   */
  ValueType prepare_for_publication(ValueType vals_to_publish)
  {
    vals_to_publish[0] = sigma_x_;
    vals_to_publish[1] = sigma_y_;
    vals_to_publish[2] = new_information_count_;
    return vals_to_publish;
  }

  /** @brief Returns the current estimate of the global average.
   */
  scalar_t return_solution() const
  {
    scalar_t consensus_value = x_value_/y_value_;
    return consensus_value;
  }

 double return_new_information_count() const
  {
    return new_information_count_;
  }

  scalar_t get_x() const {return x_value_;}
  scalar_t get_y() const {return y_value_;}
  
private:
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
};  // class PushSumProcessor


#endif
