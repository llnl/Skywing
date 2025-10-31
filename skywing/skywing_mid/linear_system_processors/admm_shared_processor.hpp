#ifndef ADMM_SHARED_PROCESSOR_HPP
#define ADMM_SHARED_PROCESSOR_HPP

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
 *  in a column-partitioned setting, that is, each agent owns the entire right-
 *  hand side and a partitioning of the solution (correponding to the owned 
 *  columns of the matrix, A).
 *
 *  This ADMM implementation is for solving the least squares optimization
 *  problem with l2 regularization:
 *  min_x (1/2) ||Ax - b||_2^2 + \lambda ||x||_2^2.
 *  where A and x are partitioned column-wise across decentralized agents.
 *
 *  This processor computes a local update by receiving its neighbors' values
 *  and performing a three-step update over its local variables:
 *
 *  * Communicate A_j x_j^k and update \bar{Ax}^k = (1 / N) sum_{j=1...N} A_j x_j^k
 *  * Update local \bar{z}: \bar{z}^{k+1} =
 *    (1 / (N + \rho))(\rho \bar{Ax}^k + u^k + b)
 *  * Update local u: u^{k+1} = 
 *    u^k + \bar{Ax}^k - \bar{z}^{k+1}
 *  * Update local x : x_i^{k+1} =
 *    inv(\rho A_i^T A_i + \lambda I) \rho A_i^T (A_i x_i^k + \bar{z}^k - \bar{Ax}^k - u^k)
 *  WM: todo - do the algebra to combine \bar{z} and u updates?
 *
 *  where N is the number of agents and \rho is a tunable penalty parameter.
 *  Note that the communication step assumes all-to-all communication!
 *  The algorithm is not expected to work without fully connected communication
 *  topology (and synchronous updates across the collective).
 *  WM: todo - replace all-to-all synchronous communication with a gossip
 *  averaging algorithm (introduces an inner iteration to converge the average).
 *
 *  This implementation is based on the algorithm description in Boyd et. al.'s
 *  Distributed Optimization and Statistical Learning via the Alternating Direction
 *  Method of Multipliers, Section 8.3: Splitting across Features.
 *
 *  @tparam index_t The associative indexing type. Typically, std::size_t or
 *  std::string.
 *  @tparam scalar_t The scalar type.
 */

template <typename index_t, typename scalar_t = double>
class ADMMSharedProcessor
{
public:
    using OpenVector = AssociativeVector<index_t, scalar_t, true>;
    using ClosedVector = AssociativeVector<index_t, scalar_t, false>;
    using ClosedMatrix = AssociativeMatrix<index_t, scalar_t, false>;

    using ValueType = ClosedVector;
    using IndexType = index_t;
    using ScalarType = scalar_t;

    /**
     * @param A The local columns, A_i, of the global data matrix, A
     * @param b The global right hand side, b
     */
    ADMMSharedProcessor(ClosedMatrix A, ClosedVector b)
        : A_i_(A),
          b_(b),
          A_i_T_(A.transpose()),
          bar_z_(b.get_keys()),
          u_(b.get_keys()),
          A_i_x_i_(b.get_keys()),
          bar_A_x_(b.get_keys())
    {}

    void set_parameters(scalar_t rho, scalar_t lambda)
    {
        rho_ = rho;        
        lambda_ = lambda;        

        // Build up the set of keys for x (column indices of A_i)
        OpenVector x_open;
        for (const index_t& i : A_i_.get_keys()) {
            for (const index_t& j : A_i_.at(i).get_keys()) {
                x_open[j] = 0.;
            }
        }
        x_i_ = ClosedVector(x_open);

        // WM: todo - implement a generic method for shift/scale and use here?

        // Setup mapping from column keys to integer column indices
        int j = 0;
        for (const auto& col_key : x_i_.get_keys()) {
            col_map_[col_key] = j++;
        }

        // Converte A_i_ to Eigen
        Eigen::MatrixXd eigen_A(A_i_.size(), x_i_.size());
        int i = 0;
        for (const auto& row_key : A_i_.get_keys()) {
            ClosedVector row = A_i_.at(row_key);
            for (const auto& col_key : row.get_keys()) {
                eigen_A(i, col_map_[col_key]) = (double) row[col_key];
            }
            row_map_[row_key] = i++;
        }

        // Form the matrix (\rho A_i^T A_i + \lambda I) and setup a
        // solver in eigen for taking the inverse
        Eigen::MatrixXd eigen_M = rho_ * eigen_A.transpose() * eigen_A + lambda_ * Eigen::MatrixXd::Identity(x_i_.size(), x_i_.size());
        solver_.compute(eigen_M);
    }

    ValueType get_init_publish_values() { return A_i_x_i_; }

    template <typename NbrDataHandler, typename IterMethod>
    void process_update(const NbrDataHandler& nbr_data_handler,
                        [[maybe_unused]] const IterMethod& iter_method)
    {
        // Communicate A_j x_j^k and update \bar{Ax}^k = (1 / N) sum_{j in N_j} A_j x_j^k
        bar_A_x_ = 0;
        size_t N = 0;
        for (const auto& pTag : nbr_data_handler.recvd_data_tags()) {
            const ValueType A_j_x_j = nbr_data_handler.get_data(pTag);
            bar_A_x_ += A_j_x_j;
            N++;
        }
        bar_A_x_ /= N;

        // Update local \bar{z}: \bar{z}^{k+1} =
        // (1 / (N + \rho))(\rho \bar{Ax}^k + u^k + b)
        bar_z_ = (1.0 / (N + rho_)) * (rho_ * bar_A_x_ + u_ + b_);

        // Update local u: u^{k+1} = 
        // u^k + \bar{Ax}^k - \bar{z}^{k+1}
        u_ += bar_A_x_ - bar_z_;
        
        // Update local x : x_i^{k+1} =
        // inv(\rho A_i^T A_i + \lambda I) \rho A_i^T (A_i x_i^k + \bar{z}^k - \bar{Ax}^k - u^k)
        Eigen::VectorXd rhs(x_i_.size());
        ClosedVector tmp = A_i_x_i_ + bar_z_ - bar_A_x_ - u_;
        ClosedVector A_i_T_tmp = A_i_T_.matvec(tmp);
        A_i_T_tmp *= rho_;
        for (const auto& col_key : x_i_.get_keys()) {
            rhs(col_map_[col_key]) =
                (double) A_i_T_tmp.at(col_key);
        }
        Eigen::VectorXd soln = solver_.solve(rhs);
        for (const auto& col_key : x_i_.get_keys()) {
            x_i_[col_key] = soln(col_map_[col_key]);
        }

        // Get A_i x_i
        A_i_x_i_ = A_i_.matvec(x_i_);
    }

    ValueType prepare_for_publication(ValueType) { return A_i_x_i_; }

    ValueType get_value() const { return x_i_; }

    std::unordered_map<std::string, scalar_t> get_local_error_metrics() const {
        return std::unordered_map<std::string, scalar_t>();
    }

private:
    // Original matrix and right-hand side
    const ClosedMatrix A_i_;
    ClosedVector b_;

    // Local components, x_i, of the solution
    ClosedVector x_i_;

    // Parameters
    scalar_t rho_;
    scalar_t lambda_;

    // Intermediate matricex/vectors used in the local updates and communication
    ClosedMatrix A_i_T_;
    ClosedVector bar_z_;
    ClosedVector u_;
    ClosedVector A_i_x_i_;
    ClosedVector bar_A_x_;

    // Eigen LU solver
    // Eigen::ColPivHouseholderQR<Eigen::MatrixXd> solver_;
    Eigen::PartialPivLU<Eigen::MatrixXd> solver_;

    // Row/column mappings for converting between associative and Eigen data structures
    std::unordered_map<index_t, int> col_map_;
    std::unordered_map<index_t, int> row_map_;


}; // class ADMMSharedProcessor

} // namespace skywing

#endif // ADMM_SHARED_PROCESSOR_HPP
