#include "cola_driver.hpp"

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cout << "Must pass agent number" << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    // Parse the agent number (required)
    size_t agent_num = std::stoul(argv[1]);

    // Parse additional optional args
    size_t num_agents = 3;
    size_t topology = 2;
    scalar_t lambda = 0.0001;
    bool shift_scale = false;
    int problem = 2;
    std::string data_file;
    std::string output_dir = "output";
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-n" || arg == "--num_agents") {
            num_agents = std::stoul(argv[++i]);
        } else if (arg == "-t" || arg == "--topology") {
            topology = std::stoul(argv[++i]);
        } else if (arg == "-l" || arg == "--lambda") {
            lambda = std::stod(argv[++i]);
        } else if (arg == "-s" || arg == "--shift_scale") {
            shift_scale = std::stoi(argv[++i]);
        } else if (arg == "-p" || arg == "--problem") {
            problem = std::stoi(argv[++i]);
        } else if (arg == "-f" || arg == "--data_file") {
            data_file = argv[++i];
        } else if (arg == "-o" || arg == "--output_dir") {
            output_dir = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    return drive_COLA(agent_num, num_agents, topology, lambda, shift_scale, problem, data_file, output_dir);
}
