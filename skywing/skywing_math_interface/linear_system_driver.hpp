#ifndef SKYWING_MATH_LINEAR_SYSTEM_DRIVER
#define SKYWING_MATH_LINEAR_SYSTEM_DRIVER

#include <iostream>
#include <chrono>

#include "skywing_core/manager.hpp"
#include "skywing_core/skywing.hpp"
#include "skywing_math_interface/machine_setup.hpp"
#include "skywing_mid/associative_vector.hpp"
#include "skywing_mid/asynchronous_iterative.hpp"
#include "skywing_mid/data_input.hpp"
#include "skywing_mid/publish_policies.hpp"
#include "skywing_mid/stop_policies.hpp"

namespace skywing
{
/** @brief Driver code for running a linear system
 * 
 * Creates an IterativeMethod with processor type LinearProcessor, then subscribes to tags
 * based on the matrix sparsity and runs the job. Here each agent is assigned a subset of the rows
 * based on partition, and publishes to one tag per agent. An agent subscribes to another's tag 
 * if it wants updates on any of the rows assigned to that agent.
 * 
 * @tparam LinearProcessor A processor to be used in the IterativeMethod class.
 * @tparam PublishPolicy The policy for when an agent should publish an update to a tag.
 * @tparam StopPolicy The policy for when this agent should stop, such as stopping after 
 * a certain time has elapsed. 
 * @tparam ResiliencePolicy the policy for handling or dropping unresponsive nodes
 */
template <typename LinearProcessor,
          typename PublishPolicy,
          typename StopPolicy,
          typename ResiliencePolicy>

class LinearSystemDriver
{
public:
    using OpenVector = typename LinearProcessor::OpenVector;
    using ClosedVector = typename LinearProcessor::ClosedVector;
    using AssociativeMatrix = typename LinearProcessor::AssociativeMatrix;

    LinearSystemDriver(std::unordered_map<std::string, MachineConfig> configurations, // map from machine names to MachineCongfigs
        unsigned agent_id,
        AssociativeMatrix A,
        ClosedVector b,
        std::unordered_map<uint32_t, std::vector<uint32_t>> partition, //assignments of machines to the matrix rows they own
        std::chrono::seconds timeout_duration) 
        : configurations_(configurations),
          agent_id_(agent_id),
          M_(A),
          c_(b),
          partition_(partition),
          test_output_(b),
          timeout_duration_(timeout_duration)
    {
        LinearProcessor::setup(A, b, M_, c_); // This sets M_ and c_ 
    }


/* @brief Skywing Job which subscribes to tags based on the matrix sparcity, 
    then solves the linear system using the specified processor.
 */
    void linear_system_job(skywing::Job& job, skywing::ManagerHandle manager_handle)
    {
        std::cout << "Agent " << agent_id_ << " beginning the job."
                  << std::endl;

        const double NONZERO_CUTOFF = 1e-12;

        // always subscribe to your own tag
        uint32_t my_machine_num = agent_id_;
        std::string pubTagID_ = "linear_system_tag" + std::to_string(my_machine_num);
        std::vector<std::string> tagIDs_for_sub = {
            "linear_system_tag" + std::to_string(my_machine_num)};
        std::vector<uint32_t> my_rows = partition_.at(my_machine_num);

        // create vector of tag IDs for subscription based on the sparsity of the matrix
        bool subscribed = false;
        // check if we need to subscribe to the tag corresponding to a machine
        for (const auto& [machine_num, assigned_rows] : partition_) {
            subscribed = false;
            // we have already subscribed to ourselves
            if (machine_num == my_machine_num)
                continue;
            for (uint32_t my_row : my_rows) {
                for (uint32_t assigned_row : assigned_rows) {
                    // if there is a non-zero entry in a column published by a machine, subscribe to its tag
                    if (std::abs((M_.at(my_row)).at(assigned_row))
                        > NONZERO_CUTOFF)
                    {
                        tagIDs_for_sub.push_back("linear_system_tag"
                                                 + std::to_string(machine_num));
                        subscribed = true;
                    }
                    // if we have already subscribed to this tag, we can stop looking
                    if (subscribed)
                        break;
                }
                if (subscribed)
                    break;
            }
        }

        // initialize a linear sytem solver with the specified linear processor
        using IterMethod = AsynchronousIterative<LinearProcessor,
                                                 AlwaysPublish,
                                                 StopAfterTime,
                                                 TrivialResiliencePolicy>;
        Waiter<IterMethod> iter_waiter =
            WaiterBuilder<IterMethod>(
                manager_handle, job, pubTagID_, tagIDs_for_sub)
                .set_processor(M_, c_)
                .set_publish_policy()
                .set_stop_policy(timeout_duration_) // stop iterating after this duration
                .set_resilience_policy()
                .build_waiter();

        // Callback called on each iteration of the iterative method.
        auto update_fun = [&](IterMethod& p) {
            std::cout << p.run_time().count() << "ms: Machine " << agent_id_
                      << " has value " << p.get_processor().get_value()
                      << std::endl;
        };

        IterMethod linear_system_solver = iter_waiter.get();

        linear_system_solver.run(update_fun);
        test_output_ = linear_system_solver.get_processor().get_value();
    }

    void solve()
    {  
        std::string name = "agent" + std::to_string(agent_id_);
        // look up port number for this agent for printing
        std::uint16_t port = configurations_.at(name).port;
        std::cout << "Agent " << agent_id_ << " is listening on port "
                  << port << std::endl;

        skywing::Manager manager(port, name);
        // use helper function to convert machine configurations object to
        // the address-port pairs object needed by the manager 
        std::vector<std::tuple<std::string, uint16_t>>
            neighbor_address_port_pairs =
                create_address_port_pairs_from_machine_configurations(
                    configurations_, name);
        manager.configure_initial_neighbors(neighbor_address_port_pairs,
                                            std::chrono::seconds(30)); //set timeout for forming initial connections
        
        // define job to run the linear system iterative method
        auto linear_system_lambda = [&](Job& job, ManagerHandle manager_handle)
        {
            linear_system_job(job, manager_handle);
        };

        manager.submit_job("linear_system_job", linear_system_lambda);
        manager.run();
    }

   ClosedVector test_output()
    {
        return test_output_;
    }


private:
    std::unordered_map<std::string, MachineConfig> configurations_;
    unsigned agent_id_;
    AssociativeMatrix M_;
    ClosedVector c_;
    std::unordered_map<uint32_t, std::vector<uint32_t>> partition_;
    ClosedVector test_output_;
    std::chrono::seconds timeout_duration_;
};

} // namespace skywing
#endif // SKYWING_MATH_LINEAR_SYSTEM_DRIVER