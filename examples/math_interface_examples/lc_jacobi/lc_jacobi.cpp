#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>

#include "skywing_core/enable_logging.hpp"
#include "skywing_core/manager.hpp"
#include "skywing_core/skywing.hpp"
#include "skywing_math_interface/linear_system_driver.hpp"
#include "skywing_mid/asynchronous_iterative.hpp"
#include "skywing_mid/data_input.hpp"
#include "skywing_mid/iteration_policies.hpp"
#include "skywing_mid/linear_system_processors/jacobi_processor.hpp"
#include "skywing_mid/publish_policies.hpp"

using index_t = uint32_t;
using scalar_t = double;

using namespace skywing;

/* This main function is for running on LC. To do so,
   use the run.py script.
*/
int main(const int argc, const char* const argv[])

{
    // Explicitly disable logging as the output is too noisy otherwise
    SKYWING_SET_LOG_LEVEL_TO_WARN();

    if (argc != 5) {
        std::cerr
            << "Usage:\n"
            << argv[0]
            << " config_file slurm_nodeid slurm_localid agents_per_node \n"
            << std::endl;
        return 1;
    }

    std::string machine_config_file = argv[1];
    unsigned slurm_nodeid = std::stoi(argv[2]);
    unsigned slurm_localid = std::stoi(argv[3]);
    unsigned agents_per_node = std::stoi(argv[4]);
    // Calculate the agent ID
    unsigned agent_id = agents_per_node * slurm_nodeid + slurm_localid;

    using MyJacobiProcessor = JacobiProcessor<uint32_t, double>;
    // Set the processor, stop, publish, and resilience policies
    using MyJacobiDriver = LinearSystemDriver<MyJacobiProcessor,
                                              AlwaysPublish,
                                              IterateUntilTime,
                                              TrivialResiliencePolicy>;

    std::string datadirectory = "../jacobi_data";

    std::chrono::seconds timeout = std::chrono::seconds(10);

    // This associates with each machine name a MachineConfig struct
    // A MachineConfig struct stores that machine's name, port, address, and its
    // neighbors' machine names
    std::unordered_map<std::string, MachineConfig> configurations =
        read_machine_configurations_from_file(machine_config_file);

    // Get the current working directory
    std::filesystem::path current_path = std::filesystem::current_path();

    // Convert the path to a string if needed
    std::string outputdirectory = current_path.string();

    MyJacobiDriver driver(configurations,
                          agent_id,
                          datadirectory,
                          outputdirectory,
                          timeout);

    driver.solve();
}
