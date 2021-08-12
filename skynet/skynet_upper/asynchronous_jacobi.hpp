#ifndef SKYNET_UPPER_ASYNCHRONOUS_JACOBI_HPP
#define SKYNET_UPPER_ASYNCHRONOUS_JACOBI_HPP

#include "skynet_core/job.hpp"
#include "skynet_core/master.hpp"
#include "skynet_upper/asynchronous_iterative.hpp"
#include "skynet_upper/stopping_criterion.hpp"

/** 
 * Solves the square linear system Ax=b with the Asynchronous Jacobi algorithm. This assumes that the system is square, consistent, and diagonally dominant to be solvable by this method. This class stores the full solution vector x_iter at each machine (return_full_solution), as well as the partition of x_iter corresponding to the partition Ax=b that each machine is responsible for updating (return_partition_solution ), i.e., if a machine is responsible for updating components 0 and 1 of "x" from Ax=b, then the only the first two components of x_iter are returned for (return_full_solution).
 * 
 * This implementation is agnostic of overlapping computations, nonuniform partitioning, and non-sequential partitioning of the linear system. This is why the row_index for each component of x must be sent along with every method for proper processing.
 * 
 * @param[in] A_partition A matrix row partition
 * @param[in] b_partition b vector row partition -> same as A partition
 * @param[in] row_indices indices which correspond to the row the partition above with respect to the full system "Ax=b"
 * 
 * @param[out] x_iter -> solution vector x from the linear solver
 * @param[out] x_iter[row_indices] -> partition solution vector x that the individual skynet process is responsible for updating.
 * 
*/

using namespace skynet;
using ValueTag = skynet::PublishTag<std::vector<double>>;

class AsynchronousJacobi
{

private:

  int machine_number;
  int size_of_linear_system;
  std::vector<std::vector<double>> A_partition;
  std::vector<double> b_partition;
  std::vector<int> row_indices;
  std::vector<ValueTag> tags_vector;
  int number_of_updated_components;
  // Variables internal to this class.
  std::vector<double> x_iter;
  std::vector<double> publish_values;
  // Variables for stopping criterion
  int debug_machine_number;
  int delayed_iteration_count;
  int delayed_process ;
  int new_information_count = 0;
  std::chrono::duration<double> max_run_time;
  int max_new_information;
  bool iterate = true;
  std::chrono::duration<double> run_time = std::chrono::milliseconds(1);
  // Skynet asynchronous iterative variables. 
  friend class AsynchronousIterative<std::vector<double>>;
  AsynchronousIterative<std::vector<double>> iter_method ;
  
public:

  AsynchronousJacobi(int machine_number, std::vector<std::vector<double>> A_partition, std::vector<double> b_partition, std::vector<int> row_indices, std::vector<ValueTag> tags_vector, AsynchronousIterative<std::vector<double>> it):
    machine_number(machine_number),
    A_partition(A_partition),
    b_partition(b_partition),
    row_indices(row_indices),
    tags_vector(tags_vector),
    iter_method(it)
  {

    size_of_linear_system = static_cast<int>(A_partition[0].size());
    number_of_updated_components = static_cast<int>(row_indices.size());
    for(int i = 0 ; i < size_of_linear_system; i ++)
    {
      x_iter.push_back(0.0);
    }
    for(int i = 0 ; i < number_of_updated_components*2; i++)
    {
      publish_values.push_back(0.0);
    }
    max_new_information = 1000 * size_of_linear_system;
    max_run_time = std::chrono::milliseconds(100000)* size_of_linear_system;
    // This serves as the initial computation and broadcast.
    jacobi_computation();
    obtain_publish_values();
    iter_method.submit_values(publish_values);
  };

  void run()
  {
    auto start_jacobi = std::chrono::high_resolution_clock::now();
    auto stop_jacobi = std::chrono::high_resolution_clock::now();
    while(iterate)
    {
      create_iteration();
      // This allows for quick diagnostics by seeing what's in the buffers rho_x, rho_y,  at terminal.
      // print_current_information();
      stop_jacobi = std::chrono::high_resolution_clock::now();
      run_time = std::chrono::duration_cast<std::chrono::microseconds>(stop_jacobi - start_jacobi);
      iterate = should_stop(run_time, max_run_time, new_information_count, max_new_information);
      // iterate = should_stop(return_new_information_count(), max_new_information);
    }
  };

  void create_iteration()
  {
    // This stores the values as a AsynchronousValues allocator which contains a bool if it is updated and a vector<double> and alive tags as vector<ValueTag>.
    const auto& [values, alive_tags] = iter_method.values();
    // Cycles through received information associated with the tags this process subscribes to.
    for(int values_index = 0; values_index < static_cast<int>(values.size()); ++values_index)
    {
      //stores the received values as a vector<double> and updated as bool
      const auto& [received_values, updated] = values[values_index];
      if (updated)
      {
        // This cycles through the received_values in order not to replace a component that each process is updating with another processes update if there's overlapping computations.
        // Since messages of the form [component index ; component], we have to parse these messages in pairs, which is easier to do without iterators.
        for(int received_values_index = 0; received_values_index < (static_cast<int>(received_values.size())/2); received_values_index++)
        {
          bool use_this_value = true;
          // Cycles through individual values in row_index to avoid replacing it's own updates if there's overlap in the linear system partition.
          for(int row_index_cycle = 0 ; row_index_cycle < number_of_updated_components; row_index_cycle++)
          {
            if((int)received_values[received_values_index*2] == row_indices[row_index_cycle])
            {
              use_this_value = false;
            }
          }
          if(use_this_value)
          {
            new_information_count++;
            int updated_index = (int) received_values[received_values_index * 2];
            x_iter[updated_index] = received_values[received_values_index * 2 + 1];
            jacobi_computation();
            // This submits values after each piece of new information.
            obtain_publish_values();
            iter_method.submit_values(publish_values);
          }
        }
      }
    }
  };

  void obtain_publish_values()
  {
    for(int i = 0 ; i < number_of_updated_components; i ++)
    {
      publish_values[i*2] = row_indices[i]*1.0;
      publish_values[i*2+1] = x_iter[row_indices[i]];
    }
  }

  void jacobi_computation()
  {
    for(int i = 0 ; i < number_of_updated_components; i++)
    {
      double hold= 0.0;
      for(int j = 0 ; j < static_cast<int>(A_partition[0].size()); j++)
      {
        if(j!=row_indices[i])
          hold += A_partition[i][j]*x_iter[j];
      }
      hold = (b_partition[i] - hold)/A_partition[i][row_indices[i]];
      int updated_index = (int)row_indices[i];
      x_iter[updated_index] = hold;
    }
  };

  // Returns only the components for which this process updates. 
  // Since jacobi is a row - wise operation, this is NOT the full x_iter. 
  // If return_partial_solution() = return_partition_solution(), then each process solved the entire linear system by themselves.
  std::vector<double> return_partition_solution()
  {
    std::vector<double> return_vector;
    for(int i = 0 ; i < number_of_updated_components; i++)
    {
      return_vector.push_back(x_iter[row_indices[i]]);
    }
    return return_vector;
  };

  std::vector<double> return_full_solution()
  {
    return x_iter;
  }

  // Prints only the vector components of x  which this process updates. 
  void print_partition_solution()
  {
    std::cout << "\t machine " << machine_number << "\tsolution ";

    for(int i = 0 ; i < number_of_updated_components; i ++)
    {
      std::cout << x_iter[row_indices[i]] << " ";
    }
    std::cout << std::endl;
  };

  void print_solution()
  {
    std::cout << "\t machine " << machine_number << "\t full solution ";
    for(std::vector<double>::size_type i = 0 ; i < x_iter.size(); i++)
    {
      std::cout << x_iter[i] << " ";
    }
    std::cout << std::endl;
  };

  double return_runtime()
  {
    return run_time.count();
  }

  bool return_iterate()
  {
    return iterate;
  }

  int return_information_received()
  {
    return new_information_count;
  }

  // Diagnostic output to track received information from asynchronous iter_method
  void print_all_received_information(skynet::AsynchronousValues<std::vector<double>> values)
  {

    std::cout << machine_number << " has information: \n\t";
    for(int values_index = 0; values_index < static_cast<int>(values.size()); ++values_index)
    {
      const auto& [received_values, updated] = values[values_index];
      if (updated)
      {
        std::cout << " values_index: " << values_index << "\n\t\t";
        for(int received_values_index = 0; received_values_index < number_of_updated_components; received_values_index++)
        {
          std::cout << received_values[received_values_index] << " ";
        }
        if(values_index < static_cast<int>(values.size())-1)
        {
          std::cout << "\n\t";

        }
        else
        {
          std::cout << "\n";
        }
      }
    }
  }

};

// This is the continuation that makes this class possible as this implementation depends upon the asynchronous_iterative class.
template<typename... Args>
auto create_asynchronous_jacobi(int machine_number, std::vector<std::vector<double>> A_partition, std::vector<double> b_partition, std::vector<int> row_indices, std::vector<ValueTag> tags, Args&&... args) noexcept
{
  return create_asynchronous_iterative(std::forward<Args>(args)...).then([=](std::optional<AsynchronousIterative<std::vector<double>>> it) -> std::optional<AsynchronousJacobi> {
     if (it) { return AsynchronousJacobi(machine_number, A_partition, b_partition, row_indices, tags, *it);}
     else    { return {}; }
  });

}

#endif 