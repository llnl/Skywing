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
#include <iomanip>

// #include "typeinfo"
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

// Terminal Diagnostic for exact solution.
void print_exact_solution(int machine_number, std::vector<double> x_sol_partition)
{
  std::cout << "\t The exact solution for " << machine_number << " is ";
  for(auto entry :  x_sol_partition)
  {
    std::cout << entry << " ";
  }
   std::cout << std::endl;
}

// All of the Skynet specific code is located in this function.
void machine_task(const int machine_number, int size_of_network, __attribute__((unused)) int trial, std::vector<double> matrix_row_hold, std::vector<double> b_values, __attribute__((unused))std::vector<double> x_sol_partition, std::vector<int> row_indices, std::vector<std::uint16_t> ports, std::vector<std::string> machine_names, std::vector<ValueTag> tags)
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

    double b_value = b_values[0];
    int row_index = row_indices[0];

    // For the synchronous_jacobi class to work, one needs to send the information for the computation, whichs is the row of the matrix used for computation, which is at this point the same as the machine_number.
    auto opt_iter_method = create_synchronous_jacobi(
      machine_number,
      size_of_network,
      row_index,
      matrix_row_hold,
      b_value,
      master_handle,
      job,
      tags[machine_number],
      tags
    ).get();

    auto sync_jaco = *opt_iter_method;
    sync_jaco.run();

    auto x_local_estimate = sync_jaco.return_full_x_iter();
    auto x_partition_estimate = sync_jaco.return_solution_as_vec();

    // double partial_residual = calculate_partial_residual(1, x_local_estimate, b_values, matrix_row_hold);
    // double partial_forward_error = calculate_partial_forward_error(1, x_partition_estimate, x_sol_partition);
    // collect_data_each_component(machine_number, 1, trial, partial_forward_error, partial_residual, iteration_count, run_time.count());

    // std::cout<<"machine " << machine_number << " has answer: " << sync_jaco.return_solution() << " compared to exact value: " << x_sol_partition[0] << std::endl;
    sync_jaco.print_solution();
    print_exact_solution(machine_number, x_sol_partition);

  });
  master.run();
}


int main(int argc, char* argv[])
{
  // Error checking for the number of arguments
  if (argc != 7)
  {
    std::cout << "Usage: Note Enough Arguments: " << argc << std::endl;
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
  //This creates the relevant vectors needed to interact with skynet.
  auto ports = set_port(starting_port_number, size_of_network);
  auto machine_names = obtain_machine_names(size_of_network);
  auto tags = obtain_tags<ValueTag>(size_of_network);

  // This collects the matrices and vectors for the function.
  std::string row_index_name= "machine_" + std::to_string(machine_number) + "_row_count_" + std::to_string(1)  + "_indices_" + matrix_name ;
  std::vector<int> row_indices = input_vector_from_matrix_market<int>(directory, row_index_name);

  std::string matrix_partition_name = "machine_" + std::to_string(machine_number) + "_row_count_" + std::to_string(1)  + "_" + matrix_name ;
  std::vector<double> matrix_row_hold = input_vector_from_matrix_market<double>(directory, matrix_partition_name);

  std::string rhs_partition_name = "machine_" + std::to_string(machine_number) + "_row_count_" + std::to_string(1)  + "_rhs_" + matrix_name ;
  std::vector<double> b_values = input_vector_from_matrix_market<double>(directory, rhs_partition_name);

  std::string x_sol_partition_name = "machine_" + std::to_string(machine_number) + "_row_count_" + std::to_string(1)  + "_x_sol_" + matrix_name ;
  std::vector<double> x_sol_partition = input_vector_from_matrix_market<double>(directory, x_sol_partition_name);
  
  // This is contrived since we know the solution.
  std::string x_sol_name =  "x_sol_" + matrix_name ;
  std::vector<double> x_full_solution = input_vector_from_matrix_market<double>("../../../examples/async_jacobi/system", x_sol_name);

  // if(machine_number == 0 )
  // {
  //   print_mat(matrix_rows_hold);
  //   print_vec(b_values);
  //   print_vec(x_sol_partition);
  //   // print_vec(x_full_solution);
  //   print_vec(row_indices);
  // }

  // Skynet code call.
  // This runs the actual skynet code.
  machine_task(machine_number, size_of_network, trial, matrix_row_hold, b_values, x_sol_partition, row_indices, ports, machine_names, tags);
  return 0;
}