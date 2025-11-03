#ifndef SKYWING_MATH_LINEAR_SYSTEM_DRIVER
#define SKYWING_MATH_LINEAR_SYSTEM_DRIVER

#include <chrono>
#include <filesystem>
#include <iostream>

#include "skywing_core/manager.hpp"
#include "skywing_core/skywing.hpp"
#include "skywing_math_interface/machine_setup.hpp"
#include "skywing_mid/associative_matrix.hpp"
#include "skywing_mid/associative_vector.hpp"
#include "skywing_mid/asynchronous_iterative.hpp"
#include "skywing_mid/data_input.hpp"
#include "skywing_mid/iteration_policies.hpp"
#include "skywing_mid/publish_policies.hpp"
#include "skywing_mid/synchronous_iterative.hpp"

namespace skywing
{

/**
 * @brief Reads a partition file and maps machine numbers to assigned indices
 * indices.
 *
 * This function reads a file where each line represents the indices assigned to
 * a specific machine (or agent). It returns a map where the keys are machine
 * numbers and the values are vectors of indices assigned to each machine.
 * The machine numbers are mapped using the line number of the partition.txt
 * file and not the IP address.
 * For example, in the example below we have 3 machines, thus the machine
 * numbers are 0,1 and 2.
 *
 *
 * @param filename The name of the file containing partition data.
 * @return An unordered map from machine numbers to vectors of indices.
 *
 * @example
 * // Given a file with the following content:
 * // 0 1 2 3
 * // 4 5
 * // 6
 * std::unordered_map<uint32_t, std::vector<index_t>> partition =
 *     readPartition("partition.txt");
 * // partition will be {{0, {0, 1, 2, 3}}, {1, {4, 5}}, {2, {6}}}, e.g.,
 * // machine 0 will control indices 0 through 3.
 */
template <typename index_t>
std::unordered_map<uint32_t, std::vector<index_t>>
readPartition(const std::string& filename)
{
    std::ifstream partitionDataFile(filename);
    if (!partitionDataFile.is_open()) {
        std::cerr << "Error: Could not open the file " << filename << std::endl;
    }

    std::string partitionIdxString;
    std::string partitionEntry;
    std::unordered_map<uint32_t, std::vector<index_t>> partition;
    uint32_t key = 0;
    while (std::getline(partitionDataFile, partitionIdxString)) {
        std::vector<index_t> idxList;
        std::stringstream partitionIdxStringStream(partitionIdxString);
        while (partitionIdxStringStream >> partitionEntry)
            idxList.push_back(std::stoi(partitionEntry));
        partition.try_emplace(key, std::move(idxList));
        key += 1;
    }
    return partition;
}

/** @brief Driver code for running a linear system
 *
 * Creates an IterativeMethod with processor type LinearProcessor that ingests a
 * linear system and saves output and history info to file. The linear system
 * info is read from several files containing the matrix, right-hand side
 * vector, row and column partitioning, and communication topology. The matrix
 * may be arbitrarily partitioned among agents according to the partition files
 * and the communication topology among agents may also be arbitrary. See
 * python_helpers/matrix_gen.py or python_helpers/topology_gen.py for example
 * problem generation scripts that construct the necessary input files. Note
 * that the LinearProcessor used here is expected to conform to some standards
 * described in detail in skywing_mid/linear_system_processors/README.md.
 *
 * @tparam LinearProcessor A processor to be used in the IterativeMethod class.
 * @tparam PublishPolicy The policy for when an agent should publish an update
 * to a tag.
 * @tparam IterationPolicy The policy for when this agent should iterate, such
 * as iterating until a certain time has elapsed.
 * @tparam ResiliencePolicy the policy for handling or dropping unresponsive
 * nodes
 */
template <typename LinearProcessor,
          typename PublishPolicy,
          typename IterationPolicy,
          typename ResiliencePolicy>

class LinearSystemDriver
{
public:
    using OpenVector = typename LinearProcessor::OpenVector;
    using ClosedVector = typename LinearProcessor::ClosedVector;
    using ClosedMatrix = typename LinearProcessor::ClosedMatrix;
    using IndexType = typename LinearProcessor::IndexType;
    using ScalarType = typename LinearProcessor::ScalarType;

    LinearSystemDriver(
        std::unordered_map<std::string, MachineConfig>
            configurations, // map from machine names to MachineCongfigs
        unsigned agent_id,
        const std::string& data_directory,
        const std::string& output_directory,
        std::chrono::seconds timeout_duration,
        bool synchronous = false) // output directory location

        : configurations_(configurations),
          agent_id_(agent_id),
          timeout_duration_(timeout_duration),
          output_directory_(output_directory),
          synchronous_(synchronous)
    {
        std::filesystem::path dir = data_directory;
        std::filesystem::path file;

        // Read partitions and communication topology
        std::unordered_map<uint32_t, std::vector<IndexType>> row_partition;
        file = dir / "row_partition.txt";
        if (std::filesystem::exists(file)) {
            row_partition = readPartition<IndexType>(file);
        }
        std::unordered_map<uint32_t, std::vector<IndexType>> col_partition;
        file = dir / "col_partition.txt";
        if (std::filesystem::exists(file)) {
            col_partition = readPartition<IndexType>(file);
        }
        file = dir / "comm_topology.txt";
        std::unordered_map<uint32_t, std::vector<uint32_t>> comm_topology =
            readPartition<uint32_t>(file);

        // Setup publication and subscription tags
        pubTagID_ = "linear_system_tag" + std::to_string(agent_id_);
        for (auto nbr : comm_topology[agent_id_]) {
            tagIDs_for_sub_.push_back("linear_system_tag"
                                      + std::to_string(nbr));
        }

        // Read matrix from file
        file = dir / "A.txt";
        A_ = ReadAssocitiveMatrix<IndexType, ScalarType, false>(
            file, row_partition[agent_id_], col_partition[agent_id_]);

        // Read vector from file
        file = dir / "b.txt";
        b_ = ReadAssocitiveVector<IndexType, ScalarType, false>(
            file, row_partition[agent_id_]);
    }

    /* @brief Skywing Job which subscribes to tags based on the communication
       topology read from file then computes on the linear system using the
       specified processor.
     */
    template <typename... Args>
    void linear_system_job(skywing::Job& job,
                           skywing::ManagerHandle manager_handle,
                           Args... processor_parameters)
    {
        std::cout << "Agent " << agent_id_ << " beginning the job."
                  << std::endl;

        // Callback called on each iteration of the iterative method.
        auto update_fun = [&](std::chrono::milliseconds runtime,
                              LinearProcessor processor) {
            std::cout << runtime.count() << "ms: Machine " << agent_id_
                      << " has value " << processor.get_value() << std::endl;

            std::filesystem::path dir = output_directory_;
            std::filesystem::path file;

            // Open output files
            file = dir / ( "output" + std::to_string(agent_id_) + ".txt" );
            std::ofstream output_file(file, std::ios::trunc); // Use trunc mode to overwrite
            file = dir / ( "history" + std::to_string(agent_id_) + ".txt" );
            std::ofstream history_file(file, std::ios::app); // Append mode to keep history
            file = dir / ( "error_metrics" + std::to_string(agent_id_) + ".txt" );
            std::ofstream error_metrics_file(file, std::ios::app); // Append mode to keep error metrics

            if (output_file.is_open() && history_file.is_open() && error_metrics_file.is_open()) {
                // Write the matrix A_ to the output file
                output_file << "Matrix A_ for Machine " << agent_id_ << ":\n";
                for (auto it = A_.begin(); it != A_.end(); ++it) {
                    unsigned int row = it->first;
                    const auto& columns = it->second;
                    for (auto col_it = columns.begin(); col_it != columns.end();
                         ++col_it)
                    {
                        unsigned int col = col_it->first;
                        double value = col_it->second;
                        output_file << "A_(" << row << ", " << col
                                    << ") = " << value << "\n";
                    }
                }
                output_file << runtime.count() << "ms: Machine " << agent_id_
                            << " has value " << processor.get_value()
                            << std::endl;

                // Write to the history file
                history_file << runtime.count() << "\t" << processor.get_value()
                             << std::endl;

                // Write to the error metrics file
                std::unordered_map<std::string, ScalarType> error_metrics = processor.get_local_error_metrics();
                for (const auto& pair : error_metrics) {
                    error_metrics_file << runtime.count() << "\t" << pair.first << "\t" << pair.second
                                 << std::endl;
                }
            }
            else {
                std::cerr << "Unable to open files for writing." << std::endl;
            }

            // Close the files
            output_file.close();
            history_file.close();
            error_metrics_file.close();
        };

        if (synchronous_) {
            using SyncIterMethod = SynchronousIterative<LinearProcessor,
                                                        IterationPolicy,
                                                        ResiliencePolicy>;

            Waiter<SyncIterMethod> iter_waiter =
                WaiterBuilder<SyncIterMethod>(
                    manager_handle, job, pubTagID_, tagIDs_for_sub_)
                    .set_processor(A_, b_)
                    .set_iteration_policy(
                        timeout_duration_) // stop iterating after this duration
                    .set_resilience_policy()
                    .build_waiter();

            // Callback called on each iteration of the iterative method.
            auto update_fun_wrapper = [&](SyncIterMethod& p) {
                update_fun(p.run_time(), p.get_processor());
            };

            SyncIterMethod linear_system_solver = iter_waiter.get();

            linear_system_solver.get_processor().set_parameters(
                processor_parameters...);
            linear_system_solver.run(update_fun_wrapper);
            iteration_count_ = linear_system_solver.get_iteration_count();
            test_output_ = linear_system_solver.get_processor().get_value();
        }
        else {
            using AsyncIterMethod = AsynchronousIterative<LinearProcessor,
                                                          PublishPolicy,
                                                          IterationPolicy,
                                                          ResiliencePolicy>;
            Waiter<AsyncIterMethod> iter_waiter =
                WaiterBuilder<AsyncIterMethod>(
                    manager_handle, job, pubTagID_, tagIDs_for_sub_)
                    .set_processor(A_, b_)
                    .set_publish_policy()
                    .set_iteration_policy(
                        timeout_duration_) // stop iterating after this duration
                    .set_resilience_policy()
                    .build_waiter();

            // Callback called on each iteration of the iterative method.
            auto update_fun_wrapper = [&](AsyncIterMethod& p) {
                update_fun(p.run_time(), p.get_processor());
            };

            AsyncIterMethod linear_system_solver = iter_waiter.get();

            linear_system_solver.get_processor().set_parameters(
                processor_parameters...);
            linear_system_solver.run(update_fun_wrapper);
            iteration_count_ = linear_system_solver.get_iteration_count();
            test_output_ = linear_system_solver.get_processor().get_value();
        }
    }

    template <typename... Args>
    void solve(Args&&... processor_parameters)
    {
        std::string name = "agent" + std::to_string(agent_id_);
        // look up port number for this agent for printing
        std::uint16_t port = configurations_.at(name).port;
        std::cout << "Agent " << agent_id_ << " is listening on port "
                  << port << std::endl;

        // skywing::Manager manager(port, name);
        manager_ = std::make_unique<skywing::Manager>(
            SocketAddr{configurations_.at(name).remoteAddress, port}, name);

        // use helper function to convert machine configurations object to
        // the address-port pairs object needed by the manager
        std::vector<std::tuple<std::string, uint16_t>>
            neighbor_address_port_pairs =
                create_address_port_pairs_from_machine_configurations(
                    configurations_, name);

        manager_->configure_initial_neighbors(neighbor_address_port_pairs,
                                          std::chrono::seconds(30)); // Set timeout for forming initial connections                                            std::chrono::seconds(30)); //set timeout for forming initial connections

        // define job to run the linear system iterative method
        auto linear_system_lambda = [&](Job& job,
                                        ManagerHandle manager_handle) {
            linear_system_job(job,
                              manager_handle,
                              std::forward<Args>(processor_parameters)...);
        };
        // The first argument to submit_job is the job ID
        manager_->submit_job("linear_system_job" + std::to_string(agent_id_), linear_system_lambda);
        manager_->run();


    }

    ClosedVector test_output() { return test_output_; }

    int iteration_count(){
        return iteration_count_;
    }
    Manager* get_manager() {
        return manager_.get(); // Use .get() to return the raw pointer
    }


private:
    std::unordered_map<std::string, MachineConfig> configurations_;
    unsigned agent_id_;
    ClosedMatrix A_;
    ClosedVector b_;
    ClosedVector test_output_;
    std::unordered_map<uint32_t, std::vector<uint32_t>> partition_;
    int iteration_count_;
    std::chrono::seconds timeout_duration_;
    std::string output_directory_;
    bool synchronous_;
    std::unique_ptr<skywing::Manager> manager_; // Store the Manager instance
    std::string pubTagID_;
    std::vector<std::string> tagIDs_for_sub_;
};

} // namespace skywing
#endif // SKYWING_MATH_LINEAR_SYSTEM_DRIVER
