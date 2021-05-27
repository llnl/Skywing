//This goal of this example is to solve a simple 4x4 linear system using synchronous jacobi.
// The exact linear system will be declared as a global variables, so each instance knows the full system, and the iterates can be tracked.
// This example uses 4 threads to solve this system, so each thread solves a single component.


#include "skynet_core/skynet.hpp"
#include "skynet_upper/asynchronous_jacobi.hpp"
#include "skynet_core/master.hpp"
// #include "local_consolidation.hpp"


// #include "utils.hpp"
// #include "typeinfo"
#include "../jacobi_include/linear_system_setup/skynet_jacobi_setup.hpp"
#include "../jacobi_include/linear_system_setup/input_system_from_matrix_market_v2.hpp"
// #include "../include/data_collection/local_consolidation.hpp"
#include "../jacobi_include/data_collection/save_async_sync_jacobi_data.hpp"
#include "../jacobi_include/data_collection/jacobi_error_residual_functions.hpp"
#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <thread>
#include <cstdint>
#include <filesystem>
#include <fstream>
using namespace skynet;

using ValueTag = skynet::PublishTag<std::vector<double>>;
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
void machine_task(int machine_number, int number_of_updated_components, int trial, std::vector<std::vector<double>> matrix_rows, std::vector<double> b_values, std::vector<double> x_local_solution, std::vector<int> row_indices, std::vector<std::uint16_t> ports, std::vector<std::string> machine_names, std::vector<ValueTag> tags)
{

  skynet::Master master{ports[machine_number], machine_names[machine_number]};

  master.submit_job("job", [&](skynet::Job& job, MasterHandle master_handle) {

  //So this only affects macines 0,1,2
  if (machine_number != static_cast<int>((ports.size()) - 1) )
  {
    // Connecting to the server is an asynchronous operation and can fail.
    // Wait for the result each time and keep attempting to connect until it does
    while (!master_handle.connect_to_server("127.0.0.1", ports[machine_number + 1]).get())
    {
      // Empty
    }
  }
  std::cout << "after network connection: " << machine_number << std::endl;

  // This is the user defined stopping criteria.
  // For the sake of simplicity this is defined as max iterations outside of the header filoe, atm.
  int iteration_count = 0 ;
  int max_itr = 20;

  std::cout << "starting to iterate: " << machine_number << std::endl;

  // For the synchronous_jacobi class to work, one needs to send the information for the computation, whichs is the row of the matrix used for computation, which is at this point the same as the machine_number.
  auto opt_iter_method = create_asynchronous_jacobi(
    machine_number,
    number_of_updated_components,
    matrix_rows,
    b_values,
    row_indices,
    tags,
    master_handle,
    job,
    tags[machine_number],
    tags
  ).get();

  std::cout << "starting to iterate: " << machine_number << std::endl;
  auto async_jaco = *opt_iter_method;

  auto start_jacobi = std::chrono::high_resolution_clock::now();
  while(iteration_count <= max_itr)
  {
    iteration_count++;
    async_jaco.create_iteration(iteration_count);
  }

  auto stop_jacobi = std::chrono::high_resolution_clock::now();
  auto run_time = std::chrono::duration_cast<std::chrono::microseconds>(stop_jacobi - start_jacobi);

  std::cout << "after iteration: " << machine_number << std::endl;

  auto x_local_estimate = async_jaco.return_full_x_iter();
  auto x_partition_estimate = async_jaco.return_solution();

  double partial_residual = calculate_partial_residual(number_of_updated_components, x_local_estimate, b_values, matrix_rows);
  double partial_forward_error = calculate_partial_forward_error(number_of_updated_components, x_partition_estimate, x_local_solution);

  collect_data_each_component(machine_number, number_of_updated_components, trial, partial_forward_error, partial_residual, iteration_count, run_time.count());


  std::this_thread::sleep_for(std::chrono::milliseconds{100});

  async_jaco.print_solution();
  print_exact_solution(machine_number,number_of_updated_components,  x_local_solution);

  // std::this_thread::sleep_for(std::chrono::milliseconds{100});
  // std::cout << "This is at the end of master.run() before return 0 for " << machine_number << std::endl;
  });
  master.run();
}


int main(int argc, char* argv[])
{
  // Error checking for the number of arguments
  if (argc < 6)
  {
    std::cout << "Usage: Note Enough Arguments: " << argc << std::endl;
    // return 1;
  }
  // Terminal output to see command line arguments.
  // std::cout<< "Command Line Arguments: ";
  // for(int i =1 ; i< argc; i++)
  // {
  //   std::cout << argv[i] << " ";
  // }
  // std::cout << std::endl;

  // Parse the machine number, starting_port_number, and size_of_system that was passed in
  // Do this in a lambda so that if there's an exception a dummy value can be
  // returned which will always trigger an error
  // Machine number is the
    int machine_number = [&]() {
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
   int size_of_system = [&]() {
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
  int number_of_updated_components = std::stoi(argv[5]);
  int trial = std::stoi(argv[6]);

  //This creates the relevent vectors needed to interact with skynet.
  auto ports = set_port(starting_port_number, size_of_system);
  auto machine_names = obtain_machine_names(size_of_system);
  auto tags = obtain_tags<ValueTag>(size_of_system);

  std::vector<int> row_indices;
  for(int i = 0 ; i < number_of_updated_components; i++)
  {
    row_indices.push_back((machine_number + i) % size_of_system);
  }
  auto matrix_rows_hold = obtain_A_matrix(machine_number, size_of_system, row_indices, matrix_name);

  auto b_values = obtain_rhs_vector(machine_number, size_of_system, row_indices, matrix_name);

  auto x_local_solution = obtain_local_solution_vector(machine_number, size_of_system, row_indices, matrix_name);

  auto x_full_solution = obtain_full_solution_vector(machine_number, size_of_system, matrix_name);

  std::cout << "here after setup: " << machine_number << std::endl;

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
  // std::cout << "Setup Sucessful: " << machine_number  << std::endl;
  // std::filesystem::path p = std::filesystem::current_path();
  //
  //     std::cout << "The current path " << p << " decomposes into:\n"
  //               << "root-path " << p.root_path() << '\n'
  //               << "relative path " << p.relative_path() << '\n';
  // This runs the actual skynet code.
  std::cout << "before machine task: " << machine_number << std::endl;
  machine_task(machine_number, number_of_updated_components, trial, matrix_rows_hold, b_values, x_local_solution, row_indices, ports, machine_names, tags);

  return 0;
}
