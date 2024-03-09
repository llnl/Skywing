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

using CountProcessor = QUACCProcessor<BigFloat,
				      MinProcessor<BigFloat>,
				      PushFlowProcessor<BigFloat>>;
using SumMethod =
  SumProcessor<double, PushFlowProcessor<double>, CountProcessor>;
using IterMethod = AsynchronousIterative<SumMethod,
					 AlwaysPublish,
					 StopAfterTime,
					 TrivialResiliencePolicy>;

struct ParamStruct
{
  int agent_number;
  int size_of_system;
  std::vector<std::uint16_t> ports;
  std::vector<std::string> agent_names;
  std::string pubTagID;
  std::vector<std::string> tagIDs;
  ValueTag summation_result_tag;
  ValueTag contribution_update_tag;
};

/* \brief The job that executives a collective summation.

 * This job continually executives a collective summation. It
 * contributes a single value to the collective summation.

 * It subscribes to a tag that, when it has data, represents an update
 * to the value this agent is contribution to the summation. It
 * continually checks that tag; when the tag has new data, this job
 * immediately updates its contribution to the summation.
 *
 * This job also publishes a value that represents its current
 * estimation of the collective summation result. This value is
 * continually updated, and it is continually publishing its current
 * estimate.
 */
void summation_job_fun(Job& job, ManagerHandle manager_handle,
		       ParamStruct& param_struct)
{
  int agent_number = param_struct.agent_number;
  int size_of_system = param_struct.size_of_system;
  std::vector<std::uint16_t> ports = param_struct.ports;
  std::vector<std::string> agent_names = param_struct.agent_names;
  std::string pubTagID = param_struct.pubTagID;
  std::vector<std::string> tagIDs = param_struct.tagIDs;
  ValueTag summation_result_tag = param_struct.summation_result_tag;
  ValueTag contribution_update_tag = param_struct.contribution_update_tag;
  
  if (agent_number != static_cast<int>((ports.size()) - 1)) {
    // Connecting to the server is an asynchronous operation and can
    // fail. Wait for the result each time and keep attempting to
    // connect until it does
    while (!manager_handle.connect_to_server
	   ("127.0.0.1", ports[agent_number + 1]).get())
      { }
  }
      
  // make gossip connections in a circle.
  int i = agent_number;
  auto wrap_ind = [&](int ind) {
		    return (ind % size_of_system + size_of_system) % size_of_system;
		  };
  std::vector<std::string> tagIDs_for_sub
    { tagIDs[wrap_ind(i - 1)], tagIDs[i], tagIDs[wrap_ind(i + 1)] };

  // // set up publishing of summation results
  job.declare_publication_intent(summation_result_tag);

  // set up subscribing to individual update from other job on this agent
  job.subscribe(contribution_update_tag).wait();
  double starting_value = *job.get_waiter(contribution_update_tag).get();
    
  Waiter<IterMethod> iter_waiter =
    WaiterBuilder<IterMethod>(
			      manager_handle, job, pubTagID, tagIDs_for_sub)
    .set_processor(starting_value)
    .set_publish_policy()
    .set_stop_policy(std::chrono::seconds(60))
    .set_resilience_policy()
    .build_waiter();
    
  IterMethod summation_iteration = iter_waiter.get();

  // Set up lambda function to publish result and update
  // contribution. This function is called by the iterative method on
  // every iteration.
  auto update_fun = [&](IterMethod& p)
  {
    double current_value = p.get_processor().get_value();
    std::cout << p.run_time().count() << "ms: Agent "
	      << agent_number << " producing value "
	      << current_value << std::endl;
    job.publish(summation_result_tag, current_value);

    std::optional<double> contrib_value
      = job.get_data_if_present(contribution_update_tag);
    if (contrib_value)
      {
	std::cout << p.run_time().count() << "ms: Agent "
		  << agent_number << " receiving contribution"
		  << *contrib_value << std::endl;
	p.get_processor().set_value(*contrib_value);
      }
  };
   
  summation_iteration.run(update_fun);  
}

/* \brief The job that updates this agent's contribution to the
   summation and uses the summation result.

 * This job provides an initial value to the above summation job
 * through a publication. Then it does two things:
 * 1) Every 3 seconds, through a subscription, it checks for the
 * current summation estimate from the summation job on this agent.
 * 2) Every 12 seconds, it updates the value this agent contributes to
 * the summation.
 */
void use_result_job_fun(Job& job, ManagerHandle manager_handle,
			ParamStruct& param_struct)
{
  int agent_number = param_struct.agent_number;
  ValueTag summation_result_tag = param_struct.summation_result_tag;
  ValueTag contribution_update_tag = param_struct.contribution_update_tag;

  double contrib_value = agent_number + 1;
  job.declare_publication_intent(contribution_update_tag);
  std::this_thread::sleep_for(std::chrono::seconds(2));
  job.publish(contribution_update_tag, contrib_value);
  std::this_thread::sleep_for(std::chrono::seconds(5));
      
  job.subscribe(summation_result_tag);

  for (size_t i = 0; i < 5; i++)
    {
      for (size_t j = 0; j < 4; j++)
    	{
	  std::optional<double> curr_sum_result
	    = job.get_waiter(summation_result_tag).get();
	  if (curr_sum_result)
	    {
	      std::cout << "Agent "
			<< agent_number << " seeing summation result "
			<< *curr_sum_result << std::endl;
	      std::this_thread::sleep_for(std::chrono::seconds(3));
	    }
    	}
      contrib_value = contrib_value * 10;
      std::cout << "Agent "
		<< agent_number << " increasing contrib to "
		<< contrib_value << std::endl;
      job.publish(contribution_update_tag, contrib_value);
    }
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
    int agent_number = std::stoi(argv[1]);
    std::uint16_t starting_port_number = std::stoi(argv[2]);
    int size_of_system = std::stoi(argv[3]);

    std::vector<std::uint16_t> ports;
    std::vector<std::string> agent_names;
    std::vector<std::string> subTagIDs;
    for (std::size_t i = 0; i < size_of_system; i++)
    {
      ports.push_back(starting_port_number + i);
      agent_names.push_back("agent" + std::to_string(i+1));
      subTagIDs.push_back("summation_tag" + std::to_string(i));
    }
    std::string pubTagID = subTagIDs[agent_number];

    ParamStruct param_struct;
    param_struct.agent_number = agent_number;
    param_struct.size_of_system = size_of_system;
    param_struct.ports = ports;
    param_struct.agent_names = agent_names;
    param_struct.pubTagID = pubTagID;
    param_struct.tagIDs = subTagIDs;
    param_struct.summation_result_tag = ValueTag("summation_result");
    param_struct.contribution_update_tag
      = ValueTag("contribution_update" + std::to_string(agent_number));

    auto summation_job = [&param_struct](Job& job, ManagerHandle manager_handle)
    {
      summation_job_fun(job, manager_handle, param_struct);
    };

    auto use_result_job = [&param_struct](Job& job, ManagerHandle manager_handle)
    {
      use_result_job_fun(job, manager_handle, param_struct);
    };

    skywing::Manager manager{ports[agent_number],
                             agent_names[agent_number]};
    manager.submit_job("summation_job", summation_job);
    manager.submit_job("use_result_job", use_result_job);
    manager.run();

    return 0;
}
