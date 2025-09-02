#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "skywing_core/manager.hpp"
#include "skywing_core/skywing.hpp"
#include "skywing_math_interface/linear_system_driver.hpp"
#include "skywing_math_interface/machine_setup.hpp"
#include "skywing_mid/associative_matrix.hpp"
#include "skywing_mid/associative_vector.hpp"
#include "skywing_mid/asynchronous_iterative.hpp"
#include "skywing_mid/data_input.hpp"
#include "skywing_mid/iteration_policies.hpp"
#include "skywing_mid/linear_system_processors/jacobi_processor.hpp"
#include "skywing_mid/publish_policies.hpp"

using namespace skywing;

// Type aliases for clarity
using index_t = uint32_t;
using scalar_t = double;
using ClosedVector = AssociativeVector<index_t, scalar_t, false>;
using ClosedMatrix = AssociativeMatrix<index_t, scalar_t, false>;

// Utility function: Print machine configurations
void printConfigurations(
    const std::unordered_map<std::string, MachineConfig>& configurations)
{
    for (const auto& pair : configurations) {
        std::cout << "Key: " << pair.first << ", Value: " << pair.second
                  << std::endl;
    }
}

// Utility function: Set machine configurations to a simple line topology
std::unordered_map<std::string, MachineConfig>
setConfigurations(std::uint16_t startingPort, std::uint16_t systemSize)
{
    std::unordered_map<std::string, MachineConfig> configurations;

    for (std::uint16_t i = 0; i < systemSize; ++i) {
        std::string agentName = "agent" + std::to_string(i);
        std::string localIp = "127.0.0.1";
        std::vector<std::string> neighborAgents;

        if (i != systemSize - 1) {
            neighborAgents.push_back("agent" + std::to_string(i + 1));
        }

        configurations[agentName] =
            MachineConfig{agentName,
                          localIp,
                          neighborAgents,
                          static_cast<uint16_t>(startingPort + i)};
    }

    return configurations;
}

// Utility function: Print a map
void printMap(const std::unordered_map<uint32_t, std::vector<uint32_t>>& map)
{
    for (const auto& pair : map) {
        std::cout << "Key: " << pair.first << " -> Values: ";
        for (const auto& value : pair.second) {
            std::cout << value << " ";
        }
        std::cout << std::endl;
    }
}

// Utility function: Check if a file exists
bool fileExists(const std::string& filename)
{
    std::ifstream file(filename);
    return file.is_open();
}

// Solver function
void runSolver(
    const std::unordered_map<std::string, MachineConfig>& configurations,
    unsigned agentId,
    std::string const& A_file,
    std::string const& b_file,
    std::string const& row_partition_file,
    std::string const& col_partition_file,
    std::string const& comm_topology_file,
    const std::string& outputDirectory)
{
    std::chrono::seconds timeout(5);
    using MyJacobiProcessor = JacobiProcessor<uint32_t, double>;
    using MyJacobiDriver = LinearSystemDriver<MyJacobiProcessor,
                                              AlwaysPublish,
                                              IterateUntilTime,
                                              TrivialResiliencePolicy>;

    MyJacobiDriver driver(configurations,
                          agentId,
                          A_file,
                          b_file,
                          row_partition_file,
                          col_partition_file,
                          comm_topology_file,
                          timeout,
                          outputDirectory);
    driver.solve();
}

// Main function
int main(int argc, char* argv[])
{
    // Validate command-line arguments
    if (argc != 6) {
        std::cerr << "Usage: <program> <starting_port> <system_folder> "
                     "<data_folder> <size_of_system>"
                  << std::endl;
        return 1;
    }

    // Parse command-line arguments
    unsigned startingPort = std::stoi(argv[2]);
    std::string systemFolder = argv[3];
    std::string dataFolder = argv[4];
    uint32_t systemSize = std::stoi(argv[5]);

    // File paths
    std::string matrixFile = systemFolder + "/A.txt";
    std::string rhsFile = systemFolder + "/b.txt";
    std::string rowPartitionFile = systemFolder + "/row_partition.txt";
    std::string colPartitionFile = systemFolder + "/col_partition.txt";
    std::string commTopologyFile = systemFolder + "/comm_topology.txt";

    // Check file existence
    if (!fileExists(rhsFile)) {
        std::cerr << "File does not exist: " << rhsFile << std::endl;
        return 1;
    }

    if (!fileExists(matrixFile)) {
        std::cerr << "File does not exist: " << matrixFile << std::endl;
        return 1;
    }

    // Generate configurations
    auto configurations = setConfigurations(startingPort, systemSize);
    printConfigurations(configurations);

    // Launch solver threads
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < systemSize; i++) {
        threads.emplace_back(runSolver,
                             configurations,
                             i,
                             matrixFile,
                             rhsFile,
                             rowPartitionFile,
                             colPartitionFile,
                             commTopologyFile,
                             dataFolder);
    }

    // Join threads
    for (auto& thread : threads) {
        thread.join();
    }

    return 0;
}
