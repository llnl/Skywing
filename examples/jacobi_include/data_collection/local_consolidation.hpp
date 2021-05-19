typedef double precision;

static void forwardErrorOutput(int rank, precision local_forward_error)
{
  std::string my_file_name_local_forward_error="LocalIterateForwardErrorInformation" + std::to_string(rank) + ".csv";
  std::ofstream my_file_local_forward_error;
  my_file_local_forward_error.open(my_file_name_local_forward_error);

  //my_file_local_forward_error.precision(dbl::max_digits10);

  my_file_local_forward_error << "My rank ";
  my_file_local_forward_error << ",";
  my_file_local_forward_error << rank;
  my_file_local_forward_error << "\n";
  my_file_local_forward_error << "Local Forward Error ";
  my_file_local_forward_error << ",";
  my_file_local_forward_error << local_forward_error;
  my_file_local_forward_error << "\n";
  my_file_local_forward_error.close();

}


static void timeOutput(int rank, precision time)
{
  std::string myFileNameLocalRuntime="LocalIterateRuntimeInformation" + std::to_string(rank) + ".csv";
  std::ofstream myFileLocalRuntime;
  myFileLocalRuntime.open(myFileNameLocalRuntime);

  //myFileLocalRuntime.precision(dbl::max_digits10);

  myFileLocalRuntime << "My rank ";
  myFileLocalRuntime << ",";
  myFileLocalRuntime << rank;
  myFileLocalRuntime << "\n";
  myFileLocalRuntime << "Local Runtime ";
  myFileLocalRuntime << ",";
  myFileLocalRuntime << time;
  myFileLocalRuntime << "\n";
  myFileLocalRuntime.close();
}


static void residualOutput(int rank, precision residual)
{
  std::string myFileNameLocalResidual="LocalIterateResidualInformation" + std::to_string(rank) + ".csv";
  std::ofstream myFileLocalResidual;
  myFileLocalResidual.open(myFileNameLocalResidual);

  //myFileLocalResidual.precision(dbl::max_digits10);

  myFileLocalResidual << "My rank ";
  myFileLocalResidual << ",";
  myFileLocalResidual << rank;
  myFileLocalResidual << "\n";
  myFileLocalResidual << "Local RunResidual ";
  myFileLocalResidual << ",";
  myFileLocalResidual << residual;
  myFileLocalResidual << "\n";
  myFileLocalResidual.close();
}


static void iterationCountOutput(int rank, int IterationCount)
{
  std::string myFileNameLocalIterationCount="LocalIterateIterationCountInformation" + std::to_string(rank) + ".csv";
  std::ofstream myFileLocalIterationCount;
  myFileLocalIterationCount.open(myFileNameLocalIterationCount);

  myFileLocalIterationCount << "My rank ";
  myFileLocalIterationCount << ",";
  myFileLocalIterationCount << rank;
  myFileLocalIterationCount << "\n";
  myFileLocalIterationCount << "Local IterationCount ";
  myFileLocalIterationCount << ",";
  myFileLocalIterationCount << IterationCount;
  myFileLocalIterationCount << "\n";
  myFileLocalIterationCount.close();
}



static void endSaveToFile(std::ofstream & my_file_local_information)
{
  my_file_local_information << "eof";
}
