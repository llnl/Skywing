#ifndef SKYNET_UPPER_PUSH_SUM_HPP
#define SKYNET_UPPER_PUSH_SUM_HPP

#include "skynet_core/job.hpp"
#include "skynet_core/master.hpp"
#include "skynet_upper/asynchronous_iterative.hpp"
#include "skynet_upper/stopping_criterion.hpp"

// Convenient alias for passing doubles between machines.
using namespace skynet;
using ValueTag = skynet::PublishTag<std::vector<double>>;

class PushSum
{

private:

  friend class AsynchronousIterative<std::vector<double>>;

  int machine_number;
  int size_of_system;
  int number_of_neighbors;
  double new_information_count = 0.0;
  double max_neighbor_received = 0.0;
  double x_value;
  double y_value = 1.0;
  // this is the number of neighbors plus one needed for the update rule
  double in_nodes_plus_one;
   // Local weights.
  double sigma_x;
  double sigma_y;
  // stores iterate information
  std::vector<double> rho_x;
  std::vector<double> rho_y;
  std::vector<double> rho_x_previous;
  std::vector<double> rho_y_previous;
  // Stopping Criterion values
  // Note: These are pretty much fixed at the moment, and increase linearly with respect to network size, so these likely won't work if the network is too large.
  std::chrono::duration<double> max_run_time;
  double max_new_information;
  bool iterate = true;
  std::chrono::duration<double> run_time = std::chrono::milliseconds(1);
  // Skynet specific variables
  ValueTag myTag;
  std::vector<ValueTag> tags_vector;
  AsynchronousIterative<std::vector<double>> iter_method ;
  std::vector<double> publish_values;

public:

  PushSum(int machine_number, int size_of_system, int number_of_neighbors, double starting_value, ValueTag myTag, std::vector<ValueTag> tags_vector, AsynchronousIterative<std::vector<double>> it):
    machine_number(machine_number),
    size_of_system(size_of_system),
    number_of_neighbors(number_of_neighbors),
    x_value(starting_value),
    myTag(myTag),
    tags_vector(tags_vector),
    iter_method(it)
  {
    for(int i = 0 ; i < size_of_system; i++)
    {
      rho_x.push_back(0.0);
      rho_y.push_back(0.0);
      rho_x_previous.push_back(0.0);
      rho_y_previous.push_back(0.0);
    }
    in_nodes_plus_one = number_of_neighbors + 1.0;
    // Local weights -> This is the information passed to neighbors.
    sigma_x = sigma_x + (x_value / in_nodes_plus_one);
    sigma_y = sigma_y + (y_value / in_nodes_plus_one);

    // Stopping Criterion Initialization
    max_new_information = 20 * size_of_system;
    max_run_time = std::chrono::milliseconds(1000)* size_of_system;
    // Initial send message
    for(int j = 0 ; j < 3 ; j++)
    {
      publish_values.push_back(0.0);
    }
    obtain_publish_values();
    iter_method.submit_values(publish_values);
  };

  void run()
  {
    auto start_push_sum = std::chrono::high_resolution_clock::now();
    auto stop_push_sum = std::chrono::high_resolution_clock::now();
    while(iterate)
    {
      create_iteration();
      // This allows for quick diagnostics by seeing what's in the buffers rho_x, rho_y,  at terminal.
      // print_current_information();
      stop_push_sum = std::chrono::high_resolution_clock::now();
      run_time = std::chrono::duration_cast<std::chrono::microseconds>(stop_push_sum - start_push_sum);
      iterate = should_stop(run_time, max_run_time, return_new_information_count(), max_new_information);
    }
  }

  void create_iteration()
  {
    // This stores the values as a AsynchronousValues allocator which contains a bool if it is updated and a vector<double> and alive tags as vector<ValueTag>.
    const auto& [values, alive_tags] = iter_method.values();
    for(int values_index = 0; values_index < static_cast<int>(values.size()); ++values_index)
    {
      //stores the received values as a vector<double> and updated as bool
      const auto& [received_values, updated] = values[values_index];
      if (updated)
      {
        // index is used below to identify whether the component in question should be updated or not by checking it against the row_indices.
        for(int tags_index = 0; tags_index < size_of_system; tags_index++)
        {
          // this checks if alive_tag we are looking matches against the index we are currently checking, this is a matter of "when" not "if" we find the correct tag
          // This also avoids taking one's own received message; this shouldn't matter, but if data is corrupted it might.
          // This also saves time since all tags need to be passed into the asynchronous_iterative including each processes "pubTag" and has data associated with it after using iter_method.submit_values(), but each process already has that information.
          if(alive_tags[values_index] == tags_vector[tags_index] && alive_tags[values_index]!= myTag )
          {
            push_sum_computation(tags_index, received_values);
          }
        }
      }
    }
    // This sleep is necessary to allow the buffers behind the scene to process as intended. 
    // i.e., without this, updated = False almost always, and this means no new information is detected.
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
  };

  ~PushSum(){};

  void obtain_publish_values()
  {
    publish_values[0] = sigma_x;
    publish_values[1] = sigma_y;
    publish_values[2] = new_information_count;
  }

  void push_sum_computation(int index, std::vector<double> received_values)
  {
    new_information_count +=1.0;
    assert(index < size_of_system && index >=0);

    rho_x_previous[index] = rho_x[index];
    rho_y_previous[index] = rho_y[index];
    rho_x[index] = received_values[0];
    rho_y[index] = received_values[1];

    x_value = x_value + rho_x[index] - rho_x_previous[index];
    y_value = y_value + rho_y[index] - rho_y_previous[index];

    // This is the 'wake up' portion followed by broadcast (push_sum theory relevant)
    sigma_x = sigma_x + (x_value / in_nodes_plus_one);
    sigma_y = sigma_y + (y_value / in_nodes_plus_one);

    x_value = x_value / in_nodes_plus_one;
    y_value = y_value / in_nodes_plus_one;

    // Checks the max neighbor iterations for terminal checks.
    if (received_values[2] > max_neighbor_received)
    {
      max_neighbor_received = received_values[2];
    }

    obtain_publish_values();
    iter_method.submit_values(publish_values);
  }

  double return_solution()
  {
    double consensus_value = x_value/y_value;
    return consensus_value;
  }

 double return_new_information_count()
  {
    return new_information_count;
  }
  
  double return_run_time()
  {
    return run_time.count();
  }

  // For troubleshooting in the case of bad communication or unexplained behavior.
  void print_current_information()
  {
    std::cout << "\trho_x: \t" ;
    for(auto entry : rho_x)
    {
      std::cout << entry << " " ;
    }
    std::cout << std::endl;
    std::cout << "\trho_y: \t" ;
    for(auto entry : rho_y)
    {
      std::cout << entry << " " ;
    }
    std::cout << std::endl;
  }

  void print_publish_values()
  {
    std::cout << "\tpublish values: \t" ;
    for(auto entry : publish_values)
    {
      std::cout << entry << " " ;
    }
    std::cout << std::endl;
  }

}; 

// This is the continuation that makes this class possible as this implementation depends upon the asynchronous_iterative class.
template<typename... Args>
auto create_push_sum(int machine_number, int size_of_system, int number_of_neighbors, double starting_value, ValueTag myTag, std::vector<ValueTag> tags,  Args&&... args) noexcept
{
  return create_asynchronous_iterative(std::forward<Args>(args)...).then([=](std::optional<AsynchronousIterative<std::vector<double>>> it) -> std::optional<PushSum> {
     if (it) { return PushSum(machine_number, size_of_system, number_of_neighbors, starting_value, myTag, tags, *it);}
     else    { return {}; }
  });
}

#endif