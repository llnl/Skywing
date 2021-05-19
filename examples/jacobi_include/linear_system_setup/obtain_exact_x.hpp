std::vector<double> obtain_solution_vector(int size_of_system, std::vector<int> rows)
{
  std::vector<double> solution;

  for(int i = 0 ; i < static_cast<int>(rows.size()); i++)
  {
    solution.push_back(rows[i]);
  }


return solution;
}
