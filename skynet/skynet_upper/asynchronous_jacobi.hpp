#ifndef SKYNET_UPPER_ASYNCHRONOUS_JACOBI_HPP
#define SKYNET_UPPER_ASYNCHRONOUS_JACOBI_HPP

#include "skynet_core/job.hpp"
#include "skynet_core/master.hpp"
#include "skynet_upper/asynchronous_iterative.hpp"
#include "skynet_upper/stopping_criterion.hpp"
#include "skynet_upper/update_nbrs_criterion.hpp"

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

template<typename S = double>
class AsynchronousJacobi
{
public:
  using scalar_t = S;
  using ValueType = std::vector<scalar_t>;
  using ValueTag = skynet::PublishTag<ValueType>;

  AsynchronousJacobi(
    std::vector<std::vector<scalar_t>> A_partition,
    std::vector<scalar_t> b_partition,
    std::vector<size_t> row_indices)
    : A_partition_(A_partition),
      b_partition_(b_partition),
      row_indices_(row_indices),
      number_of_updated_components_(row_indices_.size()),
      x_iter_(A_partition_[0].size(), 0.0),
      publish_values_(2 * number_of_updated_components_, 0.0)
  {
    jacobi_computation();
  }

  ValueType get_init_publish_values()
  { return std::vector<scalar_t>(2 * number_of_updated_components_, 0.0); }

  template<typename IterativeWrapper>
  void process_update([[maybe_unused]] const ValueTag& nbr_tag,
                      const ValueType& nbr_values,
                      [[maybe_unused]] const IterativeWrapper& wrapper)
  {
    // This cycles through the received_values in order not to
    // replace a component that each process is updating with
    // another processes update if there's overlapping
    // computations.  Since messages of the form [component index
    // ; component], we have to parse these messages in pairs,
    // which is easier to do without iterators.
    for(size_t nbr_vals_ind = 0; nbr_vals_ind < (nbr_values.size()/2); nbr_vals_ind++)
    {
      bool use_this_value = true;
      // Cycles through individual values in row_index to avoid
      // replacing its own updates if there's overlap in the
      // linear system partition.
      for(size_t row_index_cycle = 0 ; row_index_cycle < number_of_updated_components_; row_index_cycle++)
      {
        if(nbr_values[nbr_vals_ind*2] == row_indices_[row_index_cycle])
          use_this_value = false;
      }
      if(use_this_value)
      {
        size_t updated_index = static_cast<size_t>(nbr_values[nbr_vals_ind * 2]);
        x_iter_[updated_index] = nbr_values[nbr_vals_ind * 2 + 1];
        jacobi_computation();
      }
    }
  }

  ValueType prepare_for_publication(ValueType vals_to_publish)
  {
    for(size_t i = 0 ; i < number_of_updated_components_; i ++)
    {
      vals_to_publish[i*2] = row_indices_[i]*1.0;
      vals_to_publish[i*2+1] = x_iter_[row_indices_[i]];
    }
    return vals_to_publish;
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
  }

  // Returns only the components for which this process updates. 
  // Since jacobi is a row - wise operation, this is NOT the full x_iter.  
  // If return_partial_solution() = return_partition_solution(), then
  // each process solved the entire linear system by themselves.
  std::vector<scalar_t> return_partition_solution() const
  {
    std::vector<scalar_t> return_vector;
    for(size_t i = 0 ; i < number_of_updated_components_; i++)
    {
      return_vector.push_back(x_iter_[row_indices_[i]]);
    }
    return return_vector;
  }

  const std::vector<scalar_t>& return_full_solution() const
  {
    return x_iter_;
  }

private:
  std::vector<std::vector<scalar_t>> A_partition_;
  std::vector<scalar_t> b_partition_;
  std::vector<size_t> row_indices_;
  size_t number_of_updated_components_;
  size_t iteration_count_ = 0;

  // Variables internal to this class.
  std::vector<scalar_t> x_iter_;
  std::vector<scalar_t> publish_values_;
}; // class AsynchronousJacobi


template<typename Range>
auto create_asynchronous_jacobi(
    std::vector<std::vector<double>> A_partition,
    std::vector<double> b_partition,
    std::vector<size_t> row_indices,
    MasterHandle handle,
    Job& job,
    const typename AsynchronousJacobi<double>::ValueTag& produced_tag,
    const Range& tags) noexcept
{
  using AsynchT = AsynchronousIterative<AsynchronousJacobi<double>, UpdateNbrsOnLinf<double>, StopAfterTime>;
  return create_asynchronous_iterative<AsynchT, Range>
    (handle, job, produced_tag, tags, A_partition, b_partition, row_indices);
  
  // return create_asynchronous_iterative<AsynchronousJacobi<double>, decltype(tags)>
  //   (std::forward<Args>(args)...)
  //   .then([=](std::optional<AsynchronousIterative<std::vector<double>>> it)
  //         -> std::optional<AsynchronousJacobi<double>> {
  //    if (it) {
  //      return AsynchronousJacobi<double>(A_partition, b_partition, row_indices, tags, *it);
  //    }
  //    else {
  //      return {};
  //    }
  //     });

}

#endif 
