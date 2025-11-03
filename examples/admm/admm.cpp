#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "skywing_core/skywing.hpp"
#include "skywing_math_interface/linear_system_driver.hpp"
#include "skywing_mid/linear_system_processors/admm_processor.hpp"
#include "skywing_mid/associative_matrix.hpp"
#include "skywing_mid/associative_vector.hpp"
#include "skywing_mid/asynchronous_iterative.hpp"
#include "skywing_mid/iteration_policies.hpp"
#include "skywing_mid/publish_policies.hpp"

using namespace skywing;

// Type aliases for clarity
using index_t = uint32_t;
using scalar_t = double;
using ClosedVector = AssociativeVector<index_t, scalar_t, false>;
using ClosedMatrix = AssociativeMatrix<index_t, scalar_t, false>;

// Helper functions for the Skywing setup step.
// Utility function: Generate the machine names
std::vector<std::string> obtain_machine_names(std::uint16_t size_of_network)
{
    std::vector<std::string> machine_names;
    machine_names.resize(size_of_network);
    for (int i = 0; i < size_of_network; i++) {
        machine_names[i] = "agent" + std::to_string(i);
    }
    return machine_names;
}

// Utility function: Generate port numbers by incrementing from
// starting_port_number
std::vector<std::uint16_t> set_port(std::uint16_t starting_port_number,
                                    std::uint16_t size_of_network)
{
    std::vector<std::uint16_t> ports;

    for (std::uint16_t i = 0; i < size_of_network; i++) {
        ports.push_back(starting_port_number + (i * 1));
    }
    return ports;
}

// Utility function: Generate subscription tags based on a provided
// communication pattern
std::vector<std::string>
obtain_tag_ids(std::vector<unsigned> agents_to_subscribe)
{
    std::vector<std::string> tag_ids;
    for (auto i : agents_to_subscribe) {
        std::string hold = "tag" + std::to_string(i);
        tag_ids.push_back(hold);
    }
    return tag_ids;
}

void machine_task(size_t machine_number,
                  ClosedMatrix A,
                  ClosedVector b,
                  ClosedVector exact_x,
                  std::vector<std::uint16_t> ports,
                  std::vector<std::string> machine_names,
                  std::vector<std::string> sub_tag_ids)
{
    skywing::Manager manager{ports[machine_number],
                             machine_names[machine_number]};
    // Setup initial gossip connections
    if (machine_number != (ports.size()) - 1) {
        // Connecting to the server is an asynchronous operation and can
        // fail. We use the default time of waiting 10 seconds to connect.
        manager.configure_initial_neighbors("127.0.0.1",
                                            ports[machine_number + 1]);
    }

    manager.submit_job(
        "job", [&](skywing::Job& job, ManagerHandle manager_handle) {
            using IterMethod =
                AsynchronousIterative<ADMMProcessor<index_t, scalar_t>,
                                      AlwaysPublish,
                                      IterateUntilTime,
                                      TrivialResiliencePolicy>;

            Waiter<IterMethod> iter_waiter =
                WaiterBuilder<IterMethod>(manager_handle,
                                          job,
                                          "tag"
                                              + std::to_string(machine_number),
                                          sub_tag_ids)
                    .set_processor(A, b)
                    .set_publish_policy()
                    .set_iteration_policy(std::chrono::seconds(5))
                    .set_resilience_policy()
                    .build_waiter();
            IterMethod async_admm = iter_waiter.get();

            std::cout << "Machine " << machine_number
                      << " about to start ADMM iteration." << std::endl;
            async_admm.run([&](const decltype(async_admm)& p) {
                std::cout << p.run_time().count() << "ms: Machine "
                          << machine_number << " has local solution = ";
                std::cout << p.get_processor().get_value() << std::endl;
            });

            std::cout << "--------------------------------------------"
                      << std::endl;
            std::cout << "Machine " << machine_number
                      << " finished ADMM iteration." << std::endl;
            auto x_local_estimate = async_admm.get_processor().get_value();
            auto diff = x_local_estimate - exact_x;
            std::cout << std::endl;
            std::cout << "\t Local Est: \t";
            std::cout << x_local_estimate << std::endl;
            std::cout << "\t Exact Sol: \t";
            std::cout << exact_x << std::endl;
            std::cout << "\t Rel L2 Error: \t";
            std::cout << diff.dot(diff) / exact_x.dot(exact_x) << std::endl;
            std::cout << "--------------------------------------------\n"
                      << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(15));
        });
    manager.run();
}

// Helper function to set-up the linear least squares problem
// for an IEEE-14 bus example problem. It reads the input communication
// partition, as well as the partitioned linear system from input files
// located in the 14_bus_problem_data directory.
void setup_and_run_14_bus_problem(size_t machine_number,
                                  std::uint16_t starting_port_number)
{
    std::cout << "Setting up 14-bus least squares problem for ADMM..."
              << std::endl;
    // Example assumes 4 agents are running
    size_t size_of_network = 4;
    auto ports = set_port(starting_port_number, size_of_network);
    auto machine_names = obtain_machine_names(size_of_network);

    // Read communication pattern from file then generate subscription tags
    std::string comm_patternfile =
        "14_bus_problem_data/communication_pattern.txt";
    // Check length is 4 to correspond to number of agents
    std::unordered_map<uint32_t, std::vector<unsigned>> comm_pattern =
        readPartition<index_t>(comm_patternfile);
    if (comm_pattern.size() != 4)
        throw std::runtime_error(
            "Incorrect size of 14-bus communication_pattern");
    std::vector<std::string> sub_tag_ids =
        obtain_tag_ids(comm_pattern[machine_number]);

    // Input files with matrix partition information
    std::string col_partitionfile = "14_bus_problem_data/col_partition.txt";
    std::string row_partitionfile = "14_bus_problem_data/row_partition.txt";
    // Input files with linear system information
    std::string rhsfile = "14_bus_problem_data/rhs.txt";
    std::string matrixfile = "14_bus_problem_data/matrix.txt";
    std::string exactsolnfile = "14_bus_problem_data/exact_soln.txt";
    // Read in the partition info for the matrix A
    // Assume each agent has a subset (both portions of rows and cols
    // of the original global matrix A)
    std::unordered_map<uint32_t, std::vector<unsigned>> row_partition =
        readPartition<index_t>(row_partitionfile);
    std::unordered_map<uint32_t, std::vector<unsigned>> col_partition =
        readPartition<index_t>(col_partitionfile);
    if (row_partition.size() != 4 || col_partition.size() != 4)
        throw std::runtime_error("Incorrect size of 14-bus partition");

    std::cout << "Reading in global linear system information..." << std::endl;
    ClosedMatrix A_global =
        ReadAssocitiveMatrix<index_t, scalar_t, false>(matrixfile);
    ClosedMatrix A = ReadAssocitiveMatrix<index_t, scalar_t, false>(
        matrixfile,
        row_partition[machine_number],
        col_partition[machine_number]);
    ClosedVector b_global =
        ReadAssocitiveVector<index_t, scalar_t, false>(rhsfile);
    ClosedVector b = ReadAssocitiveVector<index_t, scalar_t, false>(
        rhsfile, row_partition[machine_number]);
    ClosedVector x_exact_global =
        ReadAssocitiveVector<index_t, scalar_t, false>(exactsolnfile);
    ClosedVector x_exact = ReadAssocitiveVector<index_t, scalar_t, false>(
        exactsolnfile, col_partition[machine_number]);
    // Check expected size of linear system components
    if (A_global.size() != 18 || A_global[0].size() != 14)
        throw std::runtime_error("Incorrect size of 14-bus input matrix");
    if (b_global.size() != 18)
        throw std::runtime_error(
            "Incorrect size of 14-bus input right hand size");
    if (x_exact_global.size() != 14)
        throw std::runtime_error(
            "Incorrect size of 14-bus input exact solution size");

    // Call to Skywing job
    machine_task(machine_number,
                 A,
                 b,
                 x_exact,
                 ports,
                 machine_names,
                 sub_tag_ids);
}

// Helper function to set-up the linear least squares problem
// for determining an unknown signal where x* and the linear system A_i
// is the linear measurement matrix of agent i whose elements follow
// N(0,1) and the measurement vector b_i of agent i is polluted by
// random noise. That is, a randomly generated problem, where true x* is random
// in R^n. Then the matrix A_i ~ N(0,1)^m x n where b_i = A_i x* + e_i where e_i
// ~ N(0, noise*I).
void setup_and_run_random_signal_problem(size_t machine_number,
                                         std::uint16_t starting_port_number,
                                         size_t size_of_network,
                                         size_t observations_per_agent,
                                         size_t state_dim,
                                         double noise)
{
    std::cout << "Setting up random signal least squares problem for ADMM..."
              << std::endl;
    auto ports = set_port(starting_port_number, size_of_network);
    auto machine_names = obtain_machine_names(size_of_network);

    // Assume a line topology communication pattern
    std::vector<unsigned> agents_to_subscribe(1);
    if (machine_number != size_of_network - 1)
        agents_to_subscribe[0] = machine_number + 1;
    auto sub_tag_ids = obtain_tag_ids(agents_to_subscribe);

    // Set-up maps for linear system components
    std::vector<uint32_t> col_keys(state_dim);
    std::vector<uint32_t> row_keys(observations_per_agent);
    std::iota(col_keys.begin(), col_keys.end(), 0);
    std::iota(row_keys.begin(), row_keys.end(), 0);

    std::unordered_map<index_t, ClosedVector> A_map;
    std::unordered_map<index_t, scalar_t> x_map;
    std::unordered_map<index_t, scalar_t> eps_map;

    // Create a random device and seed the generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> dist(0.0, 1.0);

    // Loop over columns (state_dim)
    for (index_t j = 0; j < state_dim; j++) {
        std::unordered_map<index_t, scalar_t> A_col;
        // Loop over rows (observations_per_agent)
        for (index_t i = 0; i < observations_per_agent; i++) {
            A_col[i] = dist(gen);
            if (j == 0)
                eps_map[i] = dist(gen) * noise;
        }
        A_map[j] = A_col;
        x_map[j] = j + 1;
    }
    // Construct Associative data structures
    ClosedMatrix A(A_map);
    ClosedVector x(x_map);
    ClosedVector eps(eps_map);
    // Determine b = A * x + eps
    ClosedVector Ax(std::move(row_keys));
    for (const index_t& col_key : A.get_keys()) {
        ClosedVector col = A.at(col_key);
        for (const index_t& row_key : col.get_keys()) {
            Ax[row_key] += col.at(row_key) * x.at(col_key);
        }
    }
    ClosedVector b(Ax);
    b += eps;

    // Call to Skywing job
    machine_task(machine_number, A.transpose(), b, x, ports, machine_names, sub_tag_ids);
}

int main(int argc, char* argv[])
{
    // Required input parameters: machine number, starting_port_number
    if (argc < 3) {
        std::cout << "Usage: Must pass at least 2 arguments - only passed "
                  << argc << std::endl;
        return 1;
    }

    // Parse the required inputs - machine number, starting_port_number
    size_t machine_number = std::stoi(argv[1]);
    std::uint16_t starting_port_number = std::stoi(argv[2]);

    // Parse additional optional args
    std::string problem = "14-bus";
    // Note: These options are only relevant for the "random-signal" problem
    size_t size_of_network = 4;
    size_t observations_per_agent = 100;
    size_t state_dim = 5;
    double noise = 0.01;

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--problem") {
            problem = argv[++i];
        }
        else if (arg == "--size_of_network") {
            size_of_network = std::stoul(argv[++i]);
        }
        else if (arg == "--observations_per_agent") {
            observations_per_agent = std::stoul(argv[++i]);
        }
        else if (arg == "--state_dim") {
            state_dim = std::stoul(argv[++i]);
        }
        else if (arg == "--noise") {
            noise = std::stod(argv[++i]);
        }
        else {
            std::cerr << "Unknown option: " << arg << "\n";
            return 1;
        }
    }

    if (machine_number > size_of_network - 1) {
        std::cerr << "Invalid machine_number of " << std::quoted(argv[1])
                  << ".\n"
                  << "Must be an integer between 0 and " << size_of_network - 1
                  << '\n';
        return 1;
    }

    // Setup and launch task for this agent
    // The helper functions below set-up the linear least squares
    // problem for the problem of interest, then call `machine_task`
    // to submit the Skywing job and run via the Manager.
    if (problem == "14-bus") {
        try {
            setup_and_run_14_bus_problem(machine_number, starting_port_number);
        }
        catch (const std::runtime_error& e) {
            std::cerr << "Error setting up 14-bus problem: " << e.what()
                      << std::endl;
            return 1;
        }
    }
    else if (problem == "random-signal")
        setup_and_run_random_signal_problem(machine_number,
                                            starting_port_number,
                                            size_of_network,
                                            observations_per_agent,
                                            state_dim,
                                            noise);
    else {
        std::cerr << "Invalid problem name: " << problem << std::endl;
        std::cerr << "Valid options are 14-bus or random-signal" << std::endl;
        return 1;
    }

    return 0;
}
