#ifndef EIGEN_HELPERS_HPP
#define EIGEN_HELPERS_HPP

#include <filesystem>
#include <fstream>
#include <numeric>

#include "skywing_mid/associative_matrix.hpp"
#include "skywing_mid/associative_vector.hpp"
#include <Eigen/Dense>
#include <Eigen/Sparse>

using namespace skywing;

/**
 * @brief Converts an Eigen matrix to an AssociativeMatrix.
 *
 * This function takes an Eigen matrix and constructs an AssociativeMatrix,
 * optionally a list of specified row indices (and/or col indices) may be
 * passed to construct an AssociativeMatrix containing only the
 * specified rows / cols, i.e., a subset of the matrix. By default, each row
 * in the AssociativeMatrix is represented as an AssociativeVector.
 *
 * @param matrix The Eigen matrix to convert.
 * @param rowList A vector of index_t indicating the indices of rows to extract.
 * @param colList A vector of index_t indicating the indices of cols to extract.
 * @return An AssociativeMatrix containing the specified rows.
 *
 * @example
 * Eigen::MatrixXd matrix(2, 2);
 * matrix << 1, 8, 2, 5;  # python: [1 8; 2 5]
 * std::vector<index_t> rowList = {0};
 * AssociativeMatrix assocMatrix =
 *     convert_eigen_matrix_to_associative_matrix(matrix, rowList);
 * // converts [0, 8] to an AssociativeMatrix = {0, {{0, 1}, {1, 8}}}
 */
template <typename index_t, typename scalar_t>
inline AssociativeMatrix<index_t, scalar_t, false>
convert_eigen_matrix_to_associative_matrix(Eigen::MatrixXd matrix,
                                           std::vector<index_t> rowList = {},
                                           std::vector<index_t> colList = {})
{
    // Case where rows are represented as AssociativeVectors
    // Create full list of indices (all rows / cols), if none provided
    if (rowList.size() == 0) {
        rowList.resize(matrix.rows());
        std::iota(rowList.begin(), rowList.end(), 0);
    }
    if (colList.size() == 0) {
        colList.resize(matrix.cols());
        std::iota(colList.begin(), colList.end(), 0);
    }
    std::vector<index_t> rowListcopy = rowList;
    AssociativeMatrix<index_t, scalar_t, false> assoc_matrix(
        std::move(rowListcopy));
    for (index_t row : rowList) {
        std::vector<index_t> col_keys = colList;
        AssociativeVector<index_t, scalar_t, false> assoc_cols(
            std::move(col_keys));
        for (index_t col : colList) {
            assoc_cols[col] = matrix(row, col);
        }
        assoc_matrix[row] = assoc_cols;
    }
    return assoc_matrix;
}

/**
 * @brief Converts an Eigen vector to a AssociativeVector.
 *
 * This function extracts elements from an Eigen vector
 * and stores them in an AssociativeVector. Optionally, a list of row indices
 * may be provided to only extract certain elements. This is useful for creating
 * a sparse representation of a vector.
 *
 * @param vector The Eigen vector from which elements are to be extracted.
 * @param rowList (optional) A vector of index_t indicating the indices of
 * elements to extract.
 * @return An AssociativeVector containing the specified elements.
 *
 * @example
 * Eigen::MatrixXd vector(4, 1);
 * vector << 9, 2, 1, 7;
 * std::vector<index_t> rowList = {0, 3};
 * using ClosedVector = AssociativeVector<int, double>;
 * ClosedVector assocVec =
 *     convert_eigen_vector_to_associative_vector(vector, rowList);
 * // assocVec will be {{0, 9}, {3, 7}}, i.e.,  the 0th and 3rd elements of the
 * // vector (with values 9 and 7, respectively) are used to produce the closed
 * // vector
 */
template <typename index_t, typename scalar_t>
inline AssociativeVector<index_t, scalar_t, false>
convert_eigen_vector_to_associative_vector(Eigen::VectorXd vector,
                                           std::vector<index_t> rowList = {})
{
    if (rowList.size() == 0) {
        rowList.resize(vector.size());
        std::iota(rowList.begin(), rowList.end(), 0);
    }
    std::vector<index_t> rowListcopy(rowList);
    AssociativeVector<index_t, scalar_t, false> assoc_vec(
        std::move(rowListcopy));
    for (index_t row : rowList) {
        assoc_vec[row] = vector(row, 0);
    }
    return assoc_vec;
}

#endif // EIGEN_HELPERS_HPP
