
#include <iostream>
#include <fstream>

#ifndef SKYWING_MATH_TEST_EXAMPLE
#define SKYWING_MATH_TEST_EXAMPLE



using namespace skywing;

// In the real i/o this values will be based on the values defined in the
// processor. For now, we use these hardcoded types.
using index_t = uint32_t;
using scalar_t = double;

using ClosedVector = AssociativeVector<index_t, scalar_t, false>;
using AssociativeMatrix = AssociativeVector<index_t, ClosedVector, false>;

std::string partitionfile; 
std::string rhsfile; 
std::string matrixfile; 
void set_dataDir(){
    #ifdef DATA_DIR_DEST
        std::cout << "Data directory: " << DATA_DIR_DEST << std::endl;
    #else
        std::cerr << "DATA_DIR_DEST is not defined!" << std::endl;
    #endif


    partitionfile = std::string(DATA_DIR_DEST) +"/partition.txt";
    rhsfile =  std::string(DATA_DIR_DEST) + "/rhs.txt";
    matrixfile = std::string(DATA_DIR_DEST) + "/matrix.txt";
}

void printUnorderedMap(const std::unordered_map<uint32_t, std::vector<uint32_t>>& map) {
    for (const auto& pair : map) {
        uint32_t key = pair.first;
        const std::vector<uint32_t>& values = pair.second;

        std::cout << "Key: " << key << " -> Values: [";
        for (size_t i = 0; i < values.size(); ++i) {
            std::cout << values[i];
            if (i < values.size() - 1) {
                std::cout << ", ";
            }
        }
        std::cout << "]" << std::endl;
    }
}

std::unordered_map<uint32_t, std::vector<uint32_t>> read_partition_from_file(){
    std::unordered_map<uint32_t, std::vector<unsigned>> partition = readPartition(partitionfile);
    // printUnorderedMap(partition);
    return partition;
}

// These functions are used for testing before i/o functionality is added
AssociativeMatrix read_matrix_from_file(std::vector<unsigned> rowList)
{
    Eigen::MatrixXd A = readMatrix<Eigen::MatrixXd>(matrixfile);
    return convert_eigen_matrix_to_associative_matrix(A,rowList);
}

ClosedVector read_rhs_from_file(std::vector<unsigned> rowList)
{
    // std::cout<<"loading vector"<<std::endl; 
    Eigen::VectorXd b = readVector(rhsfile);
    // std::cout<<"done vector b = "<<b<<std::endl; 

    ClosedVector b_av = convert_eigen_vector_to_associative_vector(b, rowList);
    // printAssociativeVector(b_av);
    return  b_av;

}



#endif // SKYWING_MATH_TEST_EXAMPLE
