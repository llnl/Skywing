#include "cola.hpp"

void print_usage(const std::string& prog_name)
{
    std::cout << "Usage: " << prog_name << " [options]\n"
              << "\n"
              << "Options:\n"
              << "  -n, --num_agents N             Specify the number of "
                 "agents (default 3)\n"
              << "  -p, --starting_port P          Specify the starting port "
                 "(default 20000)\n"
              << "  -l, --lambda L                 Specify the value of the "
                 "regularization parameter, lambda (default 0.0001)\n"
              << "  -s, --shift_scale S            Choose whether to shift and "
                 "scale the data (default 0)\n"
              << "  -S, --sync S                   Choose whether to run COLA as "
                 "a synchronous method (default 1)\n"
              << "  -t, --timeout T                Choose the timeout in seconds "
                 "(default 30)\n"
              << "  -d, --data_dir DIR             Specify data directory "
                 "(default data)\n"
              << "  -o, --output_dir DIR           Specify output directory "
                 "(default output)"
              << "  -h, --help                     Display this help message "
                 "and exit\n";
}

int main(int argc, char* argv[])
{
    // Parse additional optional args
    size_t num_agents = 3;
    size_t starting_port = 20000;
    scalar_t lambda = 0.0001;
    bool shift_scale = false;
    bool sync = true;
    size_t timeout = 30;
    std::string data_dir = "data";
    std::string output_dir = "output";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-n" || arg == "--num_agents") {
            num_agents = std::stoul(argv[++i]);
        }
        else if (arg == "-p" || arg == "--starting_port") {
            starting_port = std::stoi(argv[++i]);
        }
        else if (arg == "-l" || arg == "--lambda") {
            lambda = std::stod(argv[++i]);
        }
        else if (arg == "-s" || arg == "--shift_scale") {
            shift_scale = std::stoi(argv[++i]);
        }
        else if (arg == "-S" || arg == "--synchronous") {
            sync = std::stoi(argv[++i]);
        }
        else if (arg == "-t" || arg == "--timeout") {
            timeout = std::stoi(argv[++i]);
        }
        else if (arg == "-d" || arg == "--data_dir") {
            data_dir = argv[++i];
        }
        else if (arg == "-o" || arg == "--output_dir") {
            output_dir = argv[++i];
        }
        else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
        else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    return drive_COLA(starting_port,
                      num_agents,
                      lambda,
                      shift_scale,
                      sync,
                      timeout,
                      data_dir,
                      output_dir);
}
