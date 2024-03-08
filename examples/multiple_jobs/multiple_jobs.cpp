#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>

#include "skywing_core/manager.hpp"
#include "skywing_core/skywing.hpp"
#include "skywing_mid/asynchronous_iterative.hpp"
#include "skywing_mid/big_float.hpp"
#include "skywing_mid/data_input.hpp"
#include "skywing_mid/publish_policies.hpp"
#include "skywing_mid/push_flow_processor.hpp"
#include "skywing_mid/push_sum_processor.hpp"
#include "skywing_mid/quacc_processor.hpp"
#include "skywing_mid/stop_policies.hpp"
#include "skywing_mid/sum_processor.hpp"

using namespace skywing;
using ValueTag = skywing::PublishTag<double>;

// Get names of the agents in the collective.
std::vector<std::string> obtain_machine_names(std::uint16_t size_of_system)
{
    std::vector<std::string> machine_names;
    machine_names.resize(size_of_system);
    for (int i = 0; i < size_of_system; i++) {
        machine_names[i] = "node" + std::to_string(i + 1);
    }
    return machine_names;
}

// Get the ports for each agent in the collective.
std::vector<std::uint16_t> set_port(std::uint16_t starting_port_number,
                                    std::uint16_t size_of_system)
{
    std::vector<std::uint16_t> ports;

    for (std::uint16_t i = 0; i < size_of_system; i++) {
        ports.push_back(starting_port_number + (i * 1));
    }
    return ports;
}

// Get each agent's tag ID for the summation job.
std::vector<std::string> obtain_tag_ids(int size_of_system)
{
    std::vector<std::string> tags;
    for (int i = 0; i < size_of_system; i++) {
        std::string hold = "summation_tag" + std::to_string(i);
        tags.push_back(hold);
    }
    return tags;
}

// // For this example, the exact average can be computed by inputting the system
// // size.
// double obtain_exact_average(int size_of_system)
// {
//     double average = 0.0;
//     for (int i = 0; i < size_of_system; i++) {
//         average += 1.0 * i + 1.0;
//     }
//     average /= size_of_system;
//     return average;
// }

void machine_task(int machine_number,
                  int size_of_system,
                  int number_of_neighbors,
                  std::vector<std::uint16_t> ports,
                  std::vector<std::string> machine_names,
                  std::string pubTagID,
                  std::vector<std::string> tagIDs)
{
    skywing::Manager manager{ports[machine_number],
                             machine_names[machine_number]};

    ValueTag summation_result_tag{"summation_result"};
    ValueTag contribution_update_tag{"contribution_update"
				     + std::to_string(machine_number)};

    auto summation_job = [&](Job& job, ManagerHandle manager_handle)
    {
      if (machine_number != static_cast<int>((ports.size()) - 1)) {
	// Connecting to the server is an asynchronous operation and can
	// fail. Wait for the result each time and keep attempting to
	// connect until it does
	while (!manager_handle.connect_to_server
	       ("127.0.0.1", ports[machine_number + 1]).get())
	  { }
      }
      
      // make gossip connections in a circle
      int i = machine_number;
      size_t number_of_neighbors = 2;

      auto wrap_ind = [&](int ind) {
			return (ind % size_of_system + size_of_system) % size_of_system;
		      };
      std::vector<std::string> tagIDs_for_sub
	{ tagIDs[wrap_ind(i - 1)], tagIDs[i], tagIDs[wrap_ind(i + 1)] };

      // set up publishing of summation results
      job.declare_publication_intent(summation_result_tag);

      // set up subscribing to individual update from other job on this agent
      job.subscribe(contribution_update_tag).wait();
      double starting_value = *job.get_waiter(contribution_update_tag).get();

      // set up summation iteration
      (void) number_of_neighbors;
      using CountProcessor = QUACCProcessor<BigFloat,
					    MinProcessor<BigFloat>,
					    PushFlowProcessor<BigFloat>>;
      using SumMethod =
	SumProcessor<double, PushFlowProcessor<double>, CountProcessor>;
      using IterMethod = AsynchronousIterative<SumMethod,
					       AlwaysPublish,
					       StopAfterTime,
					       TrivialResiliencePolicy>;
      Waiter<IterMethod> iter_waiter =
	WaiterBuilder<IterMethod>(
				  manager_handle, job, pubTagID, tagIDs_for_sub)
	.set_processor(starting_value)
	.set_publish_policy()
	.set_stop_policy(std::chrono::seconds(60))
	.set_resilience_policy()
	.build_waiter();

      IterMethod summation_iteration = iter_waiter.get();

      // set up lambda function to publish result and update contribution
      auto update_fun = [&](decltype(summation_iteration)& p)
      {
	double current_value = p.get_processor().get_value();
	std::cout << p.run_time().count() << "ms: Agent "
		  << machine_number << " producing value"
		  << current_value << std::endl;
	job.publish(summation_result_tag, current_value);

	double contrib_value = *job.get_waiter(contribution_update_tag).get();
	std::cout << p.run_time().count() << "ms: Agent "
		  << machine_number << " receiving contribution"
		  << contrib_value << std::endl;
	p.get_processor().set_value(contrib_value);
      };
      
      summation_iteration.run(update_fun);
    };

    auto use_result_job = [&](Job& job, ManagerHandle manager_handle)
    {
      double contrib_value = machine_number + 1;
      job.declare_publication_intent(contribution_update_tag);
      job.publish(contribution_update_tag, contrib_value);

      std::this_thread::sleep_for(std::chrono::seconds(5));
      
      job.subscribe(summation_result_tag);

      for (size_t i = 0; i < 5; i++)
      {
	for (size_t j = 0; j < 4; j++)
	{
	  double curr_sum_result = *job.get_waiter(summation_result_tag).get();
	  std::cout << "Agent "
		    << machine_number << " seeing summation result "
		    << curr_sum_result << std::endl;
	  std::this_thread::sleep_for(std::chrono::seconds(3));
	}
	contrib_value = contrib_value * 10;
	job.publish(contribution_update_tag, contrib_value);
      }
    };

    manager.submit_job("summation_job", summation_job);
    manager.submit_job("use_result_job", use_result_job);

    manager.run();
}

int main(int argc, char* argv[])
{
    // Error checking for the number of arguments
    if (argc < 4) {
        std::cout << "Usage: Note Enough Arguments: " << argc << std::endl;
        return 1;
    }
    // Parse the machine number, starting_port_number, and size_of_system that
    // was passed in
    int machine_number = std::stoi(argv[1]);
    std::uint16_t starting_port_number = std::stoi(argv[2]);
    int size_of_system = std::stoi(argv[3]);
    if (machine_number > size_of_system - 1 || machine_number < 0) {
        std::cerr << "Invalid machine_number of " << std::quoted(argv[1])
                  << ".\n"
                  << "Must be an integer between 0 and " << size_of_system - 1
                  << '\n';
        return -1;
    }
    if (size_of_system <= 0) {
        std::cerr << "Invalid size_of_system of " << std::quoted(argv[1])
                  << ".\n"
                  << "Must be an integer greater than 0 and  match the number "
                     "of threads created. \n";
        return -1;
    }

    // Skywing setup
    std::vector<std::uint16_t> ports =
        set_port(starting_port_number, size_of_system);
    std::vector<std::string> machine_names =
        obtain_machine_names(size_of_system);
    std::vector<std::string> subTagIDs = obtain_tag_ids(size_of_system);

    // This pubTag is exists in subTags[machine_number] which is needed for
    // initialization, but its declared separately here mainly to highlight how
    // the creator works for the push_sum class.
    std::string pubTagID("push_sum_tag" + std::to_string(machine_number));
    // Push sum variables -> initialized by user

    int number_of_neighbors = size_of_system - 1;

    // Skywing job
    machine_task(machine_number,
                 size_of_system,
                 number_of_neighbors,
                 ports,
                 machine_names,
                 pubTagID,
                 subTagIDs);
    return 0;
}
