#ifndef ASSOCIATIVE_MATRIX_HPP
#define ASSOCIATIVE_MATRIX_HPP

#include <fstream>
#include <iostream>
#include <sstream>

#include "skywing_mid/associative_vector.hpp"

namespace skywing
{

/**
 * @class AssociativeMatrix
 * @brief An AssociativeVector of AssociativeVector's that may be used to
 * represent a matrix (see associative_vector.hpp).
 *
 * @tparam index_t The type of the keys used for indexing both rows and columns
 * of the matrix. WM: should we allow different types for the row/col keys?
 * @tparam val_t The type of the values stored in the vectors (the entries in
 * the matrix).
 * @tparam isOpen Whether the matrix and underlying vectors are open (allows
 * dynamic key insertion) or closed (fixed keys).
 *
 * This class provides a matrix-like interface with associative access, allowing
 * elements to be accessed and modified using keys. It supports various
 * mathematical operations such as addition, subtraction, scalar multiplication,
 * and dot product.
 */
template <typename index_t = std::uint32_t,
          typename val_t = double,
          bool isOpen = true>
class AssociativeMatrix
    : public AssociativeVector<index_t,
                               AssociativeVector<index_t, val_t, isOpen>,
                               isOpen>
{
public:
    /**
     * Inherit constructors from AssociativeVector
     */
    using AssociativeVector<index_t,
                            AssociativeVector<index_t, val_t, isOpen>,
                            isOpen>::AssociativeVector;

    /**
     * @brief Modifies the value associated with a given row/col key.
     * @param ind The key to access.
     * @param value The value to set the key to.
     */
    void set(index_t row, index_t col, val_t value)
    {
        if constexpr (!isOpen) {
            if (!contains(row))
                throw std::runtime_error(
                    "AssociativeMatrix::set Attempted to set a "
                    "value in a nonexistet row in a closed matrix.");
        }
        this->at(row).set(col, value);
    }

    /**
     * @brief Computes a matrix-vector product A * x for this
     * matrix, A, other AssociativeVectors, x and b, and scalars, alpha and
     * beta.
     * @param x The AssociativeVector, x.
     * @return The matvec product result.
     */
    AssociativeVector<index_t, val_t, isOpen>
    matvec(const AssociativeVector<index_t, val_t, isOpen>& x) const
    {
        AssociativeVector<index_t, val_t, true> b_open;
        for (const index_t& row_key : this->get_keys()) {
            AssociativeVector<index_t, val_t, isOpen> row = this->at(row_key);
            b_open[row_key] += row.dot(x);
        }
        return AssociativeVector<index_t, val_t, isOpen>(b_open);
    }

    /**
     * @brief Transpose the matrix (returns a new matrix).
     */
    AssociativeMatrix<index_t, val_t, isOpen> transpose() const
    {
        // Build up transposed data
        std::unordered_map<index_t, std::unordered_map<index_t, val_t>>
            new_data;
        for (const index_t& row_key : this->get_keys()) {
            AssociativeVector<index_t, val_t, isOpen> row = this->at(row_key);
            for (const index_t& col_key : row.get_keys()) {
                new_data[col_key][row_key] = this->at(row_key).at(col_key);
            }
        }
        // Create the transposed rows
        std::unordered_map<index_t, AssociativeVector<index_t, val_t, isOpen>>
            new_rows;
        for (const auto& new_data_pair : new_data) {
            new_rows[new_data_pair.first] =
                AssociativeVector<index_t, val_t, isOpen>(new_data_pair.second);
        }
        // Finalize the transposed matrix
        return AssociativeMatrix<index_t, val_t, isOpen>(new_rows);
    }
};

/**
 * @brief Read data from file into an AssociativeMatrix.
 *
 * @param filename The filename to read from.
 * @param rowList Optional list of row keys to include (if none is passed, all
 * rows are included).
 * @param colList Optional list of col keys to include (if none is passed, all
 * cols are included).
 * @param read_row_keys Flag indicating whether to read row keys (if false, rows
 * are index by integers from 0).
 * @param read_col_keys Flag indicating whether to read col keys (if false, cols
 * are index by integers from 0).
 * @return The AssociativeMatrix
 *
 * This method reads in data from file with optional row/col keys.
 */
template <typename index_t, typename val_t, bool isOpen>
AssociativeMatrix<index_t, val_t, isOpen> ReadAssocitiveMatrix(
    std::string filename,
    const std::vector<index_t> rowList = std::vector<index_t>(),
    const std::vector<index_t> colList = std::vector<index_t>(),
    bool read_row_keys = false,
    bool read_col_keys = false)
{
    // Open file
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::stringstream ss;
        ss << filename << " file not found!";
        throw std::invalid_argument(ss.str());
    }

    std::string line;
    size_t i, j;
    index_t row_key, col_key;
    size_t row_start = read_row_keys ? 1 : 0;
    std::vector<std::string> col_keys;
    std::string val;
    std::unordered_map<index_t, AssociativeVector<index_t, val_t, isOpen>> map;
    char delimiter = '\0';
    size_t expected_row_size = 0;

    // If column keys are requested, they should comprise the first row of the
    // file
    if (read_col_keys) {
        std::getline(file, line);
        // Check for the delimiter on the first line
        if (delimiter == '\0') {
            delimiter = check_delimiter(line);
        }
        // Read items in the line
        std::stringstream ss(line);
        while (std::getline(ss, val, delimiter)) {
            col_keys.push_back(val);
        }
        // Get the expected row size
        expected_row_size = col_keys.size();
    }

    // Read file and parse values
    i = 0;
    while (std::getline(file, line)) {
        // Check for the delimiter on the first line
        if (delimiter == '\0') {
            delimiter = check_delimiter(line);
        }
        // Read comma-separated line
        std::stringstream ss(line);
        std::vector<std::string> line_vals;
        while (std::getline(ss, val, delimiter)) {
            line_vals.push_back(val);
        }
        // Check for expected row size on the first line
        if (expected_row_size == 0) {
            expected_row_size = line_vals.size();
        }
        // Check that line lengths are consistent
        if (line_vals.size() != expected_row_size) {
            std::stringstream ss;
            ss << "ERROR: wrong number of columns in file " << filename
               << " when reading AssociativeMatrix";
            throw std::runtime_error(ss.str());
        }
        row_key = read_row_keys ? type_cast_helper<index_t>(line_vals[0])
                                : type_cast_helper<index_t>(i++);
        // If the row list is empty, include all rows, otherwise
        // check to see if the row key was in the requested list.
        if (rowList.empty()
            || std::find(rowList.begin(), rowList.end(), row_key)
                   != rowList.end())
        {
            // Fill row of AssociativeMatrix
            std::unordered_map<index_t, val_t> row;
            for (j = row_start; j < line_vals.size(); j++) {
                col_key = read_col_keys ? type_cast_helper<index_t>(col_keys[j])
                                        : type_cast_helper<index_t>(j);
                // If the col list is empty, include all cols, otherwise
                // check to see if the col key was in the requested list.
                if (colList.empty()
                    || std::find(colList.begin(), colList.end(), col_key)
                           != colList.end())
                {
                    row[col_key] = type_cast_helper<val_t>(line_vals[j]);
                }
            }
            map[row_key] = row;
        }
    }
    file.close();

    return AssociativeMatrix<index_t, val_t, isOpen>(map);
}

} // namespace skywing
#endif // ASSOCIATIVE_MATRIX_HPP
