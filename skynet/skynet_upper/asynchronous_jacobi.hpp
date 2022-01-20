#ifndef SKYNET_UPPER_ASYNCHRONOUS_JACOBI_HPP
#define SKYNET_UPPER_ASYNCHRONOUS_JACOBI_HPP

#include "skynet_core/job.hpp"
#include "skynet_core/master.hpp"
#include "skynet_upper/asynchronous_iterative.hpp"
#include "skynet_upper/stopping_criterion.hpp"

/** 
 * Solves the square linear system Ax=b with the Asynchronous Jacobi
 * algorithm. This assumes that the system is square, consistent, and
 * diagonally dominant to be solvable by this method. This class
 * stores the full solution vector x_iter at each machine
 * (return_full_solution), as well as the partition of x_iter
 * corresponding to the partition Ax=b that each machine is
 * responsible for updating (return_partition_solution ), i.e., if a
 * machine is responsible for updating components 0 and 1 of "x" from
 * Ax=b, then the only the first two components of x_iter are returned
 * for (return_full_solution).
 * 
 * This implementation is agnostic of overlapping computations,
 * nonuniform partitioning, and non-sequential partitioning of the
 * linear system. This is why the row_index for each component of x
 * must be sent along with every method for proper processing.
 * 
 * @param[in] A_partition A matrix row partition
 * @param[in] b_partition b vector row partition -> same as A partition
 * @param[in] row_indices indices which correspond to the row the partition above with respect to the full system "Ax=b"
 * 
 * @param[out] x_iter -> solution vector x from the linear solver
 * @param[out] x_iter[row_indices] -> partition solution vector x that
 * the individual skynet process is responsible for updating.
 * 
*/

using namespace skynet;

template<typename value_type = double>
class AsynchronousJacobi
{
public:
  using value_t = value_type;
  using ValueTag = skynet::PublishTag<std::vector<value_t>>;

  AsynchronousJacobi(
    std::vector<std::vector<value_t>> A_partition,
    std::vector<value_t> b_partition,
    std::vector<size_t> row_indices,
    std::vector<ValueTag> tags_vector,
    AsynchronousIterative<std::vector<value_t>> it)
    : A_partition_(A_partition),
      b_partition_(b_partition),
      row_indices_(row_indices),
      tags_vector_(tags_vector),
      number_of_updated_components_(row_indices_.size()),
      x_iter_(A_partition_[0].size(), 0.0),
      publish_values_(2 * number_of_updated_components_, 0.0),
      iter_method_(it)
  {
    max_run_time = std::chrono::seconds(5);
    // This serves as the initial computation and broadcast.
    jacobi_computation();
    obtain_publish_values();
    iter_method_.submit_values(publish_values_);
  };

  void run()
  {
    auto start_jacobi = std::chrono::high_resolution_clock::now();
    auto stop_jacobi = std::chrono::high_resolution_clock::now();
    while(iterate)
    {
      ++iteration_count_;
      create_iteration();
      stop_jacobi = std::chrono::high_resolution_clock::now();
      run_time = std::chrono::duration_cast<std::chrono::microseconds>(stop_jacobi - start_jacobi);
      iterate = should_stop(run_time, max_run_time);

      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  };

  void create_iteration()
  {
    // This stores the values as a AsynchronousValues allocator which
    // contains a bool if it is updated and a vector<double> and alive
    // tags as vector<ValueTag>.
    const auto& [received_values_vec, is_updated, alive_tags] = iter_method_.values();
    // Cycles through received information associated with the tags
    // this process subscribes to.
    for(size_t values_index = 0; values_index < received_values_vec.size(); ++values_index)
    {
      //stores the received values as a vector<double> and updated as bool
      //const auto& [received_values, updated] = values[values_index];
      if (is_updated[values_index])
      {
        const auto& received_values = received_values_vec[values_index];
        // This cycles through the received_values in order not to
        // replace a component that each process is updating with
        // another processes update if there's overlapping
        // computations.  Since messages of the form [component index
        // ; component], we have to parse these messages in pairs,
        // which is easier to do without iterators.
        for(size_t received_values_index = 0; received_values_index < (received_values.size()/2);
            received_values_index++)
        {
          bool use_this_value = true;
          // Cycles through individual values in row_index to avoid
          // replacing it's own updates if there's overlap in the
          // linear system partition.
          for(size_t row_index_cycle = 0 ; row_index_cycle < number_of_updated_components_; row_index_cycle++)
          {
            if(received_values[received_values_index*2] == row_indices_[row_index_cycle])
              use_this_value = false;
          }
          if(use_this_value)
          {
            size_t updated_index = static_cast<size_t>(received_values[received_values_index * 2]);
            x_iter_[updated_index] = received_values[received_values_index * 2 + 1];
            jacobi_computation();
            // This submits values after each piece of new information.
            obtain_publish_values();
            iter_method_.submit_values(publish_values_);
          }
        }
      }
    }
  };

  void obtain_publish_values()
  {
    for(size_t i = 0 ; i < number_of_updated_components_; i ++)
    {
      publish_values_[i*2] = row_indices_[i]*1.0;
      publish_values_[i*2+1] = x_iter_[row_indices_[i]];
    }
  }

  void jacobi_computation()
  {
    for(size_t i = 0 ; i < number_of_updated_components_; i++)
    {
      double hold = 0.0;
      for(size_t j = 0 ; j < A_partition_[0].size(); j++)
      {
        if(j!=row_indices_[i])
          hold += A_partition_[i][j]*x_iter_[j];
      }
      hold = (b_partition_[i] - hold)/A_partition_[i][row_indices_[i]];
      size_t updated_index = row_indices_[i];
      x_iter_[updated_index] = hold;
    }
  };

  // Returns only the components for which this process updates. 
  // Since jacobi is a row - wise operation, this is NOT the full x_iter.  
  // If return_partial_solution() = return_partition_solution(), then
  // each process solved the entire linear system by themselves.
  std::vector<value_t> return_partition_solution()
  {
    std::vector<value_t> return_vector;
    for(size_t i = 0 ; i < number_of_updated_components_; i++)
    {
      return_vector.push_back(x_iter_[row_indices_[i]]);
    }
    return return_vector;
  };

  std::vector<value_t> return_full_solution()
  {
    return x_iter_;
  }

  double return_runtime()
  {
    return run_time.count();
  }

  bool return_iterate()
  {
    return iterate;
  }

  unsigned get_iteration_count()
  {
    return iteration_count_;
  }

private:
  std::vector<std::vector<double>> A_partition_;
  std::vector<double> b_partition_;
  std::vector<size_t> row_indices_;
  std::vector<ValueTag> tags_vector_;
  size_t number_of_updated_components_;
  size_t iteration_count_ = 0;

  // Variables internal to this class.
  std::vector<value_t> x_iter_;
  std::vector<value_t> publish_values_;

  // Variables for stopping criterion
  std::chrono::duration<double> max_run_time;
  bool iterate = true;
  std::chrono::duration<double> run_time = std::chrono::milliseconds(1);
  
  // Skynet asynchronous iterative variables. 
  friend class AsynchronousIterative<std::vector<double>>;
  AsynchronousIterative<std::vector<double>> iter_method_ ;
}; // class AsynchronousJacobi

// This is the continuation that makes this class possible as this
// implementation depends upon the asynchronous_iterative class.
template<typename... Args>
auto create_asynchronous_jacobi(
    std::vector<std::vector<double>> A_partition,
    std::vector<double> b_partition,
    std::vector<size_t> row_indices,
    std::vector<typename AsynchronousJacobi<double>::ValueTag> tags,
    Args&&... args) noexcept
{
  return create_asynchronous_iterative(std::forward<Args>(args)...)
    .then([=](std::optional<AsynchronousIterative<std::vector<double>>> it) -> std::optional<AsynchronousJacobi<double>> {
     if (it) {
       return AsynchronousJacobi<double>(A_partition, b_partition, row_indices, tags, *it);
     }
     else {
       return {};
     }
      });

}

#endif 
