
#include <stdexcept>

#ifndef SKYWING_MATH_MACHINE_SETUP
#define SKYWING_MATH_MACHINE_SETUP

/** @brief Stores the machine configuration information for a specific machine
 *  
 * name - like agent0, the string used to refer to the machine
 * remoteAddress - either a localaddress or an address such as dane10.llnl.gov
 * serverMachineNames - the names of all neighboring machines, such as [agent1, agent2]
 * port - like 30000 - the port number of the connection endpoint on the machine
 */
 
struct MachineConfig
{
    std::string name;
    std::string remoteAddress;

    std::vector<std::string> serverMachineNames;
    std::uint16_t port;


    /**
     *  The config.cfg file is formatted with dashes seperating agents. This reads all of the 
     *  machine names in the config file until it reaches dashes.
    */
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

    /**
     *  For reading in information to a MachineConfig from a file or other istream.
    */
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

    /**
     *  For printing MachineConfig information for debugging. 
    */
    friend std::ostream& operator<<(std::ostream& os, const MachineConfig& config) {
        os << "Agent Name: " << config.name << ", "
           << "Remote Address: " << config.remoteAddress << ", "
           << "Neighbors: [";
        for (const auto& neighbor : config.serverMachineNames) {
            os << neighbor << " ";
        }
        os << "], Port: " << config.port;
        return os;
    }
}; // struct MachineConfig


/**
 *  Creates a MachineConfig given a configuration file formatted such as:
 * 
    agent0
    dane79.llnl.gov
    30000
    agent1
    ---
    agent1
    dane80.llnl.gov
    30001
    agent2
    ---
    agent2
    dane81.llnl.gov
    30002
    ---
    then this would return
    {agent0, {agent0, dane79.llnl.gov, [agent1], 30000}}
    {agent1, {agent1, dane80.llnl.gov, [agent2], 30001}}
    {agent2, {agent2, dane81.llnl.gov, [], 30002}}

*/
inline std::unordered_map<std::string, MachineConfig>
read_machine_configurations_from_file(std::string filename)
{
    std::ifstream fin(filename);
    if (!fin) {
        throw std::invalid_argument( "Error opening config file");
    }

    MachineConfig temp;
    std::unordered_map<std::string, MachineConfig> configurations;
    while (fin >> temp) 
    {
        configurations[temp.name] = std::move(temp);
    }

    return configurations;
}

/* 
  Given a map of strings to MachineConfigs, returns a vector of address-port pairs for the neighbors of specified machine.
  For example, if the machine config is read in from the example above and agent_name = agent0, this would return
  [{dane80.llnl.gov, 30001}]
  Since agent 1 is the only neighbor of agent0, and agent1 has address dane80.llnl.gov and port 30001.
 */
inline std::vector<std::tuple<std::string, uint16_t>>
create_address_port_pairs_from_machine_configurations(
    std::unordered_map<std::string, MachineConfig> configurations,
    std::string agent_name)
{
    std::vector<std::tuple<std::string, uint16_t>> neighbor_address_port_pairs;
    std::vector<std::string> neighbor_names = configurations.at(agent_name).serverMachineNames;
    for (const auto& neighbor_name : neighbor_names) 
    {
        MachineConfig config = configurations.at(neighbor_name);
        std::tuple address_port_pair = std::make_tuple(config.remoteAddress, config.port);
        neighbor_address_port_pairs.push_back(address_port_pair);
    }
    return neighbor_address_port_pairs;
}

/* 
  Given a map of strings to MachineConfigs, returns a vector of all address-port pairs in the collective.
  For example, given configuration file such as
    agent0
    dane79.llnl.gov
    30000
    agent1
    ---
    agent1
    dane80.llnl.gov
    30001
    agent2
    ---
    agent2
    dane81.llnl.gov
    30002
    ---
    then this would return the vector 
    {{dane79.llnl.gov, 30000},
    {dane80.llnl.gov, 30001},
    {dane81.llnl.gov, 30002}}
 */
inline std::vector<std::tuple<std::string, uint16_t>> collect_all_address_port_pairs_from_machine_configurations(std::unordered_map<std::string, MachineConfig> configurations)
{
    std::vector<std::tuple<std::string, uint16_t>> all_address_port_pairs;
    for (const auto& [machine_name,config] : configurations) 
    {
        std::tuple address_port_pair = std::make_tuple(config.remoteAddress, config.port);
        all_address_port_pairs.push_back(address_port_pair);
    }
    return all_address_port_pairs;
}




#endif // SKYWING_MATH_MACHINE_SETUP
