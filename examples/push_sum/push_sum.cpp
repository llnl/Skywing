#include "skynet_core/skynet.hpp"
#include "skynet_core/master.hpp"
#include "skynet_upper/push_sum_processor.hpp"
#include "skynet_upper/data_input.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>

using namespace skynet;
using ValueTag = skynet::PublishTag<std::vector<double>>;

// First three functions are for the Skynet setup step.
std::vector<std::string> obtain_machine_names(std::uint16_t size_of_system)
{
  std::vector<std::string > machine_names;
  machine_names.resize(size_of_system);
  for(int i = 0 ; i < size_of_system; i++)
  {
    machine_names[i] = "node" + std::to_string(i+1);
  }
return machine_names;
}

std::vector<std::uint16_t>  set_port(std::uint16_t starting_port_number, std::uint16_t size_of_system)
{
  std::vector<std::uint16_t> ports;

  for(std::uint16_t i = 0; i < size_of_system; i++)
  {
    ports.push_back(starting_port_number + (i * 1));
  }
  return ports;
}

template <class TagType>
std::vector<TagType> obtain_tags(int size_of_system)
{
  std::vector<TagType> tags;
  for(int i = 0; i < size_of_system; i++)
  {
      std::string hold = "push_sum_tag" +  std::to_string(i);
      tags.push_back(TagType{hold});
  }
  return tags;
}

// For this example, the exact average can be computed by inputting the system size.
double obtain_exact_average(int size_of_system)
{
  double average = 0.0;
  for(int i = 0 ; i < size_of_system; i++)
  {
    average += 1.0*i + 1.0;
  }
  average /= size_of_system;
  return average;
}

void machine_task(int machine_number, int size_of_system, int number_of_neighbors, double starting_value, std::vector<std::uint16_t> ports, std::vector<std::string> machine_names, ValueTag pubTag, std::vector<ValueTag> subTags)
{
  skynet::Master master{ports[machine_number], machine_names[machine_number]};

  master.submit_job("job", [&](skynet::Job& job, MasterHandle master_handle){

  if (machine_number != static_cast<int>((ports.size()) - 1) )
  {
    // Connecting to the server is an asynchronous operation and can fail.
    // Wait for the result each time and keep attempting to connect until it does
    while (!master_handle.connect_to_server("127.0.0.1", ports[machine_number + 1]).get())
    {
      // Empty
    }
  }

  using IterMethod = AsynchronousIterative<PushSumProcessor<double>, UpdateIfPushSumShift<double>, StopAfterTime>;
  Waiter<IterMethod> iter_waiter =
    WaiterBuilder<IterMethod>(master_handle, job, pubTag, subTags)
    .set_processor(number_of_neighbors, starting_value, subTags)
    .set_publish_policy(1e-4)
    .set_stop_policy(std::chrono::seconds(5))
    .build_waiter();
  IterMethod push_sum = iter_waiter.get();

  push_sum.run(
      [&](const decltype(push_sum)& p)
      {
        std::cout << p.run_time().count() << "ms: Machine " << machine_number << " has value " <<
          p.get_processor().return_solution() << " and publications ";
        print_vec<double>(p.get_publication_values());
      } );

  double consensus_value = push_sum.get_processor().return_solution();
  double exact_solution = obtain_exact_average(size_of_system);
  double run_time_count = push_sum.run_time().count();
  double new_information_count = push_sum.get_processor().return_new_information_count();

  std::cout << "machine " << machine_number << "\tconsensus value: " << consensus_value << "\texact solution: " << exact_solution << "\truntime: " << run_time_count << "\tnew info count: " << new_information_count << std::endl;
  
  std::this_thread::sleep_for(std::chrono::seconds(10));
  });
  master.run();
}

int main(int argc, char* argv[])
{
  // Error checking for the number of arguments
  if (argc < 4)
  {
    std::cout << "Usage: Note Enough Arguments: " << argc << std::endl;
    return 1;
  }
  // Parse the machine number, starting_port_number, and size_of_system that was passed in
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
  if(machine_number > size_of_system - 1  || machine_number < 0)
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
  // Skynet setup
  auto ports = set_port(starting_port_number, size_of_system);
  auto machine_names = obtain_machine_names(size_of_system);
  auto subTags = obtain_tags<ValueTag>(size_of_system);
  // This pubTag is exists in subTags[machine_number] which is needed for initialization, but its declared separately here mainly to highlight how the creator works for the push_sum class.
  ValueTag pubTag("push_sum_tag" +  std::to_string(machine_number)) ;
  // Push sum variables -> initialized by user
  double starting_value = (machine_number+1)*1.0;
  int number_of_neighbors = size_of_system - 1;

  // Skynet job
  machine_task(machine_number, size_of_system, number_of_neighbors, starting_value, ports, machine_names, pubTag, subTags);
  return 0;
}
