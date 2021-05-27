//This goal of this example is to solve a simple 4x4 linear system using synchronous jacobi.
// The exact linear system will be declared as a global variables, so each instance knows the full system, and the iterates can be tracked.
// This example uses 4 threads to solve this system, so each thread solves a single component.


#include "skynet_core/skynet.hpp"
#include "skynet_upper/synchronous_jacobi.hpp"
#include "skynet_core/master.hpp"

// #include "typeinfo"
#include "../jacobi_include/linear_system_setup/skynet_jacobi_setup.hpp"
#include "../jacobi_include/linear_system_setup/input_system_from_matrix_market_v2.hpp"
#include "../jacobi_include/data_collection/save_async_sync_jacobi_data.hpp"
#include "../jacobi_include/data_collection/jacobi_error_residual_functions.hpp"

#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <thread>
#include <cstdint>
#include <fstream>
#include <iomanip>

using namespace skynet;

using ValueTag = skynet::PublishTag<double>;


// //The answer x to Ax=b, A and b as defined above.
// const std::vector<double> ans{
//   1.0, 2.0, 3.0, 4.0
// };

void print_exact_solution(int machine_number, int number_of_updated_components, std::vector<double> x_local_solution)
{
  std::cout << "The exact solution for " << machine_number << " is ";
  for(int i = 0 ; i < number_of_updated_components; i ++)
  {
    std::cout << x_local_solution[i] << " ";
  }
   std::cout << std::endl;
}

// All of the Skynet specific code is located in this function.
void machine_task(const int machine_number, int trial, std::vector<std::uint16_t> ports, std::vector<std::string> machine_names, std::vector<ValueTag> tags, std::vector<std::vector<double>> matrix_row_hold, std::vector<double> b_values, std::vector<double> x_local_solution)
{

  skynet::Master master{ports[machine_number], machine_names[machine_number]};

  master.submit_job("job", [&](skynet::Job& job, MasterHandle master_handle) {

  if (machine_number != (static_cast<int>(ports.size()) - 1))
  {
    // Connecting to the server is an asynchronous operation and can fail.
    // Wait for the result each time and keep attempting to connect until it does
    while (!master_handle.connect_to_server("127.0.0.1", ports[machine_number + 1]).get())
    {
      // Empty
    }
  }

    std::vector<double> matrix_row = matrix_row_hold[0];
    double b_value = b_values[0];

    // This is the user defined stopping criteria.
    // For the sake of simplicity this is defined as max iterations outside of the header filoe, atm.
    int iteration_count = 0 ;
    int max_itr = 50;

    // For the synchronous_jacobi class to work, one needs to send the information for the computation, whichs is the row of the matrix used for computation, which is at this point the same as the machine_number.
    auto opt_iter_method = create_synchronous_jacobi(
      machine_number,
      matrix_row,
      b_value,
      master_handle,
      job,
      tags[machine_number],
      tags
    ).get();

    auto sync_jaco = *opt_iter_method;

    auto start_jacobi = std::chrono::high_resolution_clock::now();
    while(iteration_count <= max_itr)
    {
      // std::cout << "in iteration" << std::endl;
      iteration_count++;
      sync_jaco.create_iteration();
    }

    auto stop_jacobi = std::chrono::high_resolution_clock::now();
    auto run_time = std::chrono::duration_cast<std::chrono::microseconds>(stop_jacobi - start_jacobi);

    auto x_local_estimate = sync_jaco.return_full_x_iter();
    auto x_partition_estimate = sync_jaco.return_solution_as_vec();



    double partial_residual = calculate_partial_residual(1, x_local_estimate, b_values, matrix_row_hold);
    double partial_forward_error = calculate_partial_forward_error(1, x_partition_estimate, x_local_solution);

    collect_data_each_component(machine_number, 1, trial, partial_forward_error, partial_residual, iteration_count, run_time.count());


    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    std::cout<<"machine " << machine_number << " has answer: " << sync_jaco.return_solution() << " compared to exact value: " << x_local_solution[0] << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds{100});

  });
  master.run();
}


int main(int argc, char* argv[])
{
  // Error checking for the number of arguments
  if (argc < 5)
  {
    std::cout<< "Usage: Note Enough Arguments: " << argc << std::endl;
    // return 1;
  }

  // Parse the machine number, starting_port_number, and size_of_system that was passed in
  // Do this in a lambda so that if there's an exception a dummy value can be
  // returned which will always trigger an error
  // Machine number is the
   const int machine_number = [&]() {
    try
    {
      return std::stoi(argv[1]);
    }
    catch (...)
    {
      return -1;
    }
  }();
  const std::uint16_t starting_port_number = [&]() {
    try
    {
      return std::stoi(argv[2]);
    }
    catch (...)
    {
      return -1;
    }
  }();
  const std::uint16_t size_of_system = [&]() {
    try
    {
      return std::stoi(argv[3]);
    }
    catch (...)
    {
      return -1;
    }
  }();
  // std::string matrix_file_name = [&]() {
  //   try
  //   {
  //     return argv[4];
  //   }
  //   catch (...)
  //   {
  //     std::string hold = "";
  //     return " ";
  //   }
  // }();
  //
  // std::string rhs_file_name = [&]() {
  //   try
  //   {
  //     return argv[5];
  //   }
  //   catch (...)
  //   {
  //     std::string hold = "";
  //     return &hold;
  //   }
  // }();
  std::string matrix_name = argv[4];
  // std::string rhs_file_name = argv[5];
  // std::string sol_file_name = argv[6];
  // std::string root_pathway = argv[7];
  // int lc = std::stoi(argv[8]);
  int trial = std::stoi(argv[5]);
  // For using the asynchronous data aggregation.
  // int number_of_updated_components = 1;

  //This creates the relevent vectors needed to interact with skynet.
  auto ports = set_port(starting_port_number, size_of_system);
  auto machine_names = obtain_machine_names(size_of_system);
  auto tags = obtain_tags<ValueTag>(size_of_system);

  std::vector<int> row_indices = {machine_number};

  auto matrix_row_hold = obtain_A_matrix(machine_number, size_of_system, row_indices, matrix_name);

  auto b_values = obtain_rhs_vector(machine_number, size_of_system, row_indices, matrix_name);

  auto x_local_solution = obtain_local_solution_vector(machine_number, size_of_system, row_indices, matrix_name);

  // This makes sure that the machine number and size_of_system is valid, and the dimension of the distributed b vector and matrix A match, outputting an error message if not.
  if (machine_number < 0 || machine_number >= static_cast<int>(ports.size()))
  {
    std::cerr
      << "Invalid machine_number of " << std::quoted(argv[1]) << ".\n"
      << "Must be an integer between 0 and " << size_of_system - 1 << '\n';
    return -1;
  }
  if (size_of_system <= 0)
  {
    std::cerr
      << "Invalid size_of_system of " << std::quoted(argv[1]) << ".\n"
      << "Must be an integer greater than 0 and  match the number of threads created. \n";
    return -1;
  }
  // if (static_cast<int>(b_values.size()) != static_cast<int>(matrix_row_hold.size()))
  // {
  //   std::cerr
  //     << "Invalid dimension size.\n";
  //   return -1;
  // }
  std::cout << "After setup for: " << machine_number << std::endl;
  // This runs the actual skynet code.
  machine_task(machine_number, trial, ports, machine_names, tags, matrix_row_hold, b_values, x_local_solution);

  return 0;
}
