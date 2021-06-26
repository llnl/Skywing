#include "skynet_core/skynet.hpp"
#include "skynet_core/master.hpp"
// #include "skynet_upper/push_sum_ps.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>
// #include <random>
#include <thread>
// #include "typeinfo"

using namespace skynet;
using ValueTag = skynet::PublishTag<std::vector<double>>;

static constexpr std::chrono::seconds SUBSCRIBE_TIMEOUT =
    std::chrono::seconds(10);

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

std::vector<ValueTag> obtain_sub_tags(int machine_number, std::uint16_t size_of_system)
{
  std::vector<ValueTag> tags;
  for(int i = 0; i < size_of_system; i++)
  {
    if(i!= machine_number)
    {
      std::string hold = "tag" +  std::to_string(i);
      tags.emplace_back(hold);
    }
  }
  return tags;
}


template<typename T>
static void subscribeToTag(skynet::Job& job, const T& tag)
{
  auto waiter = job.subscribe(tag);
  if (!waiter.wait_for(SUBSCRIBE_TIMEOUT))
  {
    std::cerr << "Could not subscribe to tag " << tag.id() << std::endl;
    std::exit(-1);
  }
}

std::vector<double> obtain_publish_values(double sigma_x, double sigma_y, double new_information_count)
  {
    std::vector<double> publish_values;
    publish_values.push_back(sigma_x);
    publish_values.push_back(sigma_y);
    publish_values.push_back(new_information_count);
    return publish_values;
  }

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

void machine_task(int machine_number, int size_of_system, int number_of_neighbors, double starting_value, std::vector<std::uint16_t> ports, std::vector<std::string> machine_names, ValueTag push_sum_pub, std::vector<ValueTag> push_sum_subs)
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

  // This is the user defined stopping criteria.
  int iteration_count = 0 ;
  int max_itr = 10;
  double new_information_count = 0.0;
  double max_neighbor_received = 0.0;
  double x_value = starting_value;
  double y_value = 1.0;
  double sigma_x = 0.0;
  double sigma_y = 0.0;
  std::vector<double> rho_x(size_of_system, 0.0);
  std::vector<double> rho_y(size_of_system, 0.0);
  std::vector<double> rho_x_previous(size_of_system, 0.0);
  std::vector<double> rho_y_previous(size_of_system, 0.0);
  double in_nodes_plus_one = number_of_neighbors + 1.0;
  // Local weights.
  sigma_x = sigma_x + (x_value / in_nodes_plus_one);
  sigma_y = sigma_y + (y_value / in_nodes_plus_one);
  std::vector<double> publish_values = obtain_publish_values(sigma_x, sigma_y, new_information_count);
  job.declare_publication_intent(push_sum_pub);
  job.publish(push_sum_subs, publish_values);
  for (int i = 0; i < size_of_system; i++)
  {
    if (i != machine_number)
    {
      subscribeToTag(job, push_sum_subs[i]);      
    }
  }

  auto start_push_sum = std::chrono::high_resolution_clock::now();
  while(iteration_count <= max_itr)
  {
    iteration_count++;
    for (int i = 0; i < size_of_system; i++)
    {
      if (i < machine_number)
      {
        if(job.has_data[push_sum_subs[i]])
        {
          new_information_count += 1.0;
          std::vector<double> received_values = *job.get_waiter(push_sum_subs[i]).get();
          new_information_count +=1.0;
          rho_x_previous[i] = rho_x[i];
          rho_y_previous[i] = rho_y[i];
          rho_x[i] = received_values[0];
          rho_y[i] = received_values[1];

          x_value = x_value + rho_x[i] - rho_x_previous[i];
          y_value = y_value + rho_y[i] - rho_y_previous[i];

          // This is the 'wake up' portion followed by broadcast.
          sigma_x = sigma_x + (x_value / in_nodes_plus_one);
          sigma_y = sigma_y + (y_value / in_nodes_plus_one);

          x_value = x_value / in_nodes_plus_one;
          y_value = y_value / in_nodes_plus_one;

          // Checks the max neighbor iterations for terminal checks.
          if (received_values[2]>max_neighbor_received)
          {
            max_neighbor_received = received_values[2];
          }
          publish_values = obtain_publish_values(sigma_x, sigma_y, new_information_count);
          job.publish(tags_to_publish, publish_values);
        }
      }
      else if (i > machine_number)
      {
        if(job.has_data[push_sum_subs[i-1]])
        {
          new_information_count += 1.0;
          std::vector<double> received_values = *job.get_waiter(push_sum_subs[i-1]).get();
          new_information_count +=1.0;
          rho_x_previous[i] = rho_x[i];
          rho_y_previous[i] = rho_y[i];
          rho_x[i] = received_values[0];
          rho_y[i] = received_values[1];

          x_value = x_value + rho_x[i] - rho_x_previous[i];
          y_value = y_value + rho_y[i] - rho_y_previous[i];

          // This is the 'wake up' portion followed by broadcast.
          sigma_x = sigma_x + (x_value / in_nodes_plus_one);
          sigma_y = sigma_y + (y_value / in_nodes_plus_one);

          x_value = x_value / in_nodes_plus_one;
          y_value = y_value / in_nodes_plus_one;

          // Checks the max neighbor iterations for terminal checks.
          if (received_values[2]>max_neighbor_received)
          {
            max_neighbor_received = received_values[2];
          }
          publish_values = obtain_publish_values(sigma_x, sigma_y, new_information_count);
          job.publish(tags_to_publish, publish_values);
        }
      }
  }

  }
  auto stop_push_sum = std::chrono::high_resolution_clock::now();
  auto run_time = std::chrono::duration_cast<std::chrono::microseconds>(stop_push_sum - start_push_sum);


  double consensus_value = x_value/y_value;
  double exact_solution = obtain_exact_average(size_of_system);
  double run_time_count = run_time.count();

  std::cout << "machine_number: " << machine_number << "\tconsensus value: " << consensus_value << "\texact solution: " << exact_solution <<  "\truntime: " << run_time_count << "\tnew_information_count: " << new_information_count << std::endl;
  // This block is for computing the information from each process for each experiment for each trial.
  // collect_data(machine_number, consensus_value, exact_solution, iteration_count, run_time.count());
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
  // Skynet setup
  auto ports = set_port(starting_port_number, size_of_system);
  auto machine_names = obtain_machine_names(size_of_system);
  auto push_sum_subs = obtain_sub_tags(machine_number, size_of_system);
  ValueTag push_sum_pub("tag"+std::to_string(machine_number));
  // Push vars
  double starting_value = (machine_number+1)*1.0; 
  int number_of_neighbors = size_of_system - 1;
  // This makes sure that the machine number and size_of_system is valid
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
  // Skynet job
  machine_task(machine_number, size_of_system, number_of_neighbors, starting_value, ports, machine_names, push_sum_pub, push_sum_subs);
  return 0;
}
// std::this_thread::sleep_for(std::chrono::milliseconds{100});