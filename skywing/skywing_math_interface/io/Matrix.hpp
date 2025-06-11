#pragma once

#include <fstream>
#include <memory>

#include <Eigen/Dense>
#include <Eigen/Sparse>

class Matrix
{
  std::unique_ptr<Eigen::MatrixXd> dense;
  std::unique_ptr<Eigen::SparseMatrix<double>> sparse;

public:

  // default constructor that creates empty matrices
  Matrix()
  {
    dense = std::make_unique<Eigen::MatrixXd>();
    sparse = std::make_unique<Eigen::SparseMatrix<double>>();
  }

  // constructor that creates either dense or sparse matrix based on file
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

  // wrapper for c * A \vec{x}
  void mult(const Eigen::VectorXd& operand, Eigen::VectorXd& result,
    const double coefficient = 1.0) const
  {
    if (dense) result = coefficient * (*dense) * operand;
    else result = coefficient * (*sparse) * operand;
  }

  // wrapper for c * A^T \vec{x}
  void multTranspose(const Eigen::VectorXd& operand, Eigen::VectorXd& result,
    const double coefficient = 1.0) const
  {
    if (dense) result = coefficient * dense->transpose() * operand;
    else result = coefficient * sparse->transpose() * operand;
  }

  // wrapper for c* A \vec{x} + \vec{y}
  void multAdd(const Eigen::VectorXd& multOperand,
    const Eigen::VectorXd& addOperand, Eigen::VectorXd& result,
    const double coefficient = 1.0) const
  {
    if (dense) result = addOperand + coefficient * (*dense) * multOperand;
    else result = addOperand + coefficient * (*sparse) * multOperand;
  }

  // exposes underlying Eigen::MatrixXd or Eigen::SparseMatrix<double> object
  // depending on template parameter
  template<typename T> T& as();

private:

  void readDenseMatrix(std::ifstream& dataFile,
    const std::vector<unsigned>& rowList)
  {
    // std::cout<<"--- Here "<<std::endl;
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
    // std::cout<<"--- matrixRowNumber =  "<<matrixRowNumber<<std::endl;

    unsigned rows = matrixRowNumber;
    unsigned cols = matrixEntries.size() / rows;
    // std::cout<<"--- rows =  "<<rows<<std::endl;
    // std::cout<<"--- cols =  "<<cols<<std::endl;

    dense = std::make_unique<Eigen::MatrixXd>(rows, cols);
    // std::cout << "Number of rows: " << dense->rows() << std::endl;
    // std::cout << "Number of columns: " << dense->cols() << std::endl;
    for (unsigned i = 0; i < rows; i++)
    {
      for (unsigned j = 0; j < cols; j++)
        (*dense)(i,j) = matrixEntries[cols*i + j];
    }
    // for (int i = 0; i < dense->size(); ++i) {
    //   std::cout << (*dense)(i) << "\n";  // Dereference the unique_ptr
    // }

    if (!rowList.empty())
      *dense = (*dense)(rowList, Eigen::all).eval();
  }

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

template<>
Eigen::MatrixXd& Matrix::as<Eigen::MatrixXd>()
{
  assert(dense);
  return *dense;
}

template<>
Eigen::SparseMatrix<double>& Matrix::as<Eigen::SparseMatrix<double>>()
{
  assert(sparse);
  return *sparse;
}
