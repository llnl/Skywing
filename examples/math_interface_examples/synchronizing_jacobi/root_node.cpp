#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>
#include "skywing_core/skywing.hpp"
#include "skywing_math_interface/machine_setup.hpp"
#include "skywing_mid/synchronizing_root_node.hpp"

using namespace skywing;
int main(const int argc, const char* const argv[])
{
    using namespace skywing;
    if (argc < 2) {
        std::cout << "Usage: Not Enough Arguments: " << argc << std::endl;
        return 1;
    }
    // Read in the machine configuration information
    std::string machine_config_file = argv[1];
    std::unordered_map<std::string, MachineConfig> configurations = read_machine_configurations_from_file(machine_config_file);
    std::vector<pubtag_t> tags;
    for (size_t i = 0; i < configurations.size(); i++){
        tags.push_back(pubtag_t{"linear_system_job"+std::to_string(i)}); //Change this to the job IDs of the collective
    }

    // Create and run the root node
    // Since there is only one root node, this is currently hardcoded as opposed to reading from a file 
    // with information for only one agent's port. Since the root node reaches out to the other user agents, only
    // the root node needs to know its port. 
    size_t root_port = 30003;
    auto timeout_duration = std::chrono::seconds(30);
    SynchronizingRootNode root_node(configurations, root_port, tags, timeout_duration);
    root_node.run();
}
