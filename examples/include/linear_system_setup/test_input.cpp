#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <string>
#include <vector>
#include <iomanip>

#include "input_system_from_matrix_market.hpp"
#include "obtain_exact_x.hpp"

int main()
{

int size_of_system = 10;
int redundant = 2;
std::vector<int> rows;
rows.resize(2);

rows[0] = 0;
rows[1] = 1;

std::string matrix_type = "randomInteger_10_diagDom_1.mtx";

auto matrix_partition = obtain_A_matrix(size_of_system, rows, matrix_type);

std::cout << "here is matrix_partition: " << std::endl;
for(std::vector<std::vector<double>>::size_type i = 0  ; i < matrix_partition.size(); i++)
{
  for(std::vector<double>::size_type j = 0 ; j < matrix_partition[0].size(); j++)
  {
    std::cout<< matrix_partition[i][j] << " ";
  }
  std::cout<<std::endl;
}

auto answer = obtain_solution_vector(size_of_system, rows);
std::cout << "here is matrix_partition: " << std::endl;
for(std::vector<std::vector<double>>::size_type i = 0  ; i < answer.size(); i++)
{
  std::cout<< answer[i] << " ";
}
std::cout<<std::endl;

return 0;
}
