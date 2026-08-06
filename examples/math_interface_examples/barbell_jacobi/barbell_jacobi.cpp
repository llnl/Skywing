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

// Solver function
void runSolver(
    const std::unordered_map<std::string, MachineConfig>& configurations,
    unsigned agentId,
    const std::string& dataDir,
    const std::string& outputDir)
{
    std::chrono::seconds timeout(5);
    using MyJacobiProcessor = JacobiProcessor<uint32_t, double>;
    using MyJacobiDriver = LinearSystemDriver<MyJacobiProcessor,
                                              AlwaysPublish,
                                              IterateUntilTime,
                                              TrivialResiliencePolicy>;

    MyJacobiDriver driver(configurations,
                          agentId,
                          dataDir,
                          outputDir,
                          timeout);
    driver.solve();
}

// Main function
int main(int argc, char* argv[])
{
    // Validate command-line arguments
    if (argc != 6) {
        std::cerr << "Usage: <program> <starting_port> <data_directory> "
                     "<output_directory> <size_of_system>"
                  << std::endl;
        return 1;
    }

    // Parse command-line arguments
    unsigned startingPort = std::stoi(argv[2]);
    std::string dataDir = argv[3];
    std::string outputDir = argv[4];
    uint32_t systemSize = std::stoi(argv[5]);

    // Generate configurations
    auto configurations = create_default_machine_configurations(startingPort, systemSize);

    // Launch solver threads
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < systemSize; i++) {
        threads.emplace_back(runSolver,
                             configurations,
                             i,
                             dataDir,
                             outputDir);
    }

    // Join threads
    for (auto& thread : threads) {
        thread.join();
    }

    return 0;
}
