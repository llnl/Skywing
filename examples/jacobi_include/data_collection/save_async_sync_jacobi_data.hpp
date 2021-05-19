void collect_data_each_component(int machine_number, int redundancy, int trial, double partial_forward_error, double partial_residual, int iteration_count, double time)
{
  std::string local_information_filename="data_hold/redundancy_" + std::to_string(redundancy) + "_trial_" + std::to_string(trial) + "_rank_" + std::to_string(machine_number) + ".csv";
  std::ofstream local_information;
  local_information.open(local_information_filename);
  local_information << "Redundancy ";
  local_information << ",";
  local_information << "Trial";
  local_information << ",";
  local_information << "Rank ";
  local_information << ",";
  local_information << "Local Error";
  local_information << ",";
  local_information << "Local Residual";
  local_information << ",";
  local_information << "Iteration Count";
  local_information << ",";
  local_information << "Time";
  local_information << "\n";

  local_information << redundancy;
  local_information << ",";
  local_information << trial;
  local_information << ",";
  local_information << machine_number;
  local_information << ",";
  local_information << partial_forward_error;
  local_information << ",";
  local_information << partial_residual;
  local_information << ",";
  local_information << iteration_count;
  local_information << ",";
  local_information << time;
  local_information << "\n";

  local_information.close();
}
