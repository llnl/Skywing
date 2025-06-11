#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>
#include <future>
#include <unordered_map>
#include <vector>
#include <string>
#include <Eigen/Dense>

#include "skywing_core/skywing.hpp"
#include "skywing_mid/linear_system_processors/jacobi_processors/jacobi_processor.hpp"
#include "skywing_math_interface/linear_system_driver.hpp"
#include "skywing_math_interface/io/io.hpp"
#include "skywing_math_interface/machine_setup.hpp"
#include "skywing_core/manager.hpp"
#include "skywing_mid/asynchronous_iterative.hpp"
#include "skywing_mid/data_input.hpp"
#include "skywing_mid/stop_policies.hpp"
#include "skywing_mid/publish_policies.hpp"

using namespace skywing;

// Type aliases for clarity
using index_t = uint32_t;
using scalar_t = double;
using ClosedVector = AssociativeVector<index_t, scalar_t, false>;
using AssociativeMatrix = AssociativeVector<index_t, ClosedVector, false>;

// Utility function: Print machine configurations
void printConfigurations(const std::unordered_map<std::string, MachineConfig>& configurations) {
    for (const auto& pair : configurations) {
        std::cout << "Key: " << pair.first << ", Value: " << pair.second << std::endl;
    }
}

// Utility function: Generate port numbers
std::vector<std::uint16_t> generatePorts(std::uint16_t startingPort, std::uint16_t systemSize) {
    std::vector<std::uint16_t> ports(systemSize);
    for (std::uint16_t i = 0; i < systemSize; ++i) {
        ports[i] = startingPort + i;
    }
    return ports;
}

// Utility function: Set machine configurations based on the Laplacian matrix
std::unordered_map<std::string, MachineConfig> setConfigurations(
    const Eigen::MatrixXd& laplacianMatrix, std::uint16_t startingPort, std::uint16_t systemSize) {
    
    std::unordered_map<std::string, MachineConfig> configurations;

    for (std::uint16_t i = 0; i < systemSize; ++i) {
        std::string agentName = "agent" + std::to_string(i);
        std::string localIp = "127.0.0.1";
        std::vector<std::string> neighborAgents;

        // Identify neighbors based on non-zero entries in the Laplacian matrix
        for (int col = 0; col < laplacianMatrix.cols(); ++col) {
            if (laplacianMatrix(i, col) != 0 && i != col) {
                neighborAgents.push_back("agent" + std::to_string(col));
            }
        }

        configurations[agentName] = MachineConfig{agentName, localIp, neighborAgents, static_cast<uint16_t>(startingPort + i)};
    }

    return configurations;
}

// Utility function: Print a map
void printMap(const std::unordered_map<uint32_t, std::vector<uint32_t>>& map) {
    for (const auto& pair : map) {
        std::cout << "Key: " << pair.first << " -> Values: ";
        for (const auto& value : pair.second) {
            std::cout << value << " ";
        }
        std::cout << std::endl;
    }
}

// Utility function: Check if a file exists
bool fileExists(const std::string& filename) {
    std::ifstream file(filename);
    return file.is_open();
}

// Solver function
void runSolver(
    const std::unordered_map<std::string, MachineConfig>& configurations,
    unsigned agentId,
    const AssociativeMatrix& A,
    const ClosedVector& b,
    std::unordered_map<uint32_t, std::vector<uint32_t>> partition,
    const std::string& outputDirectory) {
    
    std::chrono::seconds timeout(1);
    using MyJacobiProcessor = JacobiProcessor<uint32_t, double>;
    using MyJacobiDriver = LinearSystemDriver<MyJacobiProcessor, AlwaysPublish, StopAfterTime, TrivialResiliencePolicy>;

    MyJacobiDriver driver(configurations, agentId, A, b, partition, timeout, outputDirectory);
    driver.solve();
}

// Main function
int main(int argc, char* argv[]) {
    // Validate command-line arguments
    if (argc != 6) {
        std::cerr << "Usage: <program> <starting_port> <system_folder> <data_folder> <size_of_system>" << std::endl;
        return 1;
    }

    // Parse command-line arguments
    unsigned startingPort = std::stoi(argv[2]);
    std::string systemFolder = argv[3];
    std::string dataFolder = argv[4];
    uint32_t systemSize = std::stoi(argv[5]);

    // File paths
    std::string matrixFile = systemFolder + "/laplacian.txt";
    std::string rhsFile = systemFolder + "/rhs.txt";

    // Check file existence
    if (!fileExists(rhsFile)) {
        std::cerr << "File does not exist: " << rhsFile << std::endl;
        return 1;
    }

    if (!fileExists(matrixFile)) {
        std::cerr << "File does not exist: " << matrixFile << std::endl;
        return 1;
    }


    // In this example, we are not using a partition file; instead, each agent is assigned one row of the Laplacian. 
    std::vector<unsigned int> rowList(systemSize);
    for (uint32_t i = 0; i < systemSize; ++i) {
        rowList[i] = i;
    }

    Eigen::MatrixXd laplacianMatrix = readMatrix<Eigen::MatrixXd>(matrixFile, rowList);
    Eigen::VectorXd rhsVector = readVector(rhsFile);

    // Generate ports and configurations
    auto ports = generatePorts(startingPort, systemSize);
    auto configurations = setConfigurations(laplacianMatrix, startingPort, systemSize);
    printConfigurations(configurations);

    // Launch solver threads
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < systemSize; ++i) {
        std::vector<unsigned int> rowList = {static_cast<unsigned int>(i)};
        auto A = convert_eigen_matrix_to_associative_matrix(laplacianMatrix, rowList);
        auto b = convert_eigen_vector_to_associative_vector(rhsVector, rowList);
        std::unordered_map<uint32_t, std::vector<uint32_t>> partition;
        for (uint32_t i = 0; i < systemSize; ++i) {
            partition[i] = {i};
        }
        threads.emplace_back(runSolver, configurations, i, A, b, partition, dataFolder);
    }

    // Join threads
    for (auto& thread : threads) {
        thread.join();
    }

    return 0;
}