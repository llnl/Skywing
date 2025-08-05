#pragma once

#include <fstream>
#include <memory>

#include "skywing_mid/associative_vector.hpp"
#include <Eigen/Dense>
#include <Eigen/Sparse>
/**
 * @class Matrix
 * @brief A class for handling dense and sparse matrices using Eigen library.
 *
 * This class provides functionality to read matrices from files, perform
 * matrix-vector multiplications, and expose underlying Eigen matrix objects.
 */
class Matrix
{
  std::unique_ptr<Eigen::MatrixXd> dense;
  std::unique_ptr<Eigen::SparseMatrix<double>> sparse;

public:

  /**
  * @brief Default constructor that creates empty matrices.
  */  Matrix()
  {
    dense = std::make_unique<Eigen::MatrixXd>();
    sparse = std::make_unique<Eigen::SparseMatrix<double>>();
  }

  /**
  * @brief Constructor that creates either a dense or sparse matrix based on a file.
  * @param filename The name of the file containing matrix data.
  * @param rowList Optional list of rows to include from the file.
  */
  Matrix(const std::string filename,
    const std::vector<unsigned> rowList = std::vector<unsigned>())
  {
    std::ifstream dataFile(filename);
    std::string header;
    std::getline(dataFile, header);
    if (header == "sparse")
      readSparseMatrix(dataFile, rowList);
    else
    {
      dataFile.seekg(0);
      readDenseMatrix(dataFile, rowList);
    }
  }

  // disable copying
  Matrix(const Matrix& other) = delete;

  Matrix& operator=(const Matrix& other) = delete;

  // enable moving
  Matrix(Matrix&& other)
  {
    dense = std::move(other.dense);
    sparse = std::move(other.sparse);
  }

  Matrix& operator=(Matrix&& other)
  {
    dense = std::move(other.dense);
    sparse = std::move(other.sparse);
    return *this;
  }

  // simple wrapper
  Eigen::Index cols() const
  {
    if (dense) return dense->cols();
    else return sparse->cols();
  }

  // simple wrapper
  Eigen::Index rows() const
  {
    if (dense) return dense->rows();
    else return sparse->rows();
  }

  // simple wrapper
  Eigen::VectorXd row(const Eigen::Index i) const
  {
    if (dense) return dense->row(i);
    else return sparse->row(i);
  }

  /**
  * @brief Multiply the matrix by a vector and scale the result.
  * @param operand The vector to multiply.
  * @param result The result vector.
  * @param coefficient The scaling coefficient.
  */
  void mult(const Eigen::VectorXd& operand, Eigen::VectorXd& result,
    const double coefficient = 1.0) const
  {
    if (dense) result = coefficient * (*dense) * operand;
    else result = coefficient * (*sparse) * operand;
  }

  /**
  * @brief Multiply the transpose of the matrix by a vector and scale the result.
  * @param operand The vector to multiply.
  * @param result The result vector.
  * @param coefficient The scaling coefficient.
  */
  void multTranspose(const Eigen::VectorXd& operand, Eigen::VectorXd& result,
    const double coefficient = 1.0) const
  {
    if (dense) result = coefficient * dense->transpose() * operand;
    else result = coefficient * sparse->transpose() * operand;
  }
  /**
  * @brief Multiply the matrix by a vector, scale the result, and add another vector.
  * @param multOperand The vector to multiply.
  * @param addOperand The vector to add.
  * @param result The result vector.
  * @param coefficient The scaling coefficient.
  * //wrapper for c* A \vec{x} + \vec{y}
  */
  void multAdd(const Eigen::VectorXd& multOperand,
    const Eigen::VectorXd& addOperand, Eigen::VectorXd& result,
    const double coefficient = 1.0) const
  {
    if (dense) result = addOperand + coefficient * (*dense) * multOperand;
    else result = addOperand + coefficient * (*sparse) * multOperand;
  }

  /**
  * @brief Expose the underlying Eigen matrix object.
  * @tparam T The type of the matrix (Eigen::MatrixXd or Eigen::SparseMatrix<double>).
  * @return Reference to the matrix.
  */
  template<typename T> T& as();

private:
  /**
  * @brief Reads a dense matrix from a file.
  *
  * This function reads matrix data from a file stream and populates the dense matrix.
  * If a list of row indices is provided, only those rows are included in the matrix.
  *
  * @param dataFile The input file stream containing matrix data.
  * @param rowList A list of row indices to include in the matrix. If empty, all rows are included.
  */
  void readDenseMatrix(std::ifstream& dataFile,
    const std::vector<unsigned>& rowList)
  {
    std::string matrixRowString;
    std::string matrixEntry;
    std::vector<double> matrixEntries;
    unsigned matrixRowNumber = 0;

    while (std::getline(dataFile, matrixRowString))
    {
      std::stringstream matrixRowStringStream(matrixRowString);
      while (matrixRowStringStream >> matrixEntry)
        matrixEntries.push_back(std::stod(matrixEntry));
      matrixRowNumber++;
    }

    unsigned rows = matrixRowNumber;
    unsigned cols = matrixEntries.size() / rows;
    dense = std::make_unique<Eigen::MatrixXd>(rows, cols);
    for (unsigned i = 0; i < rows; i++)
    {
      for (unsigned j = 0; j < cols; j++)
        (*dense)(i,j) = matrixEntries[cols*i + j];
    }

    if (!rowList.empty())
      *dense = (*dense)(rowList, Eigen::all).eval();
  }

  /**
  * @brief Reads a sparse matrix from a file.
  *
  * This function reads matrix data from a file stream and populates the sparse matrix.
  * The file is expected to have the number of rows and columns on the first line,
  * followed by rows of data with column indices and values.
  * If a list of row indices is provided, only those rows are included in the matrix.
  *
  * @param dataFile The input file stream containing matrix data.
  * @param rowList A list of row indices to include in the matrix. If empty, all rows are included.
  */
  void readSparseMatrix(std::ifstream& dataFile,
    const std::vector<unsigned>& rowList)
  {
    std::string matrixRowString;

    // Number of rows and columns of the matrix on the first line
    unsigned rows, cols;
    std::getline(dataFile, matrixRowString);
    std::stringstream matrixRowStringStream(matrixRowString);
    matrixRowStringStream >> rows >> cols;

    // Each row occupies one line, with
    // the row index in the first number,
    // followed by pairs of column index and value.
    unsigned row_idx, col_idx;
    double value;
    std::vector<Eigen::Triplet<double>> triplets;
    int row_count = 0;
    while (std::getline(dataFile, matrixRowString))
    {
      std::stringstream matrixRowStringStream(matrixRowString);
      matrixRowStringStream >> row_idx;
      if (rowList.empty() || std::any_of(rowList.cbegin(), rowList.cend(),
          [&](unsigned i){return row_idx == i;}))
      {
        while (matrixRowStringStream >> col_idx >> value)
        {
          triplets.push_back(Eigen::Triplet<double>(row_count, col_idx, value));
        }
        row_count++;
      }
    }

    sparse = std::make_unique<Eigen::SparseMatrix<double>>(row_count, cols);
    sparse->setFromTriplets(triplets.begin(), triplets.end());
  }
};

/**
* @brief Specialization of the as function for dense matrices.
*
* This template specialization returns a reference to the underlying dense matrix
* (Eigen::MatrixXd). It asserts that the dense matrix is initialized.
*
* @return Reference to the Eigen::MatrixXd object.
* @throws std::logic_error if the dense matrix is not initialized.
*/
template<>
inline Eigen::MatrixXd& Matrix::as<Eigen::MatrixXd>()
{
  assert(dense);
  return *dense;
}

/**
* @brief Specialization of the as function for sparse matrices.
*
* This template specialization returns a reference to the underlying sparse matrix
* (Eigen::SparseMatrix<double>). It asserts that the sparse matrix is initialized.
*
* @return Reference to the Eigen::SparseMatrix<double> object.
* @throws std::logic_error if the sparse matrix is not initialized.
*/
template<>
inline Eigen::SparseMatrix<double>& Matrix::as<Eigen::SparseMatrix<double>>()
{
  assert(sparse);
  return *sparse;
}
