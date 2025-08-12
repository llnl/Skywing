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
              << "  -A, --A_file FILE              Specify file to read for "
                 "data matrix (default data/A.csv)\n"
              << "  -b, --b_file FILE              Specify file to read for "
                 "rhs vector (default data/b.csv)\n"
              << "  -r, --row_partition_file FILE  Specify file to read for "
                 "row partition (default data/row_partition.txt)\n"
              << "  -c, --col_partition_file FILE  Specify file to read for "
                 "column partition (default data/col_partition.txt)\n"
              << "  -t, --comm_topology_file FILE  Specify file to read for "
                 "communication topology (default data/comm_topology.txt)\n"
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
    std::string A_file = "data/A.csv";
    std::string b_file = "data/b.csv";
    std::string row_partition_file = "data/row_partition.txt";
    std::string col_partition_file = "data/col_partition.txt";
    std::string comm_topology_file = "data/comm_topology.txt";
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
        else if (arg == "-A" || arg == "--A_file") {
            A_file = argv[++i];
        }
        else if (arg == "-b" || arg == "--b_file") {
            b_file = argv[++i];
        }
        else if (arg == "-r" || arg == "--row_partition_file") {
            row_partition_file = argv[++i];
        }
        else if (arg == "-c" || arg == "--col_partition_file") {
            col_partition_file = argv[++i];
        }
        else if (arg == "-t" || arg == "--comm_topology_file") {
            comm_topology_file = argv[++i];
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
                      A_file,
                      b_file,
                      row_partition_file,
                      col_partition_file,
                      comm_topology_file,
                      output_dir);
}
