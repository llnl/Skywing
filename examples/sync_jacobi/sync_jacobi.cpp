#include "skynet_core/skynet.hpp"
#include "skynet_core/master.hpp"
#include "skynet_upper/synchronous_jacobi.hpp"
#include "skynet_upper/data_input.hpp"

#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <thread>
#include <cstdint>
#include <fstream>

// all jacobi_include files for matrix input and data aggregation.
#include "jacobi_data_output.hpp"

using namespace skynet;
using ValueTag = skynet::PublishTag<std::vector<double>>;

// First three functions are for the Skynet setup step.
std::vector<std::string> obtain_machine_names(std::uint16_t size_of_network)
{
  std::vector<std::string > machine_names;
  machine_names.resize(size_of_network);
  for(int i = 0 ; i < size_of_network; i++)
  {
    machine_names[i] = "node" + std::to_string(i+1);
  }
return machine_names;
}

std::vector<std::uint16_t>  set_port(std::uint16_t starting_port_number, std::uint16_t size_of_network)
{
  std::vector<std::uint16_t> ports;

  for(std::uint16_t i = 0; i < size_of_network; i++)
  {
    ports.push_back(starting_port_number + (i * 1));
  }
  return ports;
}

template <class TagType>
std::vector<TagType> obtain_tags(std::uint16_t size_of_network)
{
  std::vector<TagType> tags;
  for(int i = 0; i < size_of_network; i++)
  {
    std::string hold = "tag" +  std::to_string(i);
    tags.push_back(TagType{hold});
  }
  return tags;
}

// All of the Skynet specific code is located in this function.
void machine_task(const int machine_number, int trial, std::vector<std::vector<double>> A_partition, std::vector<double> b_partition, std::vector<double> x_partition_solution, std::vector<double> x_full_solution, std::vector<size_t> row_indices, std::vector<std::uint16_t> ports, std::vector<std::string> machine_names, std::vector<ValueTag> tags, std::string save_directory)
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

    auto opt_iter_method = create_synchronous_jacobi(
      machine_number,
      A_partition,
      b_partition,
      row_indices,
      master_handle,
      job,
      tags[machine_number],
      tags
    ).get();

    auto sync_jacobi = *opt_iter_method;
    sync_jacobi.run();


  double run_time = sync_jacobi.return_runtime();
  int information_received = sync_jacobi.return_information_received();
  auto x_local_estimate = sync_jacobi.return_full_solution();
  auto x_partition_estimate = sync_jacobi.return_partition_solution();
  // Since this is a distributed algorithm, we only have access to information that allows us to have a "partial" residual, since not every agent has every row of the matrix. 
  // In contrast, we can look at error involving only the components of the solution vector x which this process updates, or it's entire estimation vector, hence "partial" versus "full" in this language and "PSQ" versus "FSQ" for "partial error squared" and "full error squared". 
  // We avoid taking square roots here in case additional post processing is wanted.
  double partial_residual = calculate_partial_residual(x_local_estimate, b_partition, A_partition);
  double partial_forward_error = calculate_partial_forward_error(row_indices, x_partition_estimate, x_partition_solution);
  double forward_error = calculate_local_forward_error(x_local_estimate, x_full_solution);

  // Saves information from each Skynet machine for post processing, if wanted.
  collect_data_each_component(machine_number, 1, trial, partial_forward_error, partial_residual, information_received, run_time, save_directory);


  std::cout << "Machine: " << machine_number << "\tNumber of Updated Components: " << row_indices.size(); 
  std::cout << std::endl;
  if(static_cast<int>(row_indices.size()) < 10)
  {
    std::cout << "\t Estimate: \t"; 
    print_vec<double>(x_partition_estimate);
    std::cout << "\t Exact Sol: \t"; 
    print_vec<double>(x_partition_solution);
  }
  std::cout << "\t FSQ Error: \t" << forward_error;
  std::cout << std::endl;
  std::cout << "\t PSQ Error: \t" << partial_forward_error;
  std::cout << std::endl;
  std::cout << "\t PSQ Residual: \t" << partial_residual;  
  std::cout << std::endl;      
  std::cout << "\t New Info: \t" << information_received ; 
  std::cout << std::endl;
  std::cout << "\t Runtime: \t" << run_time; 
  std::cout << std::endl;      
  std::cout << std::endl;
  std::cout << "\t Iterate: \t" << sync_jacobi.return_iterate() ; 
  std::cout << std::endl;
  std::cout << "--------------------------------------------" << std::endl;
  });
  master.run();
}


int main(int argc, char* argv[])
{
  // Error checking for the number of arguments
  if (argc != 8)
  {
    std::cout << "Usage: Wrong Number of Arguments: " << argc << std::endl;
    return 1;
  }

  // Parse the machine number, starting_port_number, and size_of_network that was passed in
  // Do this in a lambda so that if there's an exception a dummy value can be
  // returned which will always trigger an error
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
   int size_of_network = [&]() {
    try
    {
      return std::stoi(argv[3]);
    }
    catch (...)
    {
      return -1;
    }
  }();
  std::string matrix_name = [&]() {
    try
    {
      return argv[4];
    }
    catch (...)
    {
      char *hold ;
      return hold;
    }
  }();
  if (machine_number < 0 || machine_number >= size_of_network)
  {
    std::cerr
      << "Invalid machine_number of " << std::quoted(argv[1]) << ".\n"
      << "Must be an integer between 0 and " << size_of_network - 1 << '\n';
    return -1;
  }
  if (size_of_network <= 0)
  {
    std::cerr
      << "Invalid size_of_network of " << std::quoted(argv[3]) << ".\n"
      << "Must be an integer greater than 0 and  match the number of threads created. \n";
    return -1;
  }
  if(matrix_name == "")
  {
    std::cerr
      << "Linear system not specified. " << std::quoted(argv[4]) << ".\n"; 
    return -1; 
  }

  std::string directory = argv[5];
  int trial = std::stoi(argv[6]);
  std::string save_directory = argv[7];
  //This creates the relevant vectors needed to interact with skynet.
  auto ports = set_port(starting_port_number, size_of_network);
  auto machine_names = obtain_machine_names(size_of_network);
  auto tags = obtain_tags<ValueTag>(size_of_network);

  // This collects the matrices and vectors for the function.
  std::string row_index_name= "machine_" + std::to_string(machine_number) + "_row_count_" + std::to_string(0)  + "_indices_" + matrix_name ;
  std::vector<size_t> row_indices = input_vector_from_matrix_market<size_t>(directory, row_index_name);

  std::string matrix_partition_name = "machine_" + std::to_string(machine_number) + "_row_count_" + std::to_string(0)  + "_" + matrix_name ;
  // std::vector<double> matrix_row_hold = input_vector_from_matrix_market<double>(directory, matrix_partition_name);
  std::vector<std::vector<double>> A_partition = input_matrix_from_matrix_market<double>(directory, matrix_partition_name);

  std::string rhs_partition_name = "machine_" + std::to_string(machine_number) + "_row_count_" + std::to_string(0)  + "_rhs_" + matrix_name ;
  std::vector<double> b_partition = input_vector_from_matrix_market<double>(directory, rhs_partition_name);

  std::string x_sol_partition_name = "machine_" + std::to_string(machine_number) + "_row_count_" + std::to_string(0)  + "_x_sol_" + matrix_name ;
  std::vector<double> x_partition_solution = input_vector_from_matrix_market<double>(directory, x_sol_partition_name);
  
  // This is contrived since we know the solution.
  // These are not used in the jacobi class, only for data output and terminal diagnostics in the example.
  std::string x_sol_name =  "x_sol_" + matrix_name ;
  std::vector<double> x_full_solution = input_vector_from_matrix_market<double>("../../../examples/sync_jacobi/system", x_sol_name);

  // Skynet call
  machine_task(machine_number, trial, A_partition, b_partition, x_partition_solution, x_full_solution, row_indices, ports, machine_names, tags, save_directory);
  return 0;
}
