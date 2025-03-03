#pragma once

#include <fstream>
#include <memory>

#include "skywing_mid/associative_vector.hpp"


#include <Eigen/Dense>
#include <Eigen/Sparse>

/**
 * @class Vector
 * @brief A class for handling dense vectors using Eigen library.
 */
class Vector {
    std::unique_ptr<Eigen::VectorXd> dense; ///< Unique pointer to the dense vector.

public:
    /**
     * @brief Default constructor that creates an empty vector.
     */
    Vector() {
        dense = std::make_unique<Eigen::VectorXd>();
    }

    /**
     * @brief Constructor that creates a dense vector based on a file.
     * @param filename The name of the file containing vector data.
     * @param rowList Optional list of row indices to filter the vector.
     */
    Vector(const std::string& filename, const std::vector<unsigned> rowList = std::vector<unsigned>()) {
        // std::cout<<"now in Vector()"<<std::endl; 
        dense = std::make_unique<Eigen::VectorXd>(readVector(filename, rowList));
        // std::cout<<dense<<std::endl; 

    }

    // Disable copying
    Vector(const Vector& other) = delete;
    Vector& operator=(const Vector& other) = delete;

    // Enable moving
    /**
     * @brief Move constructor.
     * @param other The vector to move from.
     */
    Vector(Vector&& other) {
        dense = std::move(other.dense);
    }

    /**
     * @brief Move assignment operator.
     * @param other The vector to move from.
     * @return Reference to this vector.
     */
    Vector& operator=(Vector&& other) {
        dense = std::move(other.dense);
        return *this;
    }

    /**
     * @brief Get the size of the vector.
     * @return The size of the vector.
     */
    Eigen::Index size() const {
        return dense->size();
    }

    /**
     * @brief Access an element of the vector.
     * @param i Index of the element.
     * @return The value of the element at index i.
     */
    double operator()(Eigen::Index i) const {
        return (*dense)(i);
    }

    /**
     * @brief Expose underlying Eigen::VectorXd object.
     * @return Reference to the Eigen::VectorXd object.
     */
    Eigen::VectorXd& as() {
        assert(dense);
        return *dense;
    }

private:
    /**
     * @brief Reads a vector from a file.
     * @param filename The name of the file containing vector data.
     * @param rowList Optional list of row indices to filter the vector.
     * @return The Eigen::VectorXd object created from the file data.
     * @throws std::runtime_error If the file cannot be opened.
     * @throws std::out_of_range If a row index is out of range.
     */
    Eigen::VectorXd readVector(const std::string& filename, const std::vector<unsigned>& rowList = std::vector<unsigned>()) {
        // std::cout<<"in readVector"<<std::endl; 
        std::ifstream dataFile(filename);
        if (!dataFile.is_open()) {
            throw std::runtime_error("Could not open file");
        }
        std::string line;
        std::vector<double> vectorEntries;
        // unsigned lineNumber = 0;
        while (std::getline(dataFile, line)) {
            std::stringstream lineStream(line);
            double value;
            while (lineStream >> value) {
                vectorEntries.push_back(value);
            }
            // lineNumber++;
        }
        // std::cout<<"in readVector loc 2 "<<std::endl; 

        // Create a vector with the size of the total entries
        Eigen::VectorXd vector(vectorEntries.size());
        // Fill the vector with the entries
        for (size_t i = 0; i < vectorEntries.size(); ++i) {
            vector(i) = vectorEntries[i];
        }
        // If rowList is not empty, filter the vector
        if (!rowList.empty()) {
            Eigen::VectorXd filteredVector(rowList.size());
            for (size_t i = 0; i < rowList.size(); ++i) {
                if (rowList[i] < vector.size()) {
                    filteredVector(i) = vector(rowList[i]);
                } else {
                    throw std::out_of_range("Row index out of range");
                }
            }
            return filteredVector;
        }
        // std::cout<<"in readVector loc 3 "<<std::endl; 
        // std::cout<<vector<<std::endl; 
        return vector;
    }
};
