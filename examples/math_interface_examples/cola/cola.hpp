#ifndef COLA_HPP
#define COLA_HPP

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "skywing_math_interface/linear_system_driver.hpp"
#include "skywing_math_interface/machine_setup.hpp"
#include "skywing_mid/associative_vector.hpp"
#include "skywing_mid/iteration_policies.hpp"
#include "skywing_mid/linear_system_processors/cola_processor.hpp"
#include "skywing_mid/publish_policies.hpp"
#include "skywing_mid/synchronous_iterative.hpp"

using namespace skywing;

using index_t = int;
using scalar_t = double;
using tag_t = std::string;

AssociativeVector<tag_t, scalar_t, false>
generate_mixing_matrix(std::string comm_topology_file, size_t agent_id)
{
    // Read the specified comm_topology
    std::unordered_map<uint32_t, std::vector<uint32_t>> comm_topology =
        readPartition<uint32_t>(comm_topology_file);

    // Note: W_k is indexed by the tag ids, and these need to conform to what is
    // setup in the linear system driver WM: NOTE - this isn't the best
    // design... if we change how we name tags in the linear system driver, we
    // need to change it here as well
    std::vector<std::string> tag_ids;
    for (auto nbr : comm_topology[agent_id]) {
        tag_ids.push_back("linear_system_tag" + std::to_string(nbr));
    }

    // Set value of mixing matrix for each neighbor to 1 / (num_neighbors)
    std::unordered_map<tag_t, scalar_t> W_k_data;
    for (const auto& tag_id : tag_ids) {
        W_k_data[tag_id] = 1.0 / tag_ids.size();
    }

    return AssociativeVector<tag_t, scalar_t, false>(W_k_data);
}

// Solver function
void runSolver(
    const std::unordered_map<std::string, MachineConfig>& configurations,
    unsigned agentId,
    const std::string& data_dir,
    const std::string& output_dir,
    const AssociativeVector<tag_t, scalar_t, false>& W_k,
    double lambda,
    size_t num_agents,
    bool shift_scale,
    bool sync_,
    size_t timeout_)
{
    std::chrono::seconds timeout(timeout_);
    using SyncCOLAProcessor =
        ProcessorSyncWrapper<COLAProcessor, index_t, scalar_t, tag_t>;
    using AsyncCOLAProcessor = COLAProcessor<index_t, scalar_t, tag_t>;
    using SyncCOLADriver = LinearSystemDriver<SyncCOLAProcessor,
                                              AlwaysPublish,
                                              IterateUntilTime,
                                              TrivialResiliencePolicy>;
    using AsyncCOLADriver = LinearSystemDriver<AsyncCOLAProcessor,
                                              AlwaysPublish,
                                              IterateUntilTime,
                                              TrivialResiliencePolicy>;

    if (sync_)
    {
        SyncCOLADriver driver(configurations,
                              agentId,
                              data_dir,
                              output_dir,
                              timeout,
                              true);
        driver.solve(W_k, lambda, num_agents, shift_scale);
    }
    else
    {
        AsyncCOLADriver driver(configurations,
                               agentId,
                               data_dir,
                               output_dir,
                               timeout,
                               false);
        driver.solve(W_k, lambda, num_agents, shift_scale);
    }
}

int drive_COLA(size_t starting_port,
               size_t num_agents,
               scalar_t lambda,
               bool shift_scale,
               bool sync,
               size_t timeout,
               std::string data_dir,
               std::string output_dir)
{
    // Generate machine configurations
    auto configurations = create_default_machine_configurations(starting_port, num_agents);

    std::filesystem::path comm_topology_file = data_dir;
    comm_topology_file /= "comm_topology.txt";
    
    // Launch solver threads
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < num_agents; ++i) {
        // Generate mixing matrix and dump to file
        auto W_k = generate_mixing_matrix(comm_topology_file, i);

        // Launch the linear system driver job
        threads.emplace_back(runSolver,
                             configurations,
                             i,
                             data_dir,
                             output_dir,
                             W_k,
                             lambda,
                             num_agents,
                             shift_scale,
                             sync,
                             timeout);
    }

    // Join threads
    for (auto& thread : threads) {
        thread.join();
    }

    return 0;
}

#endif // COLA_HPP
