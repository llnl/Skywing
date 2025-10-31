#ifndef ADMM_PROCESSOR_HPP
#define ADMM_PROCESSOR_HPP

#include <algorithm>
#include <cmath>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "skywing_mid/associative_vector.hpp"
#include "skywing_mid/associative_matrix.hpp"
#include "skywing_mid/big_float.hpp"
#include "skywing_mid/push_flow_processor.hpp"
#include "skywing_mid/quacc_processor.hpp"
#include "skywing_mid/sum_processor.hpp"
#include <Eigen/Dense>
#include <Eigen/QR>

namespace skywing
{

/** @brief Processor for the decentralized Alternating Direction Method of
 *  Multipliers (ADMM) for solving a linear least squares optimization problem
 *  in a row-partitioned setting, that is, each agent owns the entire solution
 *  (or at least the dofs required for a local matvec) and a partitioning of
 *  the right-hand side (correponding to the owned rows of the matrix, A).
 *
 *  This ADMM implementation is for solving the least squares optimization
 *  problem: min f(x) where f(x) = (1/2) ||Ax - b||_2^2. Specifically, a
 *  decentralized consensus least squares problem:
 *  min sum{i=1}^L (1/2) ||A_ix - b_i||_2^2
 *
 *  This processor computes a local update continuously by polling its
 *  neighbors' values and performing a two step operation on its optimization
 *  variable. The first step involves summation of neighbors' values,
 *  counting the number of neighbors, and computing an inverse. The second
 *  step involves a summation and a counting the number of neighbors.
 *
 *  The two-step update phase for the next iterate k+1 for agent i is
 *  as follows:
 *  * Update local x : x_i^{k+1} =
 *    inv(A_i^TA_i + 2c|N_i|I)(A_i^Tb_i + c|N_i|x_i^k + c sum_{j in N_j} x_j^k -
 *        alpha_i^k)
 *  * Update local alpha:
 *      alpha_i^{k+1} = alpha_i^k + c(|N_i|x_i^{k+1} - sum_{j in N_i} x_j^{k+1})
 *
 *  where N_i is the set of neighbors of agent i (excluding i) and c is a
 *  tunable penalty parameter.
 *
 *  The penalty parameter c is a tunable algorithmic parameter between 0 and 1.
 *  Here it is determined based on the connectivity ratio p of the network of
 *  agents based on data-fitting of the best practical value found via
 *  experimentation in W. Shi, Q. Ling, K. Yuan, G. Wu and W. Yin, "On the
 *  Linear Convergence of the ADMM in Decentralized Consensus Optimization," in
 *  IEEE Transactions on Signal Processing, 2014.
 *
 *  @tparam index_t The associative indexing type. Typically, std::size_t or
 *  std::string.
 *  @tparam scalar_t The scalar type.
 */

template <typename index_t, typename scalar_t = double>
class ADMMProcessor
{
public:
    using EigenMatrix = Eigen::MatrixXd;
    using EigenVector = Eigen::VectorXd;

    using OpenVector = AssociativeVector<index_t, scalar_t, true>;
    using ClosedVector = AssociativeVector<index_t, scalar_t, false>;
    using ClosedMatrix = AssociativeMatrix<index_t, scalar_t, false>;

    using ValueType = ClosedVector;
    using IndexType = index_t;
    using ScalarType = scalar_t;

    /**
     * @param A The linear system (assume keys correspond to the rows of A and
     * note that we take the transpose to keep things in column-major format here)
     * @param b Right hand side (assume keys correspond to the rows of A)
     */
    ADMMProcessor(ClosedMatrix A, ClosedVector b)
        : A_(A.transpose()),
          A_keys_(A_.get_keys()),
          x_(A_.get_keys()),
          c_scaled_sum_x_neighbors_(A_.get_keys()),
          alpha_(A_.get_keys()),
          neighbor_count_(A_.get_keys())
    {
        // Convert A from ClosedMatrix to EigenMatrix and
        // b to EigenVector
        size_t num_rows = A_.at(A_.get_keys()[0]).size();
        EigenMatrix eigen_A(num_rows, A_.size());
        EigenVector eigen_b(b.size());
        size_t row_count = 0;
        size_t col_count = 0;
        for (const index_t& col_key : A_keys_) {
            ClosedVector col = A_.at(col_key);
            row_count = 0;
            for (const index_t& row_key : col.get_keys()) {
                eigen_A(row_count, col_count) = col.at(row_key);
                if (col_count == 0)
                    eigen_b(row_count) = b.at(row_key);
                row_count++;
            }
            col_count++;
        }
        eigen_ATb_ = eigen_A.transpose() * eigen_b;
        eigen_ATA_ = eigen_A.transpose() * eigen_A;
    }

    /**
     * @param collective_connectivity_ratio Connectivity ratio of the collective
     * (agent network) used to compute the algorithmic parameter c which impacts
     * convergence rate. The connectivity ratio is the number of connections /
     * number of agents.
     */
    void set_parameters(scalar_t collective_connectivity_ratio = 1.0)
    {
        c_ = compute_c_parameter(collective_connectivity_ratio);
    }

    ValueType get_init_publish_values() { return x_; }

    template <typename NbrDataHandler, typename IterMethod>
    void process_update(const NbrDataHandler& nbr_data_handler,
                        [[maybe_unused]] const IterMethod& iter_method)
    {
        // Get the updated x values from neighbors and compute
        // sum_{j in neighbors of i and j=i} x_j^k and scale by c parameter
        // and count the number of neighbors that contribute to each
        // entry of x_i
        c_scaled_sum_x_neighbors_ = 0.0;
        neighbor_count_ = 0.0;
        for (const auto& pTag : nbr_data_handler.recvd_data_tags()) {
            // Exclude my contribution
            if (pTag == iter_method.my_tag())
                continue;
            ValueType x_j = nbr_data_handler.get_data(pTag);
            c_scaled_sum_x_neighbors_ += x_j;
            // For keys in c_scaled_sum_x_neighbors_, if key also in A_keys_
            // then increment
            for (const index_t& key : x_j.get_keys()) {
                if (std::find(A_keys_.begin(), A_keys_.end(), key)
                    != A_keys_.end())
                {
                    // Only count if my neighbor provided a non-zero
                    // contribution to the sum
                    scalar_t dot_product = x_j.dot(x_j);
                    if (dot_product > 0.0)
                        neighbor_count_[key] += 1.0;
                }
            }
        }
        c_scaled_sum_x_neighbors_ *= c_;

        // ADMM: alpha update
        // alpha_i^(k+1) = alpha_i^k + c(num_neighbors() x_i^(k+1) - \sum_{j in
        // neighbors of i and j = 1} x_j^(k+1))
        tmp = x_;
        for (const index_t& key : A_keys_) {
            tmp[key] *= (c_ * neighbor_count_.at(key));
        }
        tmp -= c_scaled_sum_x_neighbors_;
        alpha_ += tmp;

        // ADMM: x update
        // Solve for x_i^(k+1) in
        // [A_i^TA_i + 2c|N_i|I]*x_i^(k+1) =
        //   A_i^Tb_i + c*|N_i|x_i^k + c * \sum_{j in neighbors of i} x_j^k -
        //   alpha_i^k
        // where |N_i| = num_neighbors() - 1
        // matrix : A_i^TA_i + 2c|N_i|I
        matrix = eigen_ATA_;

        // Add 2c|N_i| to diagonal
        for (long i = 0; i < matrix.rows(); i++) {
            matrix(i, i) += 2.0 * c_ * (neighbor_count_.at(A_keys_[i]));
        }

        // Set operator for Eigen's QR factorization
        solver.compute(matrix);

        // rhs : A_i^Tb_i + c_ * num_neighbors() * x_i +
        // c_scaled_sum_x_neighbors_- alpha_
        rhs_tmp = c_scaled_sum_x_neighbors_;
        rhs_tmp -= alpha_;

        // Compute rhs_tmp += c_ * num_neighbors * x_
        for (const index_t& key : A_keys_) {
            rhs_tmp[key] += c_ * neighbor_count_.at(key) * x_.at(key);
        }

        // Add ClosedVector (rhs_tmp) to EigenVector rhs
        rhs = eigen_ATb_;
        // rhs += rhs_tmp
        for (size_t i = 0; i < A_keys_.size(); ++i) {
            rhs(i) += rhs_tmp.at(A_keys_[i]);
        }

        // Solve matrix * x_ = rhs
        result = solver.solve(rhs);

        // Copy previous x_ iteration
        tmp = x_;
        // Convert EigenVector back to ClosedVector
        for (size_t i = 0; i < A_keys_.size(); ++i) {
            x_[A_keys_[i]] = result(i);
        }

        // Compute x update difference
        tmp -= x_;
        norm_delta_x_ = sqrt(tmp.dot(tmp));
        SKYWING_INFO_LOG("Tag {} - ADMM primal error: ||x^k+1 - x^k|| = {}",
                         iter_method.my_tag().id(),
                         tmp.dot(tmp));
    }

    ValueType prepare_for_publication(ValueType) { return x_; }

    ValueType get_value() const { return x_; }

    std::unordered_map<std::string, scalar_t> get_local_error_metrics() const {
        std::unordered_map<std::string, scalar_t> metrics;
        metrics["norm_delta_x"] = norm_delta_x_;
        return metrics;
    }

    /** @brief Update the right-hand side
     *
     *  This allows for updating the right-hand side, for example,
     *  useful in the case where it corresponds to sensor readings that
     *  may change. This will also zero-out the solution.
     *
     *  @param b Updated right-hand side vector
     */
    void update_rhs(ClosedVector b)
    {
        // Re-convert A from ClosedMatrix to EigenMatrix and
        // b to EigenVector and compute A^Tb
        size_t num_rows = A_.at(A_.get_keys()[0]).size();
        EigenMatrix eigen_A(num_rows, A_.size());
        EigenVector eigen_b(b.size());
        size_t row_count = 0;
        size_t col_count = 0;
        for (const index_t& col_key : A_keys_) {
            ClosedVector col = A_.at(col_key);
            row_count = 0;
            for (const index_t& row_key : col.get_keys()) {
                eigen_A(row_count, col_count) = col.at(row_key);
                if (col_count == 0)
                    eigen_b(row_count) = b.at(row_key);
                row_count++;
            }
            col_count++;
        }

        eigen_ATb_ = eigen_A.transpose() * eigen_b;
        x_ = 0;
        alpha_ = 0;
    }

private:
    const ClosedMatrix A_;
    const std::vector<index_t> A_keys_;

    ClosedVector x_;
    // Captures the sum of all neighbors scaled by c and is
    // open as it may have "extra" entries beyond an agent's x values.
    OpenVector c_scaled_sum_x_neighbors_;

    ClosedVector alpha_;
    ClosedVector neighbor_count_;
    ClosedVector tmp;
    ClosedVector rhs_tmp;

    EigenMatrix eigen_ATA_;
    EigenVector eigen_ATb_;
    EigenMatrix matrix;
    EigenVector rhs;
    EigenVector result;

    scalar_t c_;
    scalar_t norm_delta_x_;
    // Eigen::ColPivHouseholderQR<EigenMatrix> solver;
    Eigen::PartialPivLU<EigenMatrix> solver;

    // Approximates the penalty parameter c using connectivity ratio p
    // This approximation is derived based on data-fitting of the best practical
    // value found via experimentation in W. Shi, Q. Ling, K. Yuan, G. Wu and W.
    // Yin, "On the Linear Convergence of the ADMM in Decentralized Consensus
    // Optimization," in IEEE Transactions on Signal Processing, 2014.
    scalar_t compute_c_parameter(scalar_t p)
    {
        return 0.0002413 * std::pow(p, -2.0156) * exp(2.4256 * p);
    }

}; // class ADMMProcessor

} // namespace skywing

#endif // ADMM_PROCESSOR_HPP
