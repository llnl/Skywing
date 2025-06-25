#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <filesystem>

#include <Eigen/Dense>

#include "skywing_mid/synchronous_iterative.hpp"
#include "skywing_mid/associative_vector.hpp"
#include "skywing_mid/iteration_policies.hpp"
#include "skywing_mid/cola_processor.hpp"

using namespace skywing;

using index_t = int;
using scalar_t = double;
using tag_t = std::string;

using OpenVector = AssociativeVector<index_t, scalar_t, true>;
using ClosedVector = AssociativeVector<index_t, scalar_t, false>;
using AssociativeMatrix = AssociativeVector<index_t, ClosedVector, false>;

void print_usage(const std::string& prog_name)
{
    std::cout << "Usage: " << prog_name << " [agent_num] [options]\n"
              << "\n"
              << "Options:\n"
              << "  -n, --num_agents N      Specify the number of agents (default 3)\n"
              << "  -t, --topology T        Specify the communication topology among agents (default 2)\n"
              << "                             0    : Line\n"
              << "                             1    : Ring\n"
              << "                             else : All-to-all\n"
              << "  -l, --lambda L          Specify the value of the regularization parameter, lambda (default 0.0001)\n"
              << "  -s, --shift_scale S     Choose whether to shift and scale the data (default 0)\n"
              << "  -p, --problem P         Specify the problem to solve (default 2)\n"
              << "                            -1    : Read problem from file (provide filename with -f)\n"
              << "                             0    : Identity matrix\n"
              << "                             1    : Tridiagonal matrix\n"
              << "                             else : Random matrix\n"
              << "  -f, --data_file FILE    Specify data file to read (no default)\n"
              << "  -o, --output_dir DIR    Specify output directory (default output)"
              << "  -h, --help              Display this help message and exit\n";
}

std::tuple<AssociativeMatrix,ClosedVector> read_data(std::string filename)
{
    // Open file
    std::ifstream file(filename + ".csv");
    if (!file.is_open()) {
        throw std::invalid_argument("Data filename not found!");
    }

    std::string line;
    index_t i = 0;
    std::unordered_map<index_t, ClosedVector> A_rows;
    std::unordered_map<index_t, scalar_t> b_rows;

    // Read file and parse values
    while (std::getline(file, line)) {

        // Read comma-separated line
        std::vector<std::string> line_vals;
        std::stringstream ss(line);
        std::string val;
        while (std::getline(ss, val, ',')) {
            line_vals.push_back(val);
        }

        // First column is b, rest are columns of A
        b_rows[i] = (scalar_t) std::stod(line_vals[0]);
        std::unordered_map<index_t, scalar_t> A_row;
        for (size_t j = 1; j < line_vals.size(); j++) {
            A_row[j - 1] = (scalar_t) std::stod(line_vals[j]);
        }
        A_rows[i] = A_row;
        i++;
    }
    file.close();

    // Construct and return Associative data structures for matrix and vector
    AssociativeMatrix A(A_rows);
    ClosedVector b(b_rows);

    return std::tuple<AssociativeMatrix, ClosedVector>(A, b);
}

void shift_scale_matrix(AssociativeMatrix &A)
{
    OpenVector col_mean;
    OpenVector col_scale;

    // Build up the set of keys for x (column indices of A)
    for (const index_t& i : A.get_keys()) {
        for (const index_t& j: A.at(i).get_keys()) {
            col_mean[j] = 0.;
            col_scale[j] = 0.;
        }
    }

    // Get column means and scaling
    for (const index_t& i : A.get_keys()) {
        for (const index_t& j: A.at(i).get_keys()) {
            scalar_t A_ij = A.at(i).at(j);
            col_mean[j] += A_ij;
            col_scale[j] += A_ij * A_ij;
        }
    }
    for (const index_t& j: col_mean.get_keys()) {
        col_mean[j] /= A.size();
        col_scale[j] /= A.size();
        col_scale[j] = sqrt(col_scale[j]);
    }

    // Shift and scale the data matrix if requested
    for (const index_t& i : A.get_keys()) {
        for (const index_t& j: A.at(i).get_keys()) {
            A[i][j] = (A[i][j] - col_mean[j]) / col_scale[j];
        }
    }

}

OpenVector compute_ref_soln(AssociativeMatrix A, ClosedVector b, scalar_t lambda)
{
    // Build up the set of keys for x (column indices of A)
    OpenVector x;
    for (const index_t& i : A.get_keys()) {
       for (const index_t& j: A.at(i).get_keys()) {
           x[j] = 0.;
       }
    }

    // Get the row and column keys for matrix A and their sizes
    auto A_keys = A.get_keys();
    auto x_keys = x.get_keys();
    int d = A.size();
    int n = x_keys.size();

    // Setup mapping from column keys to integer column indices
    std::unordered_map<index_t, int> col_map;
    int j = 0;
    for (const auto& col_key : x_keys) {
        col_map[col_key] = j++;
    }

    // Setup Eigen matrix for the local least-squares problem: M = [A; lambda * I]
    Eigen::MatrixXd M(d + n, n);
    int i = 0;
    for (const auto& row_key : A_keys) {
        ClosedVector row = A.at(row_key);
        for (const auto& col_key : row.get_keys()) {
            M(i, col_map[col_key]) = (double) row[col_key];
        }
        i++;
    }
    for (i = d; i < d + n; i++) {
        M(i, i - d) = (double) lambda;
    }

    // Get QR decomposition of M
    Eigen::ColPivHouseholderQR<Eigen::MatrixXd> qr;
    qr.compute(M);

    if (qr.rank() < n){
        std::cout << "WARNING: data matrix is rank deficient!" << std::endl;
    }

    // Setup rhs
    Eigen::VectorXd rhs(d + n);
    i = 0;
    for (const auto& row_key : A.get_keys()) {
        rhs(i++) = b[row_key];
    }
    for ([[maybe_unused]] const auto& key : x.get_keys()) {
        rhs(i++) = 0.0;
    }

    // Solve with QR
    Eigen::VectorXd soln = qr.solve(rhs);

    // Copy soln back into AssociativeVector
    j = 0;
    for (const auto& col_key : x_keys) {
        x[col_key] = soln[ col_map[col_key] ];
    }

    return x;
}

scalar_t compute_ref_suboptimality(AssociativeMatrix A, ClosedVector b, OpenVector x, scalar_t lambda)
{
    // ||b - Ax||^2 + lambda * ||x||^2
    scalar_t err = 0.0;
    for (const index_t& row_key : A.get_keys()) {
        ClosedVector row = A.at(row_key);
        err += b.at(row_key) - row.dot(x);
    }
    err += lambda * x.dot(x);
    return err;
}

scalar_t compute_ref_duality_gap(AssociativeMatrix A, ClosedVector b, OpenVector x, scalar_t lambda)
{
    OpenVector Ax_open;
    for (const index_t& row_key : A.get_keys()) {
        ClosedVector row = A.at(row_key);
        Ax_open[row_key] += row.dot(x);
    }
    ClosedVector Ax(Ax_open);
    ClosedVector Ax_minux_b = Ax - b;
    scalar_t duality_gap = Ax_minux_b.dot(Ax_minux_b)
                           + Ax_minux_b.dot(b)
                           + (lambda / 2.0) * x.dot(x);
    for (const auto& row_key : A.get_keys()) {
        for (const auto& col_key : A.at(row_key).get_keys()) {
            duality_gap += (1.0 / (2.0 * lambda)) * A.at(row_key).at(col_key) * Ax_minux_b.at(row_key);
        }
    }
    return duality_gap;
}

std::vector<ClosedVector> split_ref_soln(std::vector<AssociativeMatrix> A_k, OpenVector x)
{
    std::vector<ClosedVector> x_k;
    for (const auto& A_k_mat : A_k) {
        std::unordered_map<index_t,scalar_t> local_x;
        for (const auto& row_key : A_k_mat.get_keys()) {
            ClosedVector A_k_row = A_k_mat.at(row_key);
            for (const auto& col_key : A_k_row.get_keys()) {
                local_x[col_key] = x.at(col_key);
            }
        }
        x_k.push_back( ClosedVector(local_x) );
    }
    return x_k;
}

std::tuple<AssociativeMatrix,ClosedVector> generate_data(int problem, index_t n, index_t d)
{
    std::unordered_map<index_t, ClosedVector> A_rows;
    std::unordered_map<index_t, scalar_t> b_rows;
    for (index_t i = 0; i < d; i++) {
        std::unordered_map<index_t, scalar_t> A_row;
        for (index_t j = 0; j < n; j++) {

            // Identity matrix
            if (problem == 0)
                if (i == j) A_row[j] = 1.0;
                else A_row[j] = 0.0;
            
            // Tridiagonal
            else if (problem == 1)
            {
                if (i == j) A_row[j] = 2.0;
                else if (i == j + 1) A_row[j] = -1.0;
                else if (i == j - 1) A_row[j] = -1.0;
                else A_row[j] = 0.0;
            }

            // Random matrix
                else
            A_row[j] = rand() % 100 + 1;
        }
        A_rows[i] = A_row;

        // Constant b
        b_rows[i] = 1.0;
    }

    // Construct and return Associative data structures for matrix and vector
    AssociativeMatrix A(A_rows);
    ClosedVector b(b_rows);

    return std::tuple<AssociativeMatrix, ClosedVector>(A, b);
}

AssociativeVector<tag_t, scalar_t, false> generate_mixing_matrix(size_t topology, size_t agent_num, std::vector<tag_t> &tag_ids)
{
    // Note: W_k is indexed by the tag ids
    std::unordered_map<tag_t, scalar_t> W_k_data;

    // Line
    if (topology == 0)
    {
        if (agent_num == 0)
        {
            W_k_data[ tag_ids[agent_num] ] = 0.5;
            W_k_data[ tag_ids[agent_num + 1] ] = 0.5;
        }
        else if (agent_num == tag_ids.size() - 1)
        {
            W_k_data[ tag_ids[agent_num] ] = 0.5;
            W_k_data[ tag_ids[agent_num - 1] ] = 0.5;
        }
        else
        {
            W_k_data[ tag_ids[agent_num] ] = 1.0 / 3.0;
            W_k_data[ tag_ids[agent_num - 1] ] = 1.0 / 3.0;
            W_k_data[ tag_ids[agent_num + 1] ] = 1.0 / 3.0;
        }
    }
    // Ring
    else if (topology == 1)
    {
        if (agent_num == 0)
        {
            W_k_data[ tag_ids[agent_num] ] = 1.0 / 3.0;
            W_k_data[ tag_ids[agent_num + 1] ] = 1.0 / 3.0;
            W_k_data[ tag_ids[tag_ids.size() - 1] ] = 1.0 / 3.0;
        }
        else if (agent_num == tag_ids.size() - 1)
        {
            W_k_data[ tag_ids[agent_num] ] = 1.0 / 3.0;
            W_k_data[ tag_ids[agent_num - 1] ] = 1.0 / 3.0;
            W_k_data[ tag_ids[0] ] = 1.0 / 3.0;
        }
        else
        {
            W_k_data[ tag_ids[agent_num] ] = 1.0 / 3.0;
            W_k_data[ tag_ids[agent_num - 1] ] = 1.0 / 3.0;
            W_k_data[ tag_ids[agent_num + 1] ] = 1.0 / 3.0;
        }
    }
    // All-to-all
    else
    {
        for (const auto& tag_id : tag_ids) {
            W_k_data[tag_id] = 1.0 / tag_ids.size();
        }
    }
    return AssociativeVector<tag_t, scalar_t, false>( W_k_data );
}

std::vector<AssociativeMatrix> split_data(AssociativeMatrix A, size_t num_agents)
{
    // Partition the columns: partition[col_index] = associated processor index
    std::unordered_map<index_t, int> partition; 
    // NOTE: assume there is a 0 key and that the associated matrix row has all col indices)
    auto col_indices = A.at(0).get_keys();
    int num_cols = col_indices.size();
    size_t k = 0;
    for (const index_t& j: col_indices) {
        partition[j] = k * num_agents / num_cols;
        k++;
    }

    // Split A into A_k column partitions for each distributed process
    std::vector<std::unordered_map<index_t, ClosedVector>> A_k_rows(num_agents);
    for (const auto& row_key : A.get_keys()) {
        ClosedVector A_row = A.at(row_key);
        std::vector<std::unordered_map<index_t, scalar_t>> A_k_row(num_agents);
        for (const auto& col_key : A_row.get_keys()) {
            A_k_row[ partition[col_key] ][col_key] = A_row[col_key];
        }
        for (k = 0; k < num_agents; k++) {
            A_k_rows[k][row_key] = ClosedVector(A_k_row[k]);
        }
    }
    std::vector<AssociativeMatrix> A_k;
    for (k = 0; k < num_agents; k++) {
        A_k.push_back( AssociativeMatrix(A_k_rows[k]) );
    }

    return A_k;
}

int drive_COLA(size_t       agent_num,
               size_t       num_agents,
               size_t       topology,
               scalar_t     lambda,
               bool         shift_scale,
               int          problem,
               std::string  data_file,
               std::string  output_dir)
{
    // Setup ports and tag IDs
    size_t starting_port = 20000;
    std::vector<size_t> ports;
    std::vector<tag_t> tag_ids;
    for (size_t i = 0; i < num_agents; i++)
    {
        ports.push_back(starting_port + i);
        tag_ids.push_back(std::string("tag") + std::to_string(i));
    }

    // Get the global data
    std::tuple<AssociativeMatrix,ClosedVector> data;
    if (problem == -1)
    {
        data = read_data(data_file);
    }
    else
    {
        index_t n = 3 * num_agents; // global number of columns (dofs)
        index_t d = n;
        data = generate_data(problem, n, d);
    }

    AssociativeMatrix A = std::get<0>(data);
    ClosedVector b = std::get<1>(data);
    
    auto A_k = split_data(A, num_agents);
    auto W_k = generate_mixing_matrix(topology, agent_num, tag_ids);

    // Compute a reference, centralized solution (for checking convergence)
    if (shift_scale) {
        shift_scale_matrix(A);
    }
    OpenVector ref_soln = compute_ref_soln(A, b, lambda);
    std::vector<ClosedVector> ref_soln_k = split_ref_soln(A_k, ref_soln);
    scalar_t ref_suboptimality = compute_ref_suboptimality(A, b, ref_soln, lambda);
    scalar_t ref_duality_gap = compute_ref_duality_gap(A, b, ref_soln, lambda);
    OpenVector Ax_open;
    for (const index_t& row_key : A.get_keys()) {
        ClosedVector row = A.at(row_key);
        Ax_open[row_key] += row.dot(ref_soln);
    }
    ClosedVector Ax(Ax_open);

    // Establish agent name based on its number.
    std::string agent_name = std::string("agent") + std::to_string(agent_num);

    // Setup output/history files
    std::string output_file_path = output_dir + "/output" + std::to_string(agent_num) + ".txt";
    std::string history_file_path = output_dir + "/history" + std::to_string(agent_num) + ".txt";
    if (!std::filesystem::exists(output_dir)) {
        std::filesystem::create_directories(output_dir);
    }
    std::ofstream output_file(output_file_path, std::ios::trunc);
    std::ofstream history_file(history_file_path, std::ios::trunc);
    output_file.close();
    history_file.close();

    // Dump the global and local problems to file
    std::string problem_file_path = output_dir + "/A_k_" + std::to_string(agent_num) + ".txt";
    std::ofstream problem_file(problem_file_path, std::ios::trunc);
    if (problem_file.is_open()) {
        problem_file << A_k[agent_num] << std::endl;
    }
    problem_file.close();

    problem_file_path = output_dir + "/W_k_" + std::to_string(agent_num) + ".txt";
    problem_file.open(problem_file_path, std::ios::trunc);
    if (problem_file.is_open()) {
        problem_file << W_k << std::endl;
    }
    problem_file.close();

    if (agent_num == 0)
    {
        problem_file_path = output_dir + "/A.txt";
        problem_file.open(problem_file_path, std::ios::trunc);
        if (problem_file.is_open()) {
            problem_file << A << std::endl;
        }
        problem_file.close();

        problem_file_path = output_dir + "/b.txt";
        problem_file.open(problem_file_path, std::ios::trunc);
        if (problem_file.is_open()) {
            problem_file << b << std::endl;
        }
        problem_file.close();
    }

    // Setup the manager, configure initial neighbors, and submit the job
    Manager manager(ports[agent_num], agent_name);
    if (agent_num > 0) {
        manager.configure_initial_neighbors("localhost", ports[agent_num - 1]);
    }
    manager.submit_job(
        "job", [&](skywing::Job& job, ManagerHandle manager_handle) {

            // Setup our iterative method type
            using IterMethod = SynchronousIterative<ProcessorSyncWrapper<COLAProcessor, index_t, scalar_t, tag_t>,
                                                     IterateUntilTime,
                                                     TrivialResiliencePolicy>;

            // Build a waiter via the WaiterBuilder for the iterative method
            Waiter<IterMethod> iter_waiter =
                WaiterBuilder<IterMethod>(
                    manager_handle, job, tag_ids[agent_num], tag_ids)
                    .set_processor(A_k[agent_num], b, W_k, lambda, num_agents, shift_scale)
                    .set_iteration_policy(std::chrono::seconds(20))
                    .set_resilience_policy()
                    .build_waiter();

            // Waiter get() funciton returns the underlying iterative method
            IterMethod cola = iter_waiter.get();

            // Run the iterative method
            cola.run([&](const decltype(cola)& p) {

                // Get the local solution and compare to the reference
                std::stringstream output_string;
                ClosedVector local_solution = p.get_processor().get_local_solution();
                ClosedVector local_v_k = p.get_processor().get_local_v_k();
                output_string << p.run_time().count() << "ms: Machine " << agent_num 
                          << " has local solution = " << local_solution << std::endl;
                output_string << "Reference solution = " << ref_soln_k[agent_num] << std::endl;

                // Compute relative l2 error in solution
                ClosedVector diff = local_solution - ref_soln_k[agent_num];
                scalar_t err = diff.dot(diff) / ref_soln_k[agent_num].dot(ref_soln_k[agent_num]);
                output_string << "Relative l2 err = " << err << std::endl;

                // Get the norm of the change in local solution
                output_string << "Relative norm delta x = " << p.get_processor().get_rel_norm_delta() << std::endl;

                // Print v_k and Ax
                output_string << "Local rhs approximation, v_k = " << local_v_k << std::endl;
                output_string << "Reference Ax = " << Ax << std::endl;

                // Compute relative l2 error in v_k
                diff = Ax - local_v_k;
                err = diff.dot(diff) / Ax.dot(Ax);
                output_string << "Relative error in Ax - v_k = " << err << std::endl;

                // Report local and reference suboptimality
                output_string << "Local suboptimality = " << p.get_processor().compute_suboptimality() << std::endl;
                output_string << "Reference suboptimality = " << ref_suboptimality << std::endl;
                output_string << "Local duality gap = " << p.get_processor().compute_duality_gap() << std::endl;
                output_string << "Reference duality gap = " << ref_duality_gap << std::endl;

                // Print outputs and history info
                std::cout << output_string.str();
                output_file.open(output_file_path, std::ios::app);
                history_file.open(history_file_path, std::ios::app);
                if (output_file.is_open() && history_file.is_open()) {
                    output_file << output_string.str();
                    history_file << local_solution << std::endl;
                }
                output_file.close();
                history_file.close();
            });

            std::this_thread::sleep_for(std::chrono::seconds(10));
        });
    manager.run();

    return 0;
}
