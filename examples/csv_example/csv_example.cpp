//This goal of this example is to solve a simple 4x4 linear system using synchronous jacobi.
// The exact linear system will be declared as a global variables, so each instance knows the full system, and the iterates can be tracked.
// This example uses 4 threads to solve this system, so each thread solves a single component.


#include "skynet_core/skynet.hpp"
#include "skynet_upper/asynchronous_jacobi.hpp"
#include "skynet_core/master.hpp"


// #include "utils.hpp"

// #include "typeinfo"
#include "../include/linear_system_setup/skynet_jacobi_setup.hpp"
#include "../include/linear_system_setup/input_system_from_matrix_market.hpp"
// #include "../include/data_collection/local_consolidation.hpp"
#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <thread>
#include <cstdint>
#include <filesystem>
using namespace skynet;

using ValueTag = skynet::PublishTag<std::vector<double>>;


int main(int argc, char* argv[])
{
  if (argc < 7)
  {
    std::cerr << "Usage: Note Enough Arguments: " << argc << std::endl;
    return 1;
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
  std::string matrix_file_name = argv[4];
  std::string rhs_file_name = argv[5];
  std::string sol_file_name = argv[6];
  int number_of_updated_components = std::stoi(argv[7]);
  std::string root_pathway = argv[8];

  // This creates the relevent vectors needed to interact with skynet.
  // In this example, these are useless.
  auto ports = set_port(starting_port_number, size_of_system);
  auto machine_names = obtain_machine_names(size_of_system);
  auto tags = obtain_tags<ValueTag>(size_of_system);


  std::string my_file_name_local_forward_error="LocalIterateForwardErrorInformation" + std::to_string(machine_number) + ".csv";
  std::ofstream my_file_local_forward_error;
  my_file_local_forward_error.open(my_file_name_local_forward_error);
  std::cout << "This is my file name: " << my_file_name_local_forward_error << std::endl;
  if(my_file_local_forward_error.is_open()==false)
  {
    std::cout<<"This file is not open: "<<my_file_name_local_forward_error<<"."<<std::endl;
    assert(my_file_local_forward_error.is_open()==true);
  }
  my_file_local_forward_error << "My rank ";
  my_file_local_forward_error << ",";
  my_file_local_forward_error << machine_number;
  my_file_local_forward_error << "\n";
  my_file_local_forward_error << "Number of updated components ";
  my_file_local_forward_error << ",";
  my_file_local_forward_error << number_of_updated_components;
  my_file_local_forward_error << "\n";
  my_file_local_forward_error.close();


  return 0;
}
