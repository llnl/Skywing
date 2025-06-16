#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "skywing_core/enable_logging.hpp"
#include "skywing_core/skywing.hpp"

using namespace skywing;

using pubtag_t = Tag<int>;

struct MachineConfig
{
    std::string name;
    std::string remoteAddress;

    std::vector<std::string> serverMachineNames;
    std::uint16_t port;

    template <typename T>
    static void readUntilDash(std::istream& in, std::vector<T>& readInto)
    {
        std::string temp;
        while (std::getline(in, temp)) {
            if (temp.empty()) {
                continue;
            }
            if (!in || temp.front() == '-') {
                break;
            }
            readInto.push_back(std::move(temp));
        }
    }

    friend std::istream& operator>>(std::istream& in, MachineConfig& config)
    {
        std::getline(in, config.name);
        if (!in)
            return in; // Then the file has finished.
        std::getline(in, config.remoteAddress);
        in >> config.port >> std::ws;

        readUntilDash(in, config.serverMachineNames);
        return in;
    }
}; // struct MachineConfig

void runJob(const MachineConfig& config,
            const std::unordered_map<std::string, MachineConfig>& machines,
            unsigned agent_id,
            unsigned num_total_agents)
{
    std::cout << "Agent " << config.name << " is listening on port "
              << config.port << std::endl;
    skywing::Manager manager(config.port, config.name);

    // Configure initial handshake connections between neighboring
    // agents (Note: the actual connections are made later).
    std::vector<std::tuple<std::string, uint16_t>> neighbor_address_port_pairs;
    for (const auto& serverMachineName : config.serverMachineNames) {
        const auto serverMachineNameIter = machines.find(serverMachineName);
        if (serverMachineNameIter == machines.cend()) {
            std::cerr << "Could not find machine \"" << serverMachineName
                      << "\" to initiate connection to.\n";
        }
        neighbor_address_port_pairs.emplace_back(
            serverMachineNameIter->second.remoteAddress.c_str(),
            serverMachineNameIter->second.port);
    }
    // Connecting to the server is an asynchronous operation and can
    // fail. We want to wait for the result and keep attempting to connect
    // until a timeout of 30 seconds.
    manager.configure_initial_neighbors(neighbor_address_port_pairs,
                                        std::chrono::seconds(30));
    manager.submit_job(
        "job", [&](skywing::Job& job, skywing::ManagerHandle managerHandle) {
            std::cout << "Agent " << agent_id << " beginning the job."
                      << std::endl;
            (void) managerHandle;
            std::vector<pubtag_t> tags;
            for (unsigned i = 0; i < num_total_agents; i++)
                tags.push_back(pubtag_t{"tag" + std::to_string(i)});

            // Each agent declares that it intends to publish a stream under a
            // given tag.
            job.declare_publication_intent(tags[agent_id]);

            for (size_t i = agent_id; i < tags.size(); i++)
                job.subscribe(tags[i]).wait();

            // Repeatedly submit and receive values.
            int pub_val = 100 * agent_id;
            for (size_t i = 0; i < 10; i++) {
                // Publish a value.
                job.publish(tags[agent_id], pub_val);
                for (size_t i = agent_id; i < tags.size(); i++) {
                    // Request and wait for a new value under the subscription
                    // specified by `tags[i]`.
                    if (std::optional<int> nbr_val =
                            job.get_waiter(tags[i]).get()) {
                        std::cout << config.name << " received value "
                                  << *nbr_val;
                        std::cout << " from Agent " << i << std::endl;
                    }
                }
                // Increase the value published by this agent and wait a second.
                pub_val++;
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        });
    manager.run();
}

int main(const int argc, const char* const argv[])
{
    // Explicitly disable logging as the output is too noisy otherwise
    SKYWING_SET_LOG_LEVEL_TO_WARN();
    if (argc != 6) {
        std::cerr << "Usage:\n"
                  << argv[0]
                  << " config_file slurm_nodeid slurm_localid agents_per_node "
                     "num_total_agents"
                  << std::endl;
        return 1;
    }

    unsigned slurm_nodeid = std::stoi(argv[2]);
    unsigned slurm_localid = std::stoi(argv[3]);
    unsigned agents_per_node = std::stoi(argv[4]);
    unsigned agent_id = agents_per_node * slurm_nodeid + slurm_localid;
    std::string agent_name = "agent" + std::to_string(agent_id);
    unsigned num_total_agents = std::stoi(argv[5]);

    // Obtain machine configuration for *this* machine
    std::cout << "Agent name " << agent_name << " reading from " << argv[1]
              << std::endl;
    std::ifstream fin(argv[1]);
    if (!fin) {
        std::cerr << "Error opening config file \"" << argv[1] << "\"\n";
        return 1;
    }
    const std::unordered_map<std::string, MachineConfig> configurations =
        [&]() {
            MachineConfig temp;
            std::unordered_map<std::string, MachineConfig> toRet;
            while (fin >> temp) {
                toRet[temp.name] = std::move(temp);
            }
            return toRet;
        }();
    const auto configIter = configurations.find(agent_name);
    if (configIter == configurations.cend()) {
        std::cerr << "Could not find configuration for machine \"" << agent_name
                  << "\"\n";
        return 1;
    }

    runJob(configIter->second, configurations, agent_id, num_total_agents);
}
