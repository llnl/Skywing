#ifndef SKYWING_MID_JACOBI_PROCESSOR_HPP
#define SKYWING_MID_JACOBI_PROCESSOR_HPP

#include <iostream>

#include "skywing_mid/associative_matrix.hpp"
#include "skywing_mid/associative_vector.hpp"

namespace skywing
{
/**
 * @brief Processor for the Jacobi iterative method to find a solution x to Ax =
 * b.
 *
 *  The matrix A is stored in a distributed fashion across the
 *  collective. Each agent stores some set of rows of A. The rows and
 *  columns are indexed associatively rather than numerically; as a
 *  result, there is no inherent ordering to the rows and columns of
 *  the matrix.
 */
template <typename index_t, typename scalar_t = double>
class JacobiProcessor
{
public:
    using OpenVector = AssociativeVector<index_t, scalar_t, true>;
    using ClosedVector = AssociativeVector<index_t, scalar_t, false>;
    using ClosedMatrix = AssociativeMatrix<index_t, scalar_t, false>;

    using ValueType = ClosedVector;
    using IndexType = index_t;
    using ScalarType = scalar_t;

    JacobiProcessor(ClosedMatrix A, ClosedVector b)
        : M_(A),
          matrix_keys_(A.get_keys()),
          x_(A.get_keys()),
          x_keys_(A.get_keys()),
          local_x_(A.get_keys()), // defaults to 0, x_0 = 0
          c_(b)
    {
        // Setup preconditioning matrix, M_ = I - D^{-1}A, and vector c =
        // D^{-1}b
        for (const auto& key : A.get_keys()) {
            M_[key] = -A.at(key) / A.at(key).at(key); // this is vector / scalar
            M_[key][key] += 1.0;
            c_[key] = b.at(key) / A.at(key).at(key);
        }
    }

    void set_parameters() {}

    ValueType get_init_publish_values() { return local_x_; }

    template <typename NbrDataHandler, typename IterMethod>
    void process_update(const NbrDataHandler& nbr_data_handler,
                        const IterMethod& iter_method)
    {
        std::string my_id = iter_method.my_tag().id();
        // Neighbor updates
        for (const auto& pTag : nbr_data_handler.recvd_data_tags()) {
            if (pTag == iter_method.my_tag())
                continue;
            const ValueType& nbr_data = nbr_data_handler.get_data(pTag);
            std::vector<index_t> updated_keys = nbr_data.get_keys();
            for (index_t key : updated_keys) {
                x_[key] = nbr_data.at(key);
                x_keys_.push_back(key);
            }
        }
        // Local updates
        std::vector<index_t> matrix_keys_copy = matrix_keys_;
        ClosedVector dx = (std::move(matrix_keys_copy));
        for (const index_t& key : matrix_keys_) {
            ClosedVector row = M_.at(key);
            // dxi = xi^{k+1} - xi^k as dxi = Mij*xj^k + ci - xi^k
            dx[key] = row.dot(x_) + c_.at(key) - x_.at(key);
        }
        // Compute xi^{k+1} = xi^k + dx
        local_x_ += dx;
        x_ += dx;
    }

    ValueType prepare_for_publication(ValueType) { return local_x_; }

    ValueType get_value() const { return local_x_; }

    std::unordered_map<std::string, scalar_t> get_local_error_metrics() const {
        return std::unordered_map<std::string, scalar_t>();
    }

private:
    ClosedMatrix M_;
    const std::vector<index_t> matrix_keys_;

    OpenVector x_; // keys slowly added, starts just with the local keys
    std::vector<index_t> x_keys_;

    ClosedVector local_x_; // a copy of x_ for only matrix_keys_
    ClosedVector c_;
}; // class JacobiProcessor

} // namespace skywing

#endif // SKYWING_MID_JACOBI_PROCESSOR_HPP
