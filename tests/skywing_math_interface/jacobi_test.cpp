#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>



#include "utils.hpp"

#include "skywing_core/enable_logging.hpp"
#include <catch2/catch_test_macros.hpp>

#include "skywing_core/skywing.hpp"
#include "skywing_mid/linear_system_processors/jacobi_processors/jacobi_processor.hpp"
#include "skywing_math_interface/linear_system_driver.hpp"
#include "skywing_math_interface/machine_setup.hpp"

#include "skywing_core/manager.hpp"
#include "skywing_math_interface/io/io.hpp"
#include "skywing_mid/asynchronous_iterative.hpp"
#include "skywing_mid/data_input.hpp"
#include "skywing_mid/stop_policies.hpp"
#include "skywing_mid/publish_policies.hpp"
#include "example_input.hpp"




using ClosedVector = AssociativeVector<index_t, scalar_t, false>;
using AssociativeMatrix = AssociativeVector<index_t, ClosedVector, false>;

using namespace skywing;

namespace{
    
    constexpr int num_machines = 3;

ClosedVector createClosedVector(std::vector<index_t>&& keys, const std::vector<scalar_t>& values) {
    if (keys.size() != values.size()) {
        throw std::invalid_argument("Keys and values must have the same size");
    }

    ClosedVector vec(std::move(keys), 0);  // Initialize with default value 0
    for (size_t i = 0; i < keys.size(); ++i) {
        vec[keys[i]] = values[i];
    }
    return vec;
}

void machine_task(const int index)
{
    std::vector<uint32_t> machine_0_partition; 
    machine_0_partition.push_back(0);
    machine_0_partition.push_back(1);
    machine_0_partition.push_back(2);
    machine_0_partition.push_back(3);

    std::vector<uint32_t> machine_1_partition; 
    machine_1_partition.push_back(4);
    machine_1_partition.push_back(5);

    std::vector<uint32_t> machine_2_partition; 
    machine_2_partition.push_back(6);
    machine_2_partition.push_back(7);
    machine_2_partition.push_back(8);

    set_dataDir() ; 

    ClosedVector machine_0_rhs = createClosedVector({0, 1, 2, 3}, {1.0, 0.0, 0.0, 0.0});
    ClosedVector machine_1_rhs = createClosedVector({4, 5}, {0.0, 0.0});
    ClosedVector machine_2_rhs = createClosedVector({6, 7, 8}, {0.0, 0.0, 1.0});
    
    ClosedVector row_0 = createClosedVector({0, 1, 2, 3, 4, 5 ,6, 7, 8}, {2.0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    ClosedVector row_1 = createClosedVector({0, 1, 2, 3, 4, 5 ,6, 7, 8}, {-1.0, 2.0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    ClosedVector row_2 = createClosedVector({0, 1, 2, 3, 4, 5 ,6, 7, 8}, {0.0, -1.0, 2.0, -1.0, 0.0, 0.0, 0.0, 0.0, 0.0});
    ClosedVector row_3 = createClosedVector({0, 1, 2, 3, 4, 5 ,6, 7, 8}, {0.0, 0.0, -1.0, 2.0, -1.0, 0.0, 0.0, 0.0, 0.0});
    ClosedVector row_4 = createClosedVector({0, 1, 2, 3, 4, 5 ,6, 7, 8}, {0.0, 0.0, 0.0, -1.0, 2.0, -1.0, 0.0, 0.0, 0.0});
    ClosedVector row_5 = createClosedVector({0, 1, 2, 3, 4, 5 ,6, 7, 8}, {0.0, 0.0, 0.0, 0.0, -1.0, 2.0, -1.0, 0.0, 0.0});
    ClosedVector row_6 = createClosedVector({0, 1, 2, 3, 4, 5 ,6, 7, 8}, {0.0, 0.0, 0.0, 0.0, 0.0, -1.0, 2.0, -1.0, 0.0});
    ClosedVector row_7 = createClosedVector({0, 1, 2, 3, 4, 5 ,6, 7, 8}, {0.0, 0.0, 0.0, 0.0, 0.0, 0.0,-1.0, 2.0, -1.0});
    ClosedVector row_8 = createClosedVector({0, 1, 2, 3, 4, 5 ,6, 7, 8}, {0.0, 0.0, 0.0, 0.0, 0.0, 0.0,0.0, -1.0, 2.0});
    
    
    AssociativeMatrix machine_0_A(std::vector<index_t>{0, 1, 2, 3},ClosedVector(0));
    machine_0_A[0] = row_0; 
    machine_0_A[1] = row_1; 
    machine_0_A[2] = row_2; 
    machine_0_A[3] = row_3; 

    AssociativeMatrix machine_1_A(std::vector<index_t>{4, 5},ClosedVector(0));
    machine_1_A[4] = row_4; 
    machine_1_A[5] = row_5; 

    AssociativeMatrix machine_2_A(std::vector<index_t>{6, 7, 8 },ClosedVector(0));
    machine_2_A[6] = row_6; 
    machine_2_A[7] = row_7; 
    machine_2_A[8] = row_8; 
    

    // Calculate the agent ID 
    unsigned agent_id = index;
  
    using MyJacobiProcessor = JacobiProcessor<uint32_t, double>;

    // Set the processor, stop, publish, and resilience policies
    using MyJacobiDriver = LinearSystemDriver<MyJacobiProcessor, AlwaysPublish, StopAfterTime, TrivialResiliencePolicy>;

    // Read in the partition of the linear system, and this agent's portion of A and b from specified files
    std::unordered_map<uint32_t, std::vector<uint32_t>> partition = read_partition_from_file();        
    MyJacobiProcessor::ClosedVector b = read_rhs_from_file( partition[agent_id]); 
    MyJacobiProcessor::AssociativeMatrix A = read_matrix_from_file(partition[agent_id]);


    
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
    std::chrono::seconds timeout = std::chrono::seconds(10);

    MyJacobiDriver driver(configurations, agent_id, A, b, partition, timeout);
    driver.solve();
    std::vector<double> targets = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}; 
    std::cout<< driver.test_output()<<std::endl;

    if( agent_id == 0 ){
        REQUIRE(partition[agent_id] == machine_0_partition); 
        REQUIRE(b == machine_0_rhs);
        REQUIRE(A == machine_0_A);
        auto output = driver.test_output();
        for (auto key : output.get_keys()) {
            REQUIRE(std::abs(output[key] - targets[key]) < 0.0001);
        }

    }
    else if( agent_id == 1){
        REQUIRE(partition[agent_id] == machine_1_partition); 
        REQUIRE(b == machine_1_rhs);
        REQUIRE(A == machine_1_A);
        auto output = driver.test_output();

        for (auto key : output.get_keys()) {
            REQUIRE(std::abs(output[key] - targets[key]) < 0.0001);
        }

    }
    else if (agent_id == 2) {
        REQUIRE(partition[agent_id] == machine_2_partition); 
        REQUIRE(b == machine_2_rhs);
        REQUIRE(A == machine_2_A);
        auto output = driver.test_output();
        for (auto key : output.get_keys()) {
            REQUIRE(std::abs(output[key] - targets[key]) < 0.0001);
        }

    }
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
