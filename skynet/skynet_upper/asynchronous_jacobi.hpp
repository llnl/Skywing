#ifndef SKYNET_UPPER_ASYNCHRONOUS_JACOBI_HPP
#define SKYNET_UPPER_ASYNCHRONOUS_JACOBI_HPP

#include "skynet_core/job.hpp"
#include "skynet_core/master.hpp"
#include "skynet_upper/asynchronous_iterative.hpp"
#include <mutex>
// #include "utils.h"

// This class solves the square linear system Ax=b.
// This assumes that the system is square, consistent, and digonally dominant and thus solvable by this method.
// This class stores the full solution vector x_iter at each machine, and the specific update that each machine solves for can be viewed by the user by calling the print_solution() function or output by the return_solution() function or return_full_solution() for either the specific update the machine solved for or the full vector respectively.

// Convenient alias for passing doubles between machines.
using ValueTag = skynet::PublishTag<std::vector<double>>;

using namespace skynet;

// This is if we want to template this class.
// Kendall doesn't recommend this since we would then have to deal with the case where a user inputs tuples.
// This isn't something she recommended doing, at least for the moment.
// Since the only feasible types for the linear system are doubles or floats, this didn't seem to be a pressing issue.
// The main crux of this example is the constructor function below.
//

// template<typename ... TagValueTypes>
class AsynchronousJacobi
{

private:

  friend class AsynchronousIterative<std::vector<double>>;

  // Variables used for jacobi iteration.

  // std::vector<int> recv_indexes;
  int row_number;
  int number_of_updated_components;
  int size_of_system;
  std::vector<std::vector<double>> matrix_rows;
  // std::vector<ValueOrTuple<TagValueTypes...>> matrix_rows;
  std::vector<double> b_values;
  std::vector<int> row_indices;
  std::vector<ValueTag> tags_vector;
  AsynchronousIterative<std::vector<double>> iter_method ;
  std::vector<double> x_iter;
  std::vector<double> publish_values;
  int debug_machine_number;
  int delayed_iteration_count;
  int delayed_process ;
public:

  AsynchronousJacobi(int row_number,  int number_of_updated_components,  std::vector<std::vector<double>> matrix_rows, std::vector<double> b_values, std::vector<int> row_indices, std::vector<ValueTag> tags_vector, AsynchronousIterative<std::vector<double>> it):
    row_number(row_number),
    number_of_updated_components(number_of_updated_components),
    matrix_rows(matrix_rows),
    b_values(b_values),
    row_indices(row_indices),
    tags_vector(tags_vector),
    iter_method(it)
  {

    size_of_system = static_cast<int>(matrix_rows[0].size());

    for(int i = 0 ; i < size_of_system; i ++)
    {
      x_iter.push_back(0);
    }

    for(int i = 0 ; i < number_of_updated_components; i ++)
    {
      publish_values.push_back(0.0);
    }
    // This is a quick check on the setup.
    // if(row_number == 0 )
    // {
        // print_current_information();
    // }
    debug_machine_number = 0;
    delayed_iteration_count = 1;
    delayed_process = 1;
   // print_bvalues_partition();
   // print_matrix_partition();

  };


  void create_iteration(int count)
  {
    // count++;
    for(int i = 0 ; i < number_of_updated_components; i++)
    {
      x_iter[row_indices[i]] = jacobi(i, row_indices[i]);
      publish_values[i] = x_iter[row_indices[i]];
    }

    // print_publish_values();



    // if(row_number == debug_machine_number)
    // {
    //   std::cout << row_number << "has row_indices: ";
    //   for(int i = 0 ; i < number_of_updated_components; i++)
    //   {
    //     std::cout << row_indices[i] << " ";
    //   }
    //   std::cout << std::endl;
    // }

    // This submits the values for publication to all processes.
    iter_method.submit_values(publish_values);

    // This stores the values as a AsynchronousValues allocator which contains a bool if it is updated and a vector<double> and alive tags as vector<ValueTag>.
    const auto& [values, alive_tags] = iter_method.values();
    // if(row_number == debug_machine_number)
    // {
    //   (values);
    // }

    // std::this_thread::sleep_for(std::chrono::milliseconds{200});

    // This cycles through the entries of values from the iter.method()
    for(int values_index = 0; values_index < static_cast<int>(values.size()); ++values_index)
    {
      //stores the received values as a vector<double> and updated as bool
      const auto& [received_values, updated] = values[values_index];
      // std::cout << "received_values.size() " << received_values.size() << std::endl;
      if (updated)
      {
        // this for loop has to check for redundant components.
        // index is used below to identify whether the component in question should be updated or not by checking it against the row_indices.
        for(int tags_index = 0; tags_index < size_of_system; tags_index++)
        {
          // this checks if alive_tag w are looking matches against the index we are currently checking, this is a matter of "when" not "if" we find the correct tag
          if(alive_tags[values_index]==tags_vector[tags_index])
          {
            // This cycles through the received_values in order not to replace a component that each process is updating with another processes update.
            for(int received_values_index = 0; received_values_index < number_of_updated_components; received_values_index++)
            {
              int next_index = (tags_index + received_values_index) % size_of_system;

              bool test_if_next_index_is_replaced = true;

              // This cycles through individual values in row_index
              for(int row_index_cycle = 0 ; row_index_cycle < static_cast<int>(row_indices.size()); row_index_cycle++)
              {
                if(next_index == row_indices[row_index_cycle])
                {
                  test_if_next_index_is_replaced = false;
                }
              }

              if(test_if_next_index_is_replaced == true)
              {
                  // if(row_number == 0)
                  // {
                  //   std::cout << row_number << " is replacing component: " << next_index << " with " << received_values[received_values_index] << " which has source " << tags_index << " component " << received_values_index  << std::endl;
                  // }
                  if(count % delayed_iteration_count == 0 && row_number == delayed_process)
                  {
                    x_iter[next_index] = received_values[received_values_index];
                  }
                  else if(row_number!=delayed_process)
                  {
                    x_iter[next_index] = received_values[received_values_index];
                  }
              }
              // if(row_number == 0)
              // {
              //   std::cout << "values_index: " << values_index << " received_values: ";
              //   for(int j = 0 ; j < number_of_updated_components; j++)
              //   {
              //     std::cout << "[" << j << "] = "<< received_values[j] << " ";
              //   }
              //   std::cout << std::endl;
              // }
            }

            // std::cout << row_number << " is reciving from : "<< index<< " value: " << received_value << std::endl;
          }
        }
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{100});

  };


  ~AsynchronousJacobi(){};


  double jacobi(int redundant_component_index, int updated_index)
  {
    //redundant_component_index is the local row index of the distributed system
    // updated_index is the index of the full vector x that we are currently updating
    double hold = 0.0;
    for(int i = 0 ; i < size_of_system; i++)
    {
      if(i!= updated_index)
      {
        hold = hold + matrix_rows[redundant_component_index][i] * x_iter[i];
      }
    }
    hold  = (b_values[redundant_component_index]  - hold)/matrix_rows[redundant_component_index][updated_index] ;

    return hold;
  };

  void print_jacobi_iteration_arithmetic(int redundant_component_index, int updated_index)
  {
    //redundant_component_index is the local row index of the distributed system
    // updated_index is the index of the full vector x that we are currently updating
    std::cout << row_number << " update for component: " << updated_index << std::endl;
    std::cout << "( " << b_values[redundant_component_index] << " - (";
    for(int i = 0 ; i < size_of_system; i++)
    {
      if(i!= updated_index)
      {
        std::cout << "( " << matrix_rows[redundant_component_index][i];
        std::cout << " * " << x_iter[i] << ")";
        if(i < size_of_system-1)
        {
          std::cout << " + ";
        }
        else
        {
          std::cout << ")";
        }
      }

    }
    std::cout << ") / " << matrix_rows[redundant_component_index][updated_index] << " = " << x_iter[updated_index]<< std::endl;
  }

  void print_matrix_partition()
  {
    std::cout << row_number << " has matrix partition: \n";
    for(int i = 0 ; i < number_of_updated_components; i ++)
    {
      std::cout << "row " << row_indices[i]<< ": ";
      for(int j = 0 ; j < size_of_system; j++)
      {
        std::cout << matrix_rows[i][j] << " ";
      }
      std::cout << std::endl;
    }
  }

  void print_bvalues_partition()
  {
    std::cout << row_number << " has b_values partition: ";
    for(int i = 0 ; i < number_of_updated_components; i ++)
    {
      std::cout << b_values[i] << " ";
    }
    std::cout << std::endl;

  }

  // Functions to print solution vector or individual solution.
  void print_solution()
  {
    std::cout << "\t The solution for " << row_number << " is ";

    for(int i = 0 ; i < number_of_updated_components; i ++)
    {
      std::cout << x_iter[row_indices[i]] << " ";
    }
    std::cout << std::endl;
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
  std::vector<double> return_solution()
  {
    std::vector<double> return_vector;
    for(int i = 0 ; i < number_of_updated_components; i++)
    {
      return_vector.push_back( x_iter[row_indices[i]]);
    }
    return return_vector;
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

  }

  void print_publish_values()
  {
    std::cout << row_number << " has publish_values: ";
    for(int i = 0 ; i < number_of_updated_components; i++)
    {
      std::cout<< publish_values[i] << " ";
    }
    std::cout << std::endl;
  }

void print_all_received_information(skynet::AsynchronousValues<std::vector<double>> values)
{

  std::cout << row_number << " has information: \n\t";
  for(int values_index = 0; values_index < static_cast<int>(values.size()); ++values_index)
  {
    const auto& [received_values, updated] = values[values_index];
    if (updated)
    {
      std::cout << " values_index: " << values_index << "\n\t\t";
      for(int received_values_index = 0; received_values_index < number_of_updated_components; received_values_index++)
      {
        std::cout << received_values[received_values_index] << " ";
      }
      if(values_index < static_cast<int>(values.size())-1)
      {
        std::cout << "\n\t";

      }
      else
      {
        std::cout << "\n";
      }
    }
  }
}

}; // end asynchronous_jacobi

// This is the continuation that makes this class possible as this implementation depends upon the asynchronous_iterative class.
template<typename... Args>
auto create_asynchronous_jacobi(int row_number, int number_of_updated_components, std::vector<std::vector<double>> matrix_rows, std::vector<double> b_values, std::vector<int> row_indices, std::vector<ValueTag> tags, Args&&... args) noexcept
{

  return create_asynchronous_iterative(std::forward<Args>(args)...).then([=](std::optional<AsynchronousIterative<std::vector<double>>> it) -> std::optional<AsynchronousJacobi> {
     if (it) { return AsynchronousJacobi(row_number, number_of_updated_components, matrix_rows, b_values, row_indices, tags, *it);}
     else    { return {}; }
  });

}


#endif // SKYNET_UPPER_ASYNCHRONOUS_JACOBI_HPP
