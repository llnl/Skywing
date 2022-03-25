#ifndef SOFTMAX_COUNT_PROCESSOR_HPP
#define SOFTMAX_COUNT_PROCESSOR_HPP

#include "skynet_mid/idempotent_processor.hpp"
#include "skynet_mid/push_sum_processor.hpp"
#include <chrono>
#include <random>
#include <cmath>

using namespace skynet;

template<typename real_t = double,
         typename MinProc = MinProcessor<real_t>,
         typename MeanProc = PushSumProcessor<real_t>>
class SoftmaxCountProcessor
{
public:
  using ValueType = std::tuple<typename MinProc::ValueType, typename MeanProc::ValueType>;

  
  SoftmaxCountProcessor(size_t number_of_neighbors,
                        double lambda = 1e-3)
    : my_val_(get_exponential_dist_value(lambda)),
      min_processor_(my_val_),
      mean_processor_(number_of_neighbors, std::exp(-my_val_))
  {
    std::cout << "Have my_val_=" << my_val_ << std::endl;
  }
  
  ValueType get_init_publish_values()
  {
    return ValueType(min_processor_.get_init_publish_values(),
                     mean_processor_.get_init_publish_values());
  }

  template<typename NbrDataHandler, typename IterMethod>
  void process_update(const NbrDataHandler& nbr_data_handler, const IterMethod& iter_method)
  {
    auto min_data_handler = nbr_data_handler.template get_sub_handler<typename MinProc::ValueType>([](const ValueType& v){return std::get<0>(v);});
    min_processor_.process_update(min_data_handler, iter_method);

    auto mean_data_handler = nbr_data_handler.template get_sub_handler<typename MeanProc::ValueType>([](const ValueType& v){return std::get<1>(v);});
    mean_processor_.process_update(mean_data_handler, iter_method);
  }

  ValueType prepare_for_publication(ValueType v)
  {
    return ValueType(min_processor_.prepare_for_publication(std::get<0>(v)),
                     mean_processor_.prepare_for_publication(std::get<1>(v)));
  }

  real_t get_raw_count() const
  {
    return std::exp(-(std::log(mean_processor_.return_solution())
                      + min_processor_.get_value()));
  }
  size_t get_count() const
  {
    return static_cast<size_t>(std::round(get_raw_count()));
  }

  double get_min() const {return min_processor_.get_value();}
  double get_mean() const {return mean_processor_.return_solution();}

private:

  // Draw a value at random from the exponential distribution with
  // parameter lambda
  real_t get_exponential_dist_value(double lambda)
  {
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed);
    std::uniform_real_distribution<real_t> distribution(0.0, 1.0);

    real_t p = distribution(generator);
    return -std::log(1-p) / lambda;
  }
  
  real_t my_val_;
  MinProc min_processor_;
  MeanProc mean_processor_;

  
}; // class SoftmaxCountProcessor

#endif // SOFTMAX_COUNT_PROCESSOR_HPP
