double calculate_partial_residual(double redundancy, std::vector<double> x_local_solution,  std::vector<double> b_values,std::vector<std::vector<double>> matrix_rows){

  double partial_residual = 0.0;
  for (int i = 0 ; i< redundancy; i++)
  {
    double partial_residual_hold = 0.0;
    for (int j = 0; j< static_cast<double>(matrix_rows[0].size()); j++)
    {
      partial_residual_hold = matrix_rows[i][j]*x_local_solution[j]+ partial_residual_hold;
    }
    partial_residual_hold = partial_residual_hold - b_values[i];
    partial_residual_hold = pow(partial_residual_hold,2);
    partial_residual = partial_residual + partial_residual_hold;
  }
  return partial_residual;
}

double calculate_partial_forward_error(int number_of_updated_components, std::vector<double> x_partition_estimate, std::vector<double> x_local_solution)
{
  double partitioned_forward_error = 0.0;
  // std::vector<double> forward_error_vector(static_cast<int>(x_local_solution.size()),0.0);
  for(int i =0 ; i < number_of_updated_components; i++)
  {
    double hold_value = 0.0;
    hold_value = x_partition_estimate[i]-x_local_solution[i];
    hold_value = pow(hold_value,2);
    partitioned_forward_error = partitioned_forward_error + hold_value;
  }
  return partitioned_forward_error;
}
