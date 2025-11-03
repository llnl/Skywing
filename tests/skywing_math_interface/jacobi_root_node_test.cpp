#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <filesystem>
#include <thread>
#include <vector>
#include <chrono>

#include "utils.hpp"

#include "skywing_core/enable_logging.hpp"
#include <catch2/catch_test_macros.hpp>

#include "skywing_core/skywing.hpp"
#include "skywing_mid/associative_vector.hpp"
#include "skywing_mid/associative_matrix.hpp"
#include "skywing_mid/linear_system_processors/jacobi_processor.hpp"
#include "skywing_math_interface/linear_system_driver.hpp"
#include "skywing_math_interface/machine_setup.hpp"
#include "skywing_core/manager.hpp"
#include "skywing_math_interface/linear_system_driver.hpp"
#include "skywing_mid/asynchronous_iterative.hpp"
#include "skywing_mid/data_input.hpp"
#include "skywing_mid/iteration_policies.hpp"
#include "skywing_mid/synchronizing_root_node.hpp"
#include "skywing_mid/publish_policies.hpp"

using index_t = uint32_t;
using scalar_t = double;

using namespace skywing;

using ClosedVector = AssociativeVector<index_t, scalar_t, false>;
using ClosedMatrix = AssociativeMatrix<index_t, scalar_t, false>;


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

int iter_count0;
int iter_count1;
int iter_count2;

// This is the same task as the jacobi test, but run with the root node iteration policy
void machine_task(const int index)
{
    const std::uint16_t start_port = get_starting_port();
    const std::array<std::uint16_t, 3> ports{
    start_port,
    static_cast<std::uint16_t>(start_port + 1),
    static_cast<std::uint16_t>(start_port + 2)};

    std::cout << "agent start port" << start_port << std::endl;


    MachineConfig config0{"agent0", "127.0.0.1", {"agent1"}, ports[0]};
    MachineConfig config1{"agent1", "127.0.0.1", {"agent2"}, ports[1]};
    MachineConfig config2{"agent2", "127.0.0.1", {}, ports[2]};

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
    
    
    ClosedMatrix machine_0_A(std::vector<index_t>{0, 1, 2, 3},ClosedVector(0));
    machine_0_A[0] = row_0; 
    machine_0_A[1] = row_1; 
    machine_0_A[2] = row_2; 
    machine_0_A[3] = row_3; 

    ClosedMatrix machine_1_A(std::vector<index_t>{4, 5},ClosedVector(0));
    machine_1_A[4] = row_4; 
    machine_1_A[5] = row_5; 

    ClosedMatrix machine_2_A(std::vector<index_t>{6, 7, 8 },ClosedVector(0));
    machine_2_A[6] = row_6; 
    machine_2_A[7] = row_7; 
    machine_2_A[8] = row_8; 
    

    // Calculate the agent ID 
    unsigned agent_id = index;
  
    using MyJacobiProcessor = JacobiProcessor<uint32_t, double>;

    // Set the processor, stop, publish, and resilience policies
    using MyJacobiDriver = LinearSystemDriver<MyJacobiProcessor, AlwaysPublish, WaitForRootSignal, TrivialResiliencePolicy>;

    // Report errors before using this variable to prevent silent errors
    #ifdef DATA_DIR_DEST
        std::cout << "Data directory: " << DATA_DIR_DEST << std::endl;
    #else
        std::cerr << "DATA_DIR_DEST is not defined!" << std::endl;
    #endif

    std::string partitionfile = std::string(DATA_DIR_DEST) +"/row_partition.txt";
    std::string rhsfile =  std::string(DATA_DIR_DEST) + "/b.txt";
    std::string matrixfile = std::string(DATA_DIR_DEST) + "/A.txt";

    // Read in the partition of the linear system, and this agent's portion of A and b from specified files
    std::unordered_map<uint32_t, std::vector<index_t>> partition = readPartition<index_t>(partitionfile);        
    ClosedMatrix A_assoc = ReadAssocitiveMatrix<index_t, scalar_t, false>(matrixfile, partition[agent_id]);
    ClosedVector b_assoc = ReadAssocitiveVector<index_t, scalar_t, false>(rhsfile, partition[agent_id]);
    // This associates with each machine name a MachineConfig struct
    // A MachineConfig struct stores that machine's name, port, address, and its neighbors' machine names
    // configurations = read_machine_configurations_from_file(machine_config_file);

    std::unordered_map<std::string, MachineConfig> configurations = {{"agent0", config0}, {"agent1", config1}, {"agent2", config2}};
    std::chrono::seconds timeout = std::chrono::seconds(15);

    std::string output_directory = "./temp_output";
    std::filesystem::create_directories(output_directory); // Create the folder


    MyJacobiDriver driver(configurations,
                          agent_id,
                          std::string(DATA_DIR_DEST),
                          output_directory,
                          timeout);
    driver.solve();
    std::vector<double> targets = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0}; 
    std::cout<< driver.test_output()<<std::endl;

    // Note that the non-root node requirement of < .0001 error is stricter than this requirement. This is
    // due to the fact that the root node collectives run slower, and so take longer to converge. The root 
    // node collective would eventually reach the smaller error, but it would take longer than 10 seconds.
    // In order to have a quicker test run time, we have a larger error tolerence here. 

    if( agent_id == 0 ){
        REQUIRE(partition[agent_id] == machine_0_partition); 
        REQUIRE(b_assoc == machine_0_rhs);
        REQUIRE(A_assoc == machine_0_A);
        auto output = driver.test_output();
        for (auto key : output.get_keys()) {
            REQUIRE(std::abs(output[key] - targets[key]) < 0.001);
        }
        iter_count0 = driver.iteration_count();
    }
    else if( agent_id == 1){
        REQUIRE(partition[agent_id] == machine_1_partition); 
        REQUIRE(b_assoc == machine_1_rhs);
        REQUIRE(A_assoc == machine_1_A);
        auto output = driver.test_output();

        for (auto key : output.get_keys()) {
            REQUIRE(std::abs(output[key] - targets[key]) < 0.001);
        }
        iter_count1 = driver.iteration_count();

    }
    else if (agent_id == 2) {
        REQUIRE(partition[agent_id] == machine_2_partition); 
        REQUIRE(b_assoc == machine_2_rhs);
        REQUIRE(A_assoc == machine_2_A);
        auto output = driver.test_output();
        for (auto key : output.get_keys()) {
            REQUIRE(std::abs(output[key] - targets[key]) < 0.001);
        }
        iter_count2 = driver.iteration_count();

    }

}

// This is the thread running the root node. It does not have any test requirements other than exiting without 
// error, but faciliates the rest of the collective. 
void root_machine_task()
{
    const std::uint16_t start_port = get_starting_port();
    std::cout << "start port" << start_port << std::endl;
    const std::uint16_t root_port = start_port + 3;

    const std::array<std::uint16_t, 3> ports{
    start_port,
    static_cast<std::uint16_t>(start_port + 1),
    static_cast<std::uint16_t>(start_port + 2)};


    MachineConfig config0{"agent0", "127.0.0.1", {"agent1"}, ports[0]};
    MachineConfig config1{"agent1", "127.0.0.1", {"agent2"}, ports[1]};
    MachineConfig config2{"agent2", "127.0.0.1", {}, ports[2]};

    std::unordered_map<std::string, MachineConfig> configurations = {{"agent0", config0}, {"agent1", config1}, {"agent2", config2}};
    std::vector<pubtag_t> tags;
    for (size_t i = 0; i < configurations.size(); i++){
        tags.push_back(pubtag_t{"linear_system_job"+std::to_string(i)});
    }
    auto timeout_duration = std::chrono::seconds(15);
    SynchronizingRootNode root_node(configurations, root_port, tags, timeout_duration);
    root_node.run();
}

} // namespace

TEST_CASE("Jacobi Root Node Test", "[mid]")
{
    std::vector<std::thread> threads;
    // Collective threads
    for (auto i=0; i< num_machines; ++i) {
        threads.emplace_back(machine_task, i);
    }
    // Root node thread
    threads.emplace_back(root_machine_task);
    for (auto&& thread : threads) {
        thread.join();
    }
    // Ensure that the agents stopped on the same iteration
    REQUIRE(iter_count0 == iter_count1);
    REQUIRE(iter_count1 == iter_count2);
}
