#ifndef SKYNET_UPPER_SYNCHRONOUS_JACOBI_HPP
#define SKYNET_UPPER_SYNCHRONOUS_JACOBI_HPP

#include "skynet_core/job.hpp"
#include "skynet_core/master.hpp"
#include "skynet_upper/synchronous_iterative.hpp"
#include "skynet_upper/stopping_criterion.hpp"

/** 
 * Solves the square linear system Ax=b with the Jacobi algorithm. This assumes that the system is square, consistent, and diagonally dominant to be solvable by this method. This class stores the full solution vector x_iter at each machine (return_full_solution), as well as the partition of x_iter corresponding to the partition Ax=b that each machine is responsible for updating (return_partition_solution ), i.e., if a machine is responsible for updating components 0 and 1 of "x" from Ax=b, then the only the first two components of x_iter are returned for (return_full_solution).
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

class SynchronousJacobi
{

private:

  int machine_number;
  int size_of_linear_system;
  int row_index;
  std::vector<std::vector<double>> A_partition; 
  std::vector<double> b_partition;
  std::vector<int> row_indices;
  int number_of_updated_components;
  // Variables internal to this class.
  std::vector<double> x_iter;
  std::vector<double> publish_values;
  // Variables for stopping criterion
  int new_information_count = 0;
  int max_new_information;
  std::chrono::duration<double> run_time = std::chrono::milliseconds(1);
  std::chrono::duration<double> max_run_time;
  bool iterate = true;

  // Skynet synchronous iterative variables
  friend class SynchronousIterative<std::vector<double>>;
  SynchronousIterative<std::vector<double>> iter_method ;

public:

  SynchronousJacobi(int machine_number,  std::vector<std::vector<double>> A_partition, std::vector<double> b_partition, std::vector<int> row_indices, SynchronousIterative<std::vector<double>> it): 
  machine_number(machine_number), 
  A_partition(A_partition),
  b_partition(b_partition), 
  row_indices(row_indices),
  iter_method(it)
  {
    size_of_linear_system = static_cast<int>(A_partition[0].size());
    number_of_updated_components = static_cast<int>(row_indices.size());
    x_iter.resize(A_partition[0].size(), 0.0);
    publish_values.resize(number_of_updated_components*2,0.0);
    max_new_information = 1000 * size_of_linear_system;
    max_run_time = std::chrono::milliseconds(1000000) *size_of_linear_system;
    // Initial computation and send message, but not initial broadcast.
    jacobi_computation();
    obtain_publish_values();
  };

  ~SynchronousJacobi(){};

  void run()
  {
    auto start_jacobi = std::chrono::high_resolution_clock::now();
    auto stop_jacobi = std::chrono::high_resolution_clock::now();
    while(iterate)
    {
      create_iteration();
      stop_jacobi = std::chrono::high_resolution_clock::now();
      run_time = std::chrono::duration_cast<std::chrono::microseconds>(stop_jacobi - start_jacobi);
      iterate = should_stop(run_time, max_run_time, new_information_count, max_new_information);
    }
  };

  void create_iteration()
  {
    //This collects all publishes values, including the one of the home process.
    const auto values = iter_method.values(publish_values).get();
    for (auto recv_message : values)
    {
      for(int receive_index=0; receive_index < static_cast<int>(recv_message.size()/2) ; receive_index++)
      {
        int update_index = (int) recv_message[receive_index*2];
        bool use_this_value = true;
        for (int row_index = 0 ; row_index < number_of_updated_components; row_index++)
        {
          if(update_index == row_indices[row_index*2])
          {
            use_this_value = false;
          }
        }
        if(use_this_value)
        {
          new_information_count++;
          x_iter[update_index] = recv_message[receive_index*2+1];
        }
      }
    }
    jacobi_computation();
    obtain_publish_values();
  };
  
  void obtain_publish_values()
  {
    for(int i = 0 ; i < number_of_updated_components; i++)
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

  // Prints the full solution vector x.
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

}; 

template<typename... Args>
auto create_synchronous_jacobi(int machine_number, std::vector<std::vector<double>> A_partition, std::vector<double> b_partition, std::vector<int> row_indices,  Args&&... args) noexcept
{
  return create_synchronous_iterative(std::forward<Args>(args)...).then([=](std::optional<SynchronousIterative<std::vector<double>>> it) -> std::optional<SynchronousJacobi> {
     if (it) { return SynchronousJacobi(machine_number, A_partition, b_partition, row_indices, *it);}
     else    { return {}; }
  });
}

#endif 