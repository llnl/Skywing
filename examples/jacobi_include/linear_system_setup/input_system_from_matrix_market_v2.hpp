#include<fstream>
//This reads in the appropriate row of the matrix A that a specific process needs from a matrix market format.
std::vector<std::vector<double>> obtain_A_matrix(int size_of_system, std::vector<int> row_indices, std::string matrix_name)
{

  std::vector<std::vector<double>> A;
  A.resize(row_indices.size());
  // for(int i = 0 ; i < size_of_system; i++)
  // {
  //   A[i].resize(size_of_system);
  // }

  std::string file_path = "../../../examples/jacobi_include//system_hold_folder/matrix_hold_folder/" + matrix_name;


  // This is used to input a matrix in matrix Market Format
  std::ifstream fin(file_path, std::ifstream::in);

  //this ensures that the file is actually open. if not the application terminates
  assert(fin.is_open() == 1);

   //Declare variables for file input
  int input_rows = 0;
  int input_cols = 0;

  // probably delete this
  //int numberOfNonzeroElements=0; //this is for sparse format

  // Ignore headers and comments:
  while (fin.peek() == '%')
  {
    fin.ignore(2048, '\n');
  }

  // this inputs the rows and cols
  fin >> input_rows >> input_cols ;

  assert(input_rows == size_of_system);
  // Probably delete this.
  // fin >> input_rows >> input_cols >>numberOfNonzeroElements;//this is or sparse format input

  double hold = 0.0;
  for(int i = 0 ; i < input_rows; i++)
  {
    for(int j = 0 ; j < input_cols; j++)
    {
      // fin >> data;
      fin >> hold;
      for(int k = 0 ; k < static_cast<int>(row_indices.size()); k++)
      {
        if(i == row_indices[k])
        {
          A[k].push_back(hold);
        }
      }
    }
    hold =0.0;
  }
  std::this_thread::sleep_for(std::chrono::milliseconds{100});

  // if(row_indices[0] == 0 )
  // {
  //     std::cout << "here for " << row_indices[0] << ":" << std::endl;
  //     for(int j = 0 ; j < static_cast<int>(row_indices.size()); j++)
  //     {
  //       for(int i = 0 ; i < size_of_system; i++)
  //       {
  //         std::cout << A[j][i] << " ";
  //       }
  //       std::cout << "            ";
  //       std::cout << std::endl;
  //     }
  // }

return A;
}

//This reads in the appropriate entries of b that a specific process needs from a matrix market format.
std::vector<double> obtain_rhs_vector(int size_of_system, std::vector<int> row_indices, std::string matrix_name)
{

  std::vector<double> b_values(static_cast<int>(row_indices.size()),0.0);

  std::string file_path = "../../../examples/jacobi_include//system_hold_folder/rhs_hold_folder/rhs_" + matrix_name;


  // This is used to input a matrix in matrix Market Format
  std::ifstream fin(file_path, std::ifstream::in);

  //this ensures that the file is actually open. if not the application terminates
  assert(fin.is_open() == 1);

   //Declare variables for file input
  int input_rows = 0;
  int input_cols = 0;

  // probably delete this
  //int numberOfNonzeroElements=0; //this is for sparse format

  // Ignore headers and comments:
  while (fin.peek() == '%')
  {
    fin.ignore(2048, '\n');
  }

  // this inputs the rows and cols
  fin >> input_rows >> input_cols ;

  assert(input_rows == size_of_system);
  assert(input_cols ==1);

  double hold = 0;
  // Probably delete this.
  // fin >> input_rows >> input_cols >>numberOfNonzeroElements;//this is or sparse format input

  for(int i = 0 ; i < input_rows; i++)
  {

      fin>>hold;
      for(int k = 0 ; k < static_cast<int>(row_indices.size()); k++)
      {
        if(i == row_indices[k])
        {
          b_values[k] = hold;
        }
      }
      hold = 0.0;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds{100});
  // if(row_indices[0] == 0 )
  // {
      // std::cout << "here for " << row_indices[0] << ": ";
      // for(int i = 0 ; i < static_cast<int>(row_indices.size()); i++)
      // {
      //   std::cout << b_values[i] << " ";
      // }
      // std::cout << std::endl;
  // }

return b_values;
}


//This reads in the appropriate entries of b that a specific process needs from a matrix market format.
std::vector<double> obtain_local_solution_vector(int size_of_system, std::vector<int> row_indices, std::string matrix_name)
{

  std::vector<double> x_local_answer(static_cast<int>(row_indices.size()),0.0);

  std::string file_path = "../../../examples/jacobi_include//system_hold_folder/sol_hold_folder/x_sol_" + matrix_name;

  // This is used to input a matrix in matrix Market Format
  std::ifstream fin(file_path, std::ifstream::in);

  //this ensures that the file is actually open. if not the application terminates
  assert(fin.is_open() == 1);

   //Declare variables for file input
  int input_rows = 0;
  int input_cols = 0;

  // probably delete this
  //int numberOfNonzeroElements=0; //this is for sparse format

  // Ignore headers and comments:
  while (fin.peek() == '%')
  {
    fin.ignore(2048, '\n');
  }

  // this inputs the rows and cols
  fin >> input_rows >> input_cols ;

  assert(input_rows == size_of_system);
  assert(input_cols ==1);

  double hold = 0;
  // Probably delete this.
  // fin >> input_rows >> input_cols >>numberOfNonzeroElements;//this is or sparse format input

  for(int i = 0 ; i < input_rows; i++)
  {

      fin>>hold;
      for(int k = 0 ; k < static_cast<int>(row_indices.size()); k++)
      {
        if(i == row_indices[k])
        {
          x_local_answer[k] = hold;
        }
      }
      hold = 0.0;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds{100});
  // if(row_indices[0] == 0 )
  // {
      // std::cout << "here for " << row_indices[0] << ": ";
      // for(int i = 0 ; i < static_cast<int>(row_indices.size()); i++)
      // {
      //   std::cout << b_values[i] << " ";
      // }
      // std::cout << std::endl;
  // }

return x_local_answer;
}

std::vector<double> obtain_full_solution_vector(int size_of_system, std::string matrix_name)
{

  std::vector<double> x_full_solution(size_of_system, 0.0);

  std::string file_path = "../../../examples/jacobi_include//system_hold_folder/sol_hold_folder/x_sol_" + matrix_name;

  // This is used to input a matrix in matrix Market Format
  std::ifstream fin(file_path, std::ifstream::in);

  //this ensures that the file is actually open. if not the application terminates
  assert(fin.is_open() == 1);

   //Declare variables for file input
  int input_rows = 0;
  int input_cols = 0;

  // probably delete this
  //int numberOfNonzeroElements=0; //this is for sparse format

  // Ignore headers and comments:
  while (fin.peek() == '%')
  {
    fin.ignore(2048, '\n');
  }

  // this inputs the rows and cols
  fin >> input_rows >> input_cols ;

  assert(input_rows == size_of_system);
  assert(input_cols ==1);

  double hold = 0.0;

  for(int i = 0 ; i < input_rows; i++)
  {
      fin>>hold;
      x_full_solution[i] = hold;
      hold = 0.0;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds{100});
  // if(row_indices[0] == 0 )
  // {
      // std::cout << "here for " << row_indices[0] << ": ";
      // for(int i = 0 ; i < static_cast<int>(row_indices.size()); i++)
      // {
      //   std::cout << b_values[i] << " ";
      // }
      // std::cout << std::endl;
  // }

return x_full_solution;
}
