#ifndef COLA_PROCESSOR_HPP
#define COLA_PROCESSOR_HPP

#include <tuple>
#include <unordered_map>
#include <vector>

#include "skywing_mid/associative_matrix.hpp"
#include "skywing_mid/associative_vector.hpp"
#include "skywing_mid/data_handler.hpp"
#include <Eigen/Dense>

namespace skywing
{
/** @brief Communication-Efficient Decentralized Linear Learning (COLA) for
 * least squares
 *
 * The COLA algorithm is a decentralize optimization algorithm that solves
 * the optimization problem:
 * min_x ( f(Ax) + \sum_i (g_i(x_i)) )
 * where x \in R^n and A is (d x n) data matrix whose columns are distributed
 * among distributed processes. See the following for details on the algorithm:
 * https://arxiv.org/abs/1808.04883
 * This processor implements COLA for a linear least squares objective with
 * Tikhonov regularization:
 * min_x ( ||Ax - b||^2_2 + lambda * ||x||^2 )
 *
 * @tparam index_t The index type, e.g. int.
 *
 * @tparam scalar_t The scalar type for matrix/vector data, e.g. double.
 */
template <typename index_t,
          typename scalar_t = double,
          typename tag_t = std::string>
class COLAProcessor
{
public:
    using OpenVector = AssociativeVector<index_t, scalar_t, true>;
    using ClosedVector = AssociativeVector<index_t, scalar_t, false>;
    using ClosedMatrix = AssociativeVector<index_t, ClosedVector, false>;
    using OpenMatrix = AssociativeVector<index_t, ClosedVector, true>;

    using ValueType = ClosedVector;
    using IndexType = index_t;
    using ScalarType = scalar_t;

    /**
     * @param A_k_ stores the matrix columns associated with this process (note
     * the matrix is still stored row-wise) A_k is (d x n_k), that is d rows
     * (keys) each of length n_k
     * @param b_ is length d rhs vector
     * @param W_k_ is a vector determining neighbor contributions in the
     * communication step
     * @param x_k_ is a vector of lenght n_k with keys corresponding to column
     * indices of A_k_
     * @param delta_x_k_ will serve as an update to x_k_ and has same shape/keys
     * @param v_k_ is a vector of length d with the same keys as A_k_
     */
    COLAProcessor(ClosedMatrix A_k, ClosedVector b)
        : A_k_(A_k), b_(b), v_k_(A_k.get_keys()), v_k_prev_(A_k.get_keys())
    {}

    void set_parameters(AssociativeVector<tag_t, scalar_t, false> W_k,
                        scalar_t lambda,
                        index_t K,
                        bool shift_scale = true)
    {
        // Initialize members
        W_k_ = AssociativeVector<tag_t, scalar_t, false>(W_k), lambda_ = lambda;
        K_ = K;
        shift_scale_ = shift_scale;

        // Build up the set of keys for x (column indices of A)
        for (const index_t& i : A_k_.get_keys()) {
            for (const index_t& j : A_k_.at(i).get_keys()) {
                x_k_[j] = 0.;
                delta_x_k_[j] = 0.;
                if (shift_scale_) {
                    col_mean_[j] = 0.;
                    col_scale_[j] = 0.;
                }
            }
        }

        // Shift and scale the data matrix if requested
        if (shift_scale_) {
            // Get column means and scaling
            for (const index_t& i : A_k_.get_keys()) {
                for (const index_t& j : A_k_.at(i).get_keys()) {
                    scalar_t A_ij = A_k_.at(i).at(j);
                    col_mean_[j] += A_ij;
                    col_scale_[j] += A_ij * A_ij;
                }
            }
            for (const index_t& j : x_k_.get_keys()) {
                col_mean_[j] /= A_k_.size();
                col_scale_[j] /= A_k_.size();
                col_scale_[j] = sqrt(col_scale_[j]);
            }
            for (const index_t& i : A_k_.get_keys()) {
                for (const index_t& j : A_k_.at(i).get_keys()) {
                    A_k_[i][j] = (A_k_[i][j] - col_mean_[j]) / col_scale_[j];
                }
            }
        }

        // Get the row and column keys for matrix A_k_ and their sizes
        auto A_keys = A_k_.get_keys();
        auto x_keys = x_k_.get_keys();
        int d = A_keys.size();
        int n_k = x_keys.size();

        // Setup mapping from column keys to integer column indices
        int j = 0;
        for (const auto& col_key : x_keys) {
            col_map_[col_key] = j++;
        }

        // Setup Eigen matrix for the local least-squares problem: M = [A_k_;
        // lambda_ * I] Setup mapping from row keys to integer row indices
        // WM: todo - use convert_eigen_vector_to_associative_matrix()
        //            need some extra functionality or reorganization to
        //            handle the regularization part of the matrix.
        M_k_.resize(d + n_k, n_k);
        int i = 0;
        for (const auto& row_key : A_keys) {
            ClosedVector row = A_k_.at(row_key);
            for (const auto& col_key : row.get_keys()) {
                M_k_(i, col_map_[col_key]) = (double) row[col_key];
            }
            row_map_[row_key] = i++;
        }
        for (const auto& reg_key : x_keys) {
            M_k_(i, i - d) = (double) lambda_;
            reg_map_[reg_key] = i++;
        }

        // Get QR decomposition of M_k_
        qr_.compute(M_k_);

        if (qr_.rank() < n_k) {
            std::cout << "WARNING: data matrix is rank deficient!" << std::endl;
        }
    }

    ValueType get_init_publish_values() { return v_k_; }

    template <typename IterMethod>
    void process_update(const DataHandler<ValueType>& data_handler,
                        const IterMethod&)
    {
        // Update v_k as average over neighbors: v_k <- /sum_l (W_kl * v_l)
        v_k_ = 0;
        for (const auto& pTag : data_handler.recvd_data_tags()) {
            const ValueType& v_l = data_handler.get_data(pTag);
            v_k_ += W_k_.at(pTag) * v_l;
        }

        solve_subproblem();

        // Update x_k <- x_k + gamma * delta_x_k
        x_k_ += gamma_ * delta_x_k_;
        rel_norm_delta_ =
            sqrt(delta_x_k_.dot(delta_x_k_)) / sqrt(x_k_.dot(x_k_));

        // Update v_k <- v_k + gamma * K * A * delta_x_k
        for (const index_t& row_key : A_k_.get_keys()) {
            ClosedVector row = A_k_.at(row_key);
            v_k_[row_key] += gamma_ * K_ * row.dot(delta_x_k_);
        }
    }

    ValueType prepare_for_publication(ValueType) { return v_k_; }

    ClosedVector get_value() const { return ClosedVector(x_k_); }

    ClosedVector get_local_v_k() const { return v_k_; }

    scalar_t get_rel_norm_delta() const { return rel_norm_delta_; }

    scalar_t compute_suboptimality() const
    {
        scalar_t err = 0.0;
        for (const auto& key : v_k_.get_keys()) {
            scalar_t diff = b_.at(key) - v_k_.at(key);
            err += diff * diff / K_;
        }
        for (const auto& key : x_k_.get_keys()) {
            err += lambda_ * x_k_.at(key) * x_k_.at(key);
        }
        return err;
    }

    scalar_t compute_duality_gap() const
    {
        ClosedVector v_k_minus_b = v_k_ - b_;
        ClosedVector x(x_k_);
        scalar_t duality_gap = (1. / K_) * v_k_minus_b.dot(v_k_minus_b)
                               + v_k_minus_b.dot(b_)
                               + (lambda_ / 2.) * x.dot(x);
        for (const auto& row_key : A_k_.get_keys()) {
            for (const auto& col_key : A_k_.at(row_key).get_keys()) {
                duality_gap += (1. / (2. * lambda_ * K_ * K_))
                               * A_k_.at(row_key).at(col_key)
                               * v_k_minus_b.at(row_key);
            }
        }
        return duality_gap;
    }

private:
    void solve_subproblem()
    {
        // Setup the Eigen vector for the right-hand side of the local
        // least-squares problem: b_ls = (b - v_k)/K
        // WM: todo - write a routine to go from AssociativeVector to Eigen?
        //            need some extra functionality or reorganization to
        //            handle the regularization part.
        Eigen::VectorXd b_ls(M_k_.rows());
        for (const auto& row_key : A_k_.get_keys()) {
            b_ls(row_map_[row_key]) =
                (double) ((b_[row_key] - v_k_[row_key]) / K_);
        }
        for (const auto& reg_key : x_k_.get_keys()) {
            b_ls(reg_map_[reg_key]) = (double) -sqrt(lambda_) * x_k_[reg_key];
        }

        // Use Eigen QR to compute the least squares solution
        Eigen::VectorXd soln = qr_.solve(b_ls);

        // Copy solution values to delta_x_k_
        // WM: todo - use convert_eigen_vector_to_associative_vector()
        for (const auto& key : delta_x_k_.get_keys()) {
            delta_x_k_[key] = soln(col_map_[key]);
        }
    }

    ClosedMatrix A_k_;
    ClosedVector b_;
    Eigen::MatrixXd M_k_;
    Eigen::FullPivHouseholderQR<Eigen::MatrixXd> qr_;
    AssociativeVector<tag_t, scalar_t, false> W_k_;
    OpenVector x_k_;
    OpenVector delta_x_k_;
    OpenVector col_mean_;
    OpenVector col_scale_;
    ClosedVector v_k_;
    ClosedVector v_k_prev_;
    std::unordered_map<index_t, int> col_map_;
    std::unordered_map<index_t, int> row_map_;
    std::unordered_map<index_t, int> reg_map_;
    scalar_t gamma_ = 1.0;
    scalar_t lambda_;
    index_t K_;
    bool shift_scale_;
    scalar_t rel_norm_delta_ = 0.0;

}; // class PushFlowProcessor
} // namespace skywing

#endif // COLA_PROCESSOR_HPP
