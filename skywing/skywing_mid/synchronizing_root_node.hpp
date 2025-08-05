#ifndef SKYWING_MID_ROOT_NODE
#define SKYWING_MID_ROOT_NODE

#include <chrono>
#include <iostream>

#include "skywing_core/manager.hpp"
#include "skywing_core/skywing.hpp"
#include "skywing_math_interface/machine_setup.hpp"
#include "skywing_mid/associative_vector.hpp"
#include "skywing_mid/asynchronous_iterative.hpp"
#include "skywing_mid/data_input.hpp"
#include "skywing_mid/publish_policies.hpp"
#include "skywing_mid/iteration_policies.hpp"

using pubtag_t = skywing::Tag<int>;

namespace skywing
{

 /**
 * @brief Root node class for synchronizing agents in a distributed computation.
 *
 * This class sets up and runs the root node root. It coordinates agent synchronization during each iteration, 
 * and signals agents when to proceed.
 */
class SynchronizingRootNode
{
public:
    SynchronizingRootNode(
        std::unordered_map<std::string, MachineConfig> configurations, size_t root_port, 
        std::vector<pubtag_t> tags, std::chrono::seconds timeout_duration)
        : configurations_(configurations),
        root_port_(root_port),
        tags_(tags),
        timeout_duration_(timeout_duration){
            agent_name_ = std::string("root");
            num_agents_ = configurations.size();
            SKYWING_DEBUG_LOG("The number of agents in the collective is {}", num_agents_ );
        }

    /**
     * @brief Synchronizes all agents for each iteration of the job.
     *
     * For each iteration:
     * - Waits until all agents signal they are ready.
     * - Signals all agents to start the next iteration.
     * 
     * Note: This uses hardcoded values of the job IDs to connect to. This will need
     * to
     */
    void sync_iteration_job(Job& job, ManagerHandle manager_handle)
    {
        // To remove compiler error 
        (void) manager_handle;
        auto start_time = std::chrono::steady_clock::now();
        SKYWING_DEBUG_LOG("Root node beginning the job");
        pubtag_t root_signal_tag = pubtag_t{"root_signal"};
        job.declare_publication_intent(root_signal_tag);
        // Subscribe to all of the agent_signal_tags
        for (pubtag_t& agent_signal_tag : tags_){
            job.subscribe(agent_signal_tag).wait();
            SKYWING_DEBUG_LOG("Subscribing to agent signal tag");
        }
        
        bool timeout_occured = false;
        // Wait for all agents in the collective to be ready, then signal to continue.
        int iter_count = 0;
        while (true) {
            std::vector<Waiter<void>> waiters;
            waiters.reserve(num_agents_);
            SKYWING_DEBUG_LOG("Waiting to recieve ready signal from all {} agents", num_agents_); 
            for (const auto& tag : tags_) {
                waiters.push_back(job.get_target_val_waiter(tag, iter_count));
            }
            WaiterVec waitervec_ = make_waitervec(std::move(waiters));
            bool collective_ready = false;
            while (!collective_ready) {
                // This timeout value balances reducing excess waiting with limiting the number of
                // messages sent, and could be changed depending on priorities
                collective_ready = waitervec_.wait_for(std::chrono::milliseconds(50));
                // To prevent deadlock, resend the last iteration number in case some agents missed it
                job.publish(root_signal_tag, iter_count - 1);
                auto elapsed = std::chrono::steady_clock::now() - start_time;
                // To prevent deadlock - check for timeout every time waiting is iterupted
                // and exit job completely if timeout has occured
                if (elapsed > timeout_duration_){
                    timeout_occured = true;
                    break;
                }
            }
            if (timeout_occured){
                break;
            }
            SKYWING_DEBUG_LOG("Signaling start of iteration count {}",
                                iter_count);
            job.publish(root_signal_tag, iter_count);
            iter_count++;

        }
    };

     /**
     * @brief Main run() function for the root node.
     *
     * - Sets up the Skywing manager.
     * - Reads the global config file and collects all neighbors, initializes the manager to connect to all neighbors.
     * - Begins the above job
     */
    void run()
    {
        // Create a Skywing Manager
        manager_ = std::make_unique<skywing::Manager>(root_port_, agent_name_);

        // Create a vector of all of the agents in the collective
        std::vector<std::tuple<std::string, uint16_t>> all_address_port_pairs =
            collect_all_address_port_pairs_from_machine_configurations(
                configurations_);
        // The root node is neighbors will all the agents
        manager_->configure_initial_neighbors(all_address_port_pairs,
            std::chrono::seconds(30)); // Set timeout for forming initial connections
        auto sync_iteration_job_lambda = [&](Job& job, ManagerHandle manager_handle)
        {
            sync_iteration_job(job, manager_handle);
        };

        manager_->submit_job("job", sync_iteration_job_lambda);
        manager_->run();

    }
private:

    std::unordered_map<std::string, MachineConfig> configurations_;
    size_t root_port_;
    std::unique_ptr<skywing::Manager> manager_; // Store the Manager instance
    std::vector<pubtag_t> tags_;// Signalling tags of the agents in the collective
    std::string agent_name_;
    size_t num_agents_;
    std::chrono::seconds timeout_duration_;

};

} // namespace skywing
#endif
