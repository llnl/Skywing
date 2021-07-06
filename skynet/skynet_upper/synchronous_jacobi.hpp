#ifndef SKYNET_UPPER_SYNCHRONOUS_JACOBI_HPP
#define SKYNET_UPPER_SYNCHRONOUS_JACOBI_HPP

#include "skynet_core/job.hpp"
#include "skynet_core/master.hpp"
#include "skynet_upper/synchronous_iterative.hpp"
#include "skynet_upper/stopping_criterion.hpp"

// This class solves the square linear system Ax=b.
// This assumes that the system is square, consistent, and digonally dominant and thus solvable by this method.
// This class stores the full solution vector x_iter at each machine, and the specific update that each machine solves for can be viewed by the user by calling the print_solution() function or output by the return_solution() function or return_full_solution() for either the specific update the machine solved for or the full vector respectively.

using namespace skynet;
using ValueTag = skynet::PublishTag<std::vector<double>>;

// This is if we want to template this class.
// Kendall doesn't recommend this since we would then have to deal with the case where a user inputs tuples.
// This isn't something she recommended doing, at least for the moment.
// Since the only feasible types for the linear system are doubles or floats, this didn't seem to be a pressing issue.
// The main crux of this example is the constructor function below.
//

// template<typename ... TagValueTypes>
class SynchronousJacobi
{

private:

  int machine_number;
  int size_of_network;
  int row_index;
  std::vector<double> matrix_row; 
  double b_partition;
  // std::vector<ValueOrTuple<TagValueTypes...>> matrix_row;
  // Variables internal to this class.
  std::vector<double> x_iter;
  std::vector<double> publish_values;
  int new_information_count = 0;
  int max_new_information;
  std::chrono::duration<double> run_time = std::chrono::milliseconds(1);
  std::chrono::duration<double> max_run_time;
  bool iterate = true;


  // Variables used for SynchronousIterative class and communication.
  // std::array<ValueTag, 4> tags;
  // Skynet synchronous iterative variables
  friend class SynchronousIterative<std::vector<double>>;
  SynchronousIterative<std::vector<double>> iter_method ;

public:

  SynchronousJacobi(int machine_number, int size_of_network, int row_index, std::vector<double> matrix_row, double b_partition, SynchronousIterative<std::vector<double>> it): 
  machine_number(machine_number), 
  size_of_network(size_of_network),
  row_index(row_index),
  matrix_row(matrix_row),
  b_partition(b_partition), 
  iter_method(it)
  {
    x_iter.resize(matrix_row.size(), 0.0);
    publish_values.resize(2,0.0);
    max_new_information = 20 * size_of_network;
    max_run_time = std::chrono::milliseconds(1000) *size_of_network;
    // Initial computation and send message.
    x_iter[row_index] = jacobi();
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
      // This allows for quick diagnostics by seeing what's in the buffers rho_x, rho_y,  at terminal.
      // print_current_information();
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
      new_information_count++;
      int update_index = (int) recv_message[0];
      x_iter[update_index] = recv_message[1];
    }
    x_iter[row_index] = jacobi();
    obtain_publish_values();
  };
  
  void obtain_publish_values()
  {
    publish_values[0] = row_index*1.0;
    publish_values[1] = x_iter[row_index];
  }
  
  double jacobi()
  {
    double hold = 0.0;
    for(std::vector<double>::size_type i = 0 ; i < matrix_row.size(); i++)
    {
      if((int)i!= row_index)
      {
        hold = hold + matrix_row[i] * x_iter[i];
      }
    }
    hold  = (b_partition  - hold)/matrix_row[row_index] ;
    return hold;
  };

  // Functions to return solution vector or individual solution.
  double return_solution()
  {
    return x_iter[row_index];
  };

  std::vector<double> return_solution_as_vec()
  {
    std::vector<double> return_vec(1,x_iter[row_index]);
    return return_vec;
  };

  std::vector<double> return_full_x_iter()
  {
    return x_iter;
  }

  // Functions to print solution vector or individual solution.
  void print_solution()
  {
    std::cout << "\t machine " << machine_number << "\t solution: " << x_iter[row_index] << std::endl;
  };

  void print_x_iter()
  {
    std::cout << "The full x_iter vector is: ";
    for(std::vector<double>::size_type i = 0 ; i < x_iter.size(); i++)
    {
      std::cout << x_iter[i] << " ";
    }
    std::cout << std::endl;
  };


}; // end synchronous_jacobi

// int machine_number, int row_index, std::vector<double> matrix_row, double b_partition, SynchronousIterative<double> it)
template<typename... Args>
auto create_synchronous_jacobi(int machine_number, int size_of_system, int row_index, std::vector<double> matrix_row, double b_partition, Args&&... args) noexcept
{
  return create_synchronous_iterative(std::forward<Args>(args)...).then([=](std::optional<SynchronousIterative<std::vector<double>>> it) -> std::optional<SynchronousJacobi> {
     if (it) { return SynchronousJacobi(machine_number, size_of_system, row_index, matrix_row, b_partition, *it);}
     else    { return {}; }
  });
}

#endif // SKYNET_UPPER_SYNCHRONOUS_JACOBI_HPP