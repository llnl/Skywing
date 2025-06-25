#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "skywing_core/manager.hpp"
#include "skywing_core/enable_logging.hpp"
#include "skywing_core/skywing.hpp"
#include "skywing_mid/push_sum_basic_processor.hpp"
#include "skywing_mid/asynchronous_iterative.hpp"
#include "skywing_mid/publish_policies.hpp"
#include "skywing_mid/iteration_policies.hpp"
#include "skywing_mid/data_input.hpp"
#include "skywing_mid/power_method_processor.hpp"
#include "skywing_mid/push_sum_processor.hpp"
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
            std::string pubTagID,
            std::vector<std::string> subTagIDs)
{
    std::cout << "Agent " << config.name << " is listening on port "
              << config.port << std::endl;
    skywing::Manager manager(config.port, config.name);
    manager.submit_job(
        "job", [&](skywing::Job& job, skywing::ManagerHandle managerHandle) {
            std::cout << "Agent " << agent_id << " beginning the job."
                      << std::endl;

            // standard connectivity boilerplate, should get a convenience
            // function
            for (const auto& serverMachineName : config.serverMachineNames) {
                const auto serverMachineNameIter =
                    machines.find(serverMachineName);
                if (serverMachineNameIter == machines.cend()) {
                    std::cerr << "Could not find machine \""
                              << serverMachineName << "\" to connect to.\n";
                }
                const auto timeLimit =
                    std::chrono::steady_clock::now() + std::chrono::seconds{30};
                while (
                    !managerHandle
                         .connect_to_server(serverMachineNameIter->second
                                                .remoteAddress.c_str(),
                                            serverMachineNameIter->second.port)
                         .get())
                {
                    if (std::chrono::steady_clock::now() > timeLimit) {
                        std::cerr << config.name
                                  << ": Took too long to connect to "
                                  << serverMachineNameIter->second.remoteAddress
                                  << ":" << serverMachineNameIter->second.port
                                  << '\n';
                        return;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds{10});
                }
            }

            std::cout << "Machine " << config.name << " finished connecting."
                      << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds{4});

        int size_of_system = 4;
        int i = agent_id;

        // make gossip connections in a circle
        auto wrap_ind = [&](int ind) {
                return (ind % size_of_system + size_of_system) % size_of_system;
            };
            std::vector<std::string> tagIDs_for_sub{
                subTagIDs[wrap_ind(i - 1)], subTagIDs[i], subTagIDs[wrap_ind(i + 1)]};


        using PushSumBasicAverage = PushSumBasicProcessor<double,double>; 
        using IterMethod = AsynchronousIterative<PushSumBasicAverage,
                                                     AlwaysPublish,
                                                     IterateUntilTime,
                                                     TrivialResiliencePolicy>;

        double starting_value = (agent_id+1)*1.0; 
        std::cout<<"machine number = "<<agent_id<<" "<<pubTagID<<"\ttags \t"; 
        for(auto tag: tagIDs_for_sub){
            std::cout<<tag<<"\t"; 

        }
        std::cout<<"\n"; 
        std::cout<<"starting_value = "<<starting_value<<std::endl;
        // std::string machine_name = "agent"+std::string(agent_id); 
        Waiter<IterMethod> iter_waiter =
            WaiterBuilder<IterMethod>(managerHandle, job, pubTagID, tagIDs_for_sub)
            .set_processor(starting_value,pubTagID)
            .set_publish_policy()
            .set_iteration_policy(std::chrono::seconds(60))
            .set_resilience_policy()
            .build_waiter();

        IterMethod sum_basic = iter_waiter.get();
        sum_basic.run([&](const decltype(sum_basic)& p) {
                std::cout << p.run_time().count() << "ms: Machine "
                          << agent_id << " has value "
                          << p.get_processor().get_value() << std::endl;
            });                                        

        });
    manager.run();
}
std::vector<std::string> obtain_tag_ids(int size_of_system)
{
    std::vector<std::string> tags;
    for (int i = 0; i < size_of_system; i++) {
        std::string hold = "push_sum_tag" + std::to_string(i);
        tags.push_back(hold);
    }
    return tags;
}

int main(const int argc, const char* const argv[])
{
    std::cout<<"IN CPP"<<std::endl;
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
              << " agent id = "<<agent_id<< std::endl;
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
    std::vector<std::string> subTagIDs = obtain_tag_ids(num_total_agents);

    std::string pubTagID = "push_sum_tag" + std::to_string(agent_id); 

    runJob(configIter->second, configurations, agent_id, pubTagID,subTagIDs);
}
