#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>

#include "utils.hpp"

#include "skywing_core/enable_logging.hpp"
#include <catch2/catch_test_macros.hpp>

#include "skywing_core/skywing.hpp"
#include "skywing_mid/linear_system_processors/jacobi_processors/jacobi_processor.hpp"
#include "skywing_math_interface/linear_system_driver.hpp"
#include "skywing_math_interface/machine_setup.hpp"
#include "skywing_math_interface/fake_io.hpp"
#include "skywing_core/manager.hpp"
#include "skywing_mid/asynchronous_iterative.hpp"
#include "skywing_mid/data_input.hpp"
#include "skywing_mid/stop_policies.hpp"
#include "skywing_mid/publish_policies.hpp"


using namespace skywing;

namespace{
    constexpr int num_machines = 3;

void machine_task(const int index)
{
    // Calculate the agent ID 
    unsigned agent_id = index;

    using MyJacobiProcessor = JacobiProcessor<uint32_t, double>;
    // Set the processor, stop, publish, and resilience policies
    using MyJacobiDriver = LinearSystemDriver<MyJacobiProcessor, AlwaysPublish, StopAfterTime, TrivialResiliencePolicy>;


    // Read in the partition of the linear system, and this agent's portion of A and b from specified files
    std::unordered_map<uint32_t, std::vector<uint32_t>> partition = read_partition_from_file();
    MyJacobiProcessor::AssociativeMatrix A = read_matrix_from_file(agent_id);
    MyJacobiProcessor::ClosedVector b = read_rhs_from_file( agent_id); 

    // This associates with each machine name a MachineConfig struct
    // A MachineConfig struct stores that machine's name, port, address, and its neighbors' machine names
     // configurations = read_machine_configurations_from_file(machine_config_file);

    const std::uint16_t start_port = get_starting_port();
    const std::array<std::uint16_t, 3> ports{
        start_port,
        static_cast<std::uint16_t>(start_port + 1),
        static_cast<std::uint16_t>(start_port + 2)};

    MachineConfig config0{"agent0", "127.0.0.1", {"agent1"}, ports[0]};
    MachineConfig config1{"agent1", "127.0.0.1", {"agent2"}, ports[1]};
    MachineConfig config2{"agent2", "127.0.0.1", {}, ports[2]};

    std::unordered_map<std::string, MachineConfig> configurations = {{"agent0", config0}, {"agent1", config1}, {"agent2", config2}};

    MyJacobiDriver driver(configurations, agent_id, A, b, partition);
    driver.solve();
    MyJacobiProcessor::ClosedVector target0({{0, 0.02},  {3, 0.0526316}, {6, 0.035}});
    MyJacobiProcessor::ClosedVector target1( {{1, 0.04695}, {4, 0.1},{7, 0.026}});
    MyJacobiProcessor::ClosedVector target2({{8, 0.48}, {2, 0.02}, {5, 0.2}}); // change back to .2, this is to make sure that this is actually working
    std::vector<MyJacobiProcessor::ClosedVector> targets = {target0, target1, target2};
    REQUIRE(std::abs((driver.test_output() - targets[index]).at(index)) < .0001);
    REQUIRE(std::abs((driver.test_output() - targets[index]).at(index + 3)) < .0001);
    REQUIRE(std::abs((driver.test_output() - targets[index]).at(index + 6)) < .0001);
}
} // namespace

TEST_CASE("Jacobi Test", "[mid]")
{
    std::vector<std::thread> threads;
    for (auto i=0; i< num_machines; ++i) {
        threads.emplace_back(machine_task, i);
    }
    for (auto&& thread : threads) {
        thread.join();
    }
}
