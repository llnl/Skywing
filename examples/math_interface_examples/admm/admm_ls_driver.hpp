#ifndef ADMM_HPP
#define ADMM_HPP

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
#include "skywing_mid/linear_system_processors/admm_processor.hpp"
#include "skywing_mid/linear_system_processors/admm_shared_processor.hpp"
#include "skywing_mid/publish_policies.hpp"
#include "skywing_mid/synchronous_iterative.hpp"
#include <Eigen/Dense>

using namespace skywing;

using index_t = int;
using scalar_t = double;

// WM: todo - move some of these utilities to a reusable location? Copy/paste
// from barbell. Variable naming convention? variableName vs. variable_name?

// Utility function: Print machine configurations
void printConfigurations(
    const std::unordered_map<std::string, MachineConfig>& configurations)
{
    for (const auto& pair : configurations) {
        std::cout << "Key: " << pair.first << ", Value: " << pair.second
                  << std::endl;
    }
}

// Utility function: Set machine configurations (simple line connectivity
// between agents)
std::unordered_map<std::string, MachineConfig>
setConfigurations(std::uint16_t startingPort, std::uint16_t systemSize)
{
    std::unordered_map<std::string, MachineConfig> configurations;

    for (std::uint16_t i = 0; i < systemSize; ++i) {
        std::string agentName = "agent" + std::to_string(i);
        std::string localIp = "127.0.0.1";
        std::vector<std::string> neighborAgents;
        if (i == systemSize - 1) {
            neighborAgents = {};
        }
        else {
            neighborAgents = {"agent" + std::to_string(i + 1)};
        }
        configurations[agentName] =
            MachineConfig{agentName,
                          localIp,
                          neighborAgents,
                          static_cast<uint16_t>(startingPort + i)};
    }

    return configurations;
}

// Solver function
void runSolver(
    const std::unordered_map<std::string, MachineConfig>& configurations,
    unsigned agentId,
    const std::string& data_dir,
    const std::string& output_dir,
    scalar_t lambda_,
    bool sync_,
    bool shared_,
    size_t timeout_)
{
    std::chrono::seconds timeout(timeout_);

    using SyncADMMProcessor =
        ProcessorSyncWrapper<ADMMProcessor, index_t, scalar_t>;
    using AsyncADMMProcessor = ADMMProcessor<index_t, scalar_t>;
    using SyncADMMSharedProcessor =
        ProcessorSyncWrapper<ADMMSharedProcessor, index_t, scalar_t>;
    using AsyncADMMSharedProcessor = ADMMSharedProcessor<index_t, scalar_t>;

    using SyncADMMDriver = LinearSystemDriver<SyncADMMProcessor,
                                              AlwaysPublish,
                                              IterateUntilTime,
                                              TrivialResiliencePolicy>;
    using AsyncADMMDriver = LinearSystemDriver<AsyncADMMProcessor,
                                               AlwaysPublish,
                                               IterateUntilTime,
                                               TrivialResiliencePolicy>;
    using SyncADMMSharedDriver = LinearSystemDriver<SyncADMMSharedProcessor,
                                                    AlwaysPublish,
                                                    IterateUntilTime,
                                                    TrivialResiliencePolicy>;
    using AsyncADMMSharedDriver = LinearSystemDriver<AsyncADMMSharedProcessor,
                                                     AlwaysPublish,
                                                     IterateUntilTime,
                                                     TrivialResiliencePolicy>;

    // WM: todo - expose parameters as command line args
    scalar_t rho = 1.0;
    if (sync_)
    {
        if (shared_)
        {
            SyncADMMSharedDriver driver(configurations,
                                        agentId,
                                        data_dir,
                                        output_dir,
                                        timeout,
                                        true);
            driver.solve(rho, lambda_);
        }
        else
        {
            SyncADMMDriver driver(configurations,
                                  agentId,
                                  data_dir,
                                  output_dir,
                                  timeout,
                                  true);
            driver.solve(1.0);
        }
    }
    else
    {
        if (shared_)
        {
            AsyncADMMSharedDriver driver(configurations,
                                         agentId,
                                         data_dir,
                                         output_dir,
                                         timeout,
                                         false);
            driver.solve(rho, lambda_);
        }
        else
        {
            AsyncADMMDriver driver(configurations,
                                   agentId,
                                   data_dir,
                                   output_dir,
                                   timeout,
                                   false);
            driver.solve(1.0);
        }
    }
}

int drive_ADMM(size_t starting_port,
               size_t num_agents,
               scalar_t lambda,
               bool sync,
               bool shared,
               std::string data_dir,
               std::string output_dir,
               size_t timeout)
{
    // Generate machine configurations
    auto configurations = setConfigurations(starting_port, num_agents);
    printConfigurations(configurations);

    // Launch solver threads
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < num_agents; ++i) {
        // Launch the linear system driver job
        threads.emplace_back(runSolver,
                             configurations,
                             i,
                             data_dir,
                             output_dir,
                             lambda,
                             sync,
                             shared,
                             timeout);
    }

    // Join threads
    for (auto& thread : threads) {
        thread.join();
    }

    return 0;
}

#endif // ADMM_HPP
