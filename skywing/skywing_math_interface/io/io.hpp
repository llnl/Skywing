#pragma once

#include "matrix.hpp"
#include "vector.hpp"

#include <filesystem>
#include <fstream>
#include <numeric>

#include "skywing_mid/associative_vector.hpp"
#include <Eigen/Dense>
#include <Eigen/Sparse>

using namespace skywing;

using index_t = uint32_t;
using scalar_t = double;

using ClosedVector = AssociativeVector<index_t, scalar_t, false>;
using AssociativeMatrix = AssociativeVector<index_t, ClosedVector, false>;

/**
 * @brief Converts an Eigen matrix to an AssociativeMatrix.
 *
 * This function takes an Eigen matrix and constructs an AssociativeMatrix,
 * optionally a list of specified row indices (and/or col indices) may be
 * passed to construct an AssociativeMatrix containing only the
 * specified rows / cols, i.e., a subset of the matrix. By default, each row
 * in the AssociativeMatrix is represented as a ClosedVector. By setting
 * row_major = false, a column-major storage scheme will be used so that each
 * column in the AssociativeMatrix is represented as a ClosedVector.
 *
 * @param matrix The Eigen matrix to convert.
 * @param rowList A vector of uints indicating the indices of rows to extract.
 * @param colList A vector of uints indicating the indices of cols to extract.
 * @return An AssociativeMatrix containing the specified rows.
 *
 * @example
 * Eigen::MatrixXd matrix(2, 2);
 * matrix << 1, 8, 2, 5;  # python: [1 8; 2 5]
 * std::vector<unsigned> rowList = {0};
 * AssociativeMatrix assocMatrix =
 *     convert_eigen_matrix_to_associative_matrix(matrix, rowList);
 * // converts [0, 8] to an AssociativeMatrix = {0, {{0, 1}, {1, 8}}}
 */
inline AssociativeMatrix convert_eigen_matrix_to_associative_matrix(Eigen::MatrixXd matrix,
                                           std::vector<unsigned> rowList = {},
                                           std::vector<unsigned> colList = {},
                                           bool row_major = true)
{
    if (row_major) {
        // Case where rows are represented as ClosedVectors
        // Create full list of indices (all rows / cols), if none provided
        if (rowList.size() == 0) {
            rowList.resize(matrix.rows());
            std::iota(rowList.begin(), rowList.end(), 0);
        }
        if (colList.size() == 0) {
            colList.resize(matrix.cols());
            std::iota(colList.begin(), colList.end(), 0);
        }
        std::vector<unsigned> rowListcopy = rowList;
        AssociativeMatrix assoc_matrix(std::move(rowListcopy));
        for (uint32_t row : rowList) {
            std::vector<uint32_t> col_keys = colList;
            ClosedVector assoc_cols(std::move(col_keys));
            for (uint32_t col : colList) {
                assoc_cols[col] = matrix(row, col);
            }
            assoc_matrix[row] = assoc_cols;
        }
        return assoc_matrix;
    }
    else {
        // Case where columns are represented as ClosedVectors
        // Create full list of indices (all rows / cols), if none provided
        if (rowList.size() == 0) {
            rowList.resize(matrix.rows());
            std::iota(rowList.begin(), rowList.end(), 0);
        }
        if (colList.size() == 0) {
            colList.resize(matrix.rows());
            std::iota(colList.begin(), colList.end(), 0);
        }
        std::vector<unsigned> colListcopy = colList;
        AssociativeMatrix assoc_matrix(std::move(colListcopy));
        for (uint32_t col : colList) {
            std::vector<uint32_t> row_keys = rowList;
            ClosedVector assoc_rows(std::move(row_keys));
            for (uint32_t row : rowList) {
                assoc_rows[row] = matrix(row, col);
            }
            assoc_matrix[col] = assoc_rows;
        }
        return assoc_matrix;
    }
}

/**
 * @brief Converts an Eigen vector to a ClosedVector.
 *
 * This function extracts elements from an Eigen vector
 * and stores them in a ClosedVector. Optionally, a list of row indices may be
 * provided to only extract certain elements. This is useful for creating a
 * sparse representation of a vector.
 *
 * @param vector The Eigen vector from which elements are to be extracted.
 * @param rowList (optional) A vector of uints indicating the indices of
 * elements to extract.
 * @return A ClosedVector containing the specified elements.
 *
 * @example
 * Eigen::MatrixXd vector(4, 1);
 * vector << 9, 2, 1, 7;
 * std::vector<unsigned> rowList = {0, 3};
 * ClosedVector assocVec =
 *     convert_eigen_vector_to_associative_vector(vector, rowList);
 * // assocVec will be {{0, 9}, {3, 7}}, i.e.,  the 0th and 3rd elements of the
 * // vector (with values 9 and 7, respectively) are used to produce the closed
 * // vector
 */
inline ClosedVector convert_eigen_vector_to_associative_vector(Eigen::VectorXd vector,
                                           std::vector<unsigned> rowList = {})
{
    if (rowList.size() == 0) {
        rowList.resize(vector.size());
        std::iota(rowList.begin(), rowList.end(), 0);
    }
    std::vector<unsigned> rowListcopy(rowList);
    ClosedVector assoc_vec(std::move(rowListcopy));
    for (uint32_t row : rowList) {
        assoc_vec[row] = vector(row, 0);
    }
    return assoc_vec;
}

/**
 * @brief Reads a partition file and maps machine numbers to assigned row
 * indices.
 *
 * This function reads a file where each line represents the rows assigned to a
 * specific machine (or agent). It returns a map where the keys are machine
 * numbers and the values are vectors of row indices assigned to each machine.
 * The machine numbers are mapped using the line number of the partition.txt
 * file and not the IP address.
 * For example, in the example below we have 3 machines, thus the machine
 * numbers are 0,1 and 2.
 *
 *
 * @param filename The name of the file containing partition data.
 * @return An unordered map from machine numbers to vectors of row indices.
 *
 * @example
 * // Given a file with the following content:
 * // 0 1 2 3
 * // 4 5
 * // 6
 * std::unordered_map<uint32_t, std::vector<unsigned>> partition =
 *     readPartition("partition.txt");
 * // partition will be {{0, {0, 1, 2, 3}}, {1, {4, 5}}, {2, {6}}}, e.g.,
 * // machine 0 will control rows 0 through 3.
 */
inline std::unordered_map<uint32_t, std::vector<unsigned>>
readPartition(const std::string& filename)
{
    std::ifstream partitionDataFile(filename);
    if (!partitionDataFile.is_open()) {
        std::cerr << "Error: Could not open the file " << filename << std::endl;
    }

    std::string partitionRowString;
    std::string partitionEntry;
    std::unordered_map<uint32_t, std::vector<unsigned>> partition;
    uint32_t key = 0;
    while (std::getline(partitionDataFile, partitionRowString)) {
        std::vector<unsigned> rowList;
        std::stringstream partitionRowStringStream(partitionRowString);
        while (partitionRowStringStream >> partitionEntry)
            rowList.push_back(std::stoi(partitionEntry));
        partition.insert({key, rowList});
        key += 1;
    }
    return partition;
}

/**
 * @brief Reads a matrix from a file and returns it as a specified MatrixType.
 *
 * This template function reads a matrix from a file and converts it into the
 * specified MatrixType. It optionally takes a list of row indices to extract.
 *
 * @tparam MatrixType The type of matrix to return (e.g., Eigen::MatrixXd).
 * @param filename The name of the file containing the matrix data.
 * @param rowList Optional vector of row indices to extract.
 * @return The matrix read from the file, converted to MatrixType.
 */
template <class MatrixType>
MatrixType readMatrix(std::string filename,
                      std::vector<unsigned> rowList = std::vector<unsigned>())
{
    return Matrix(filename, rowList).as<MatrixType>();
}

/**
 * @brief Reads a vector from a file and returns it as a Eigen::VectorXd
 *
 * This template function reads a matrix from a file and converts it into a
 * Eigen::VectorXd. It optionally takes a list of row indices to extract.
 *
 * @param filename The name of the file containing the vector data.
 * @param rowList Optional vector of row indices to extract.
 * @return The matrix read from the file, converted to Eigen::VectorXd.
 */

inline Eigen::VectorXd
readVector(std::string filename,
           std::vector<unsigned> rowList = std::vector<unsigned>())
{
    return Vector(filename, rowList).as();
}
