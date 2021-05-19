#ifndef SKYNET_UPPER_SYNCHRONOUS_JACOBI_HPP
#define SKYNET_UPPER_SYNCHRONOUS_JACOBI_HPP

#include "skynet_core/job.hpp"
#include "skynet_core/master.hpp"
#include "skynet_upper/synchronous_iterative.hpp"
// This class solves the square linear system Ax=b.
// This assumes that the system is square, consistent, and digonally dominant and thus solvable by this method.
// This class stores the full solution vector x_iter at each machine, and the specific update that each machine solves for can be viewed by the user by calling the print_solution() function or output by the return_solution() function or return_full_solution() for either the specific update the machine solved for or the full vector respectively.

// Convenient alias for passing doubles between machines.
using ValueTag = skynet::PublishTag<double>;

using namespace skynet;

// This is if we want to template this class.
// Kendall doesn't recommend this since we would then have to deal with the case where a user inputs tuples.
// This isn't something she recommended doing, at least for the moment.
// Since the only feasible types for the linear system are doubles or floats, this didn't seem to be a pressing issue.
// The main crux of this example is the constructor function below.
//

// template<typename ... TagValueTypes>
class SynchronousJacobi
{

private:

  friend class SynchronousIterative<double>;

  // Variables used for jacobi iteration.
  std::vector<double> matrix_row;
  // std::vector<ValueOrTuple<TagValueTypes...>> matrix_row;
  double b_value;
  std::vector<double> x_iter;
  std::vector<int> recv_indexes;
  int row_number;


  // Variables used for SynchronousIterative class and communication.
  // std::array<ValueTag, 4> tags;

  SynchronousIterative<double> iter_method ;

public:

  SynchronousJacobi( int row_number, std::vector<double> matrix_row, double b_value, SynchronousIterative<double> it): matrix_row(matrix_row), b_value(b_value), row_number(row_number),
  iter_method(it)
  {
    x_iter.resize(matrix_row.size(), 0.0);
    recv_indexes.resize(matrix_row.size()-1, 0.0);

    for(std::vector<double>::size_type i = 0 ; i < matrix_row.size(); i++)
    {
      if((int)i < row_number)
      {
        recv_indexes[i] = i ;
      }
      else if ((int)i > row_number)
      {
        recv_indexes[i - 1] = i ;
      }
    }

    // This is a quick check on the setup.
    // if(row_number == 0 )
    // {
        // print_current_information();
    // }

  };


  void create_iteration()
  {
    x_iter[row_number] = jacobi();
    //This publishes the value to everyone.
    const auto& values_to_publish = x_iter[row_number];

    //This collects all publishes values, including the one of the home process.
    const auto values = iter_method.values(values_to_publish).get();

    for(std::vector<double>::size_type i = 0 ; i <recv_indexes.size()  ; i++)
    {
      x_iter[recv_indexes[i]] = values[recv_indexes[i]];
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{100});

  };


  ~SynchronousJacobi(){};


  double jacobi()
  {
    double hold = 0.0;
    for(std::vector<double>::size_type i = 0 ; i < matrix_row.size(); i++)
    {
      if((int)i!= row_number)
      {
        hold = hold + matrix_row[i] * x_iter[i];
      }
    }
    hold  = (b_value  - hold)/matrix_row[row_number] ;
    return hold;
  };

  // Functions to print solution vector or individual solution.
  void print_solution()
  {
    std::cout << "The solution for this machine is " << x_iter[row_number] << std::endl;
  };
  void print_full_x_iter()
  {
    std::cout << "The full x_iter vector is: ";
    for(std::vector<double>::size_type i = 0 ; i < x_iter.size(); i++)
    {
      std::cout << x_iter[i] << " ";
    }
    std::cout << std::endl;
  };
  // Functions to return solution vector or individual solution.
  double return_solution()
  {
    return x_iter[row_number];
  };
  std::vector<double> return_solution_as_vec()
  {
    std::vector<double> return_vec(1,x_iter[row_number]);
    return return_vec;
  };
  std::vector<double> return_full_x_iter()
  {
    return x_iter;
  }
  // This is mainly for troubleshooting in the case of bad communication or unexplained behavior.
  void print_current_information()
  {
      std::cout << "x_iter: " ;

      for(std::vector<double>::size_type i = 0 ; i < x_iter.size(); i++)
      {
        std::cout << x_iter[i] << " ";
      }
      std::cout << std::endl;

      std::cout << "recv_indexes " ;

      for(std::vector<int>::size_type i = 0 ; i < recv_indexes.size(); i++)
      {
        std::cout << recv_indexes[i] << " ";
      }
      std::cout << std::endl;
  }

}; // end synchronous_jacobi


template<typename... Args>
auto create_synchronous_jacobi(int row_number, std::vector<double> matrix_row, double b_value, Args&&... args) noexcept
{

  return create_synchronous_iterative(std::forward<Args>(args)...).then([=](std::optional<SynchronousIterative<double>> it) -> std::optional<SynchronousJacobi> {
     if (it) { return SynchronousJacobi(row_number, matrix_row, b_value, *it);}
     else    { return {}; }
  });


}


#endif // SKYNET_UPPER_SYNCHRONOUS_JACOBI_HPP
