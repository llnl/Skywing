#include <fstream>

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


static void iterationCountOutput(int rank, int iteration_count)
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
  myFileLocalIterationCount << iteration_count;
  myFileLocalIterationCount << "\n";
  myFileLocalIterationCount.close();
}


// static void createOutputFileForLocalInformation(int num_ranks, int rank, int buffer_size, int message_size, int components_for_each_process_for_this_set_of_trials, std::ofstream &my_file_local_information)
// {
//
//   // outputs the process number for the information in the .csv file.
//   int message_size_count = 0 ;
//
//   std::string myFileNameLocalInformation = "LocalIterateInformationFor_" + std::to_string(rank) + ".csv";
//
//   my_file_local_information.open(myFileNameLocalInformation);
//
//   my_file_local_information << "My rank ";
//   my_file_local_information << ",";
//   my_file_local_information << rank;
//   my_file_local_information << "\n";
//
//   my_file_local_information << ",";
//
//   for(int i = 0 ; i < buffer_size; i++)
//   {
//     my_file_local_information << ",";
//
//     if(i % (message_size - 1) == 0 && message_size_count < num_ranks)
//     {
//       my_file_local_information << "Process ";
//       my_file_local_information << message_size_count;
//       my_file_local_information << ",";
//
//       message_size_count ++;
//     }
//
//   }
//
//   my_file_local_information << "\n";
//   my_file_local_information << "\n";
//
// }
//
//
// static void localInformationSaveToFile(int num_ranks, int rank, int components_for_each_process_for_this_set_of_trials, int message_size, int buffer_size, int information_for_iteration_size, precision *&all_local_information, precision *&receive_buffer, precision *& information_for_iteration, std::ofstream & my_file_local_information)
// {
//   my_file_local_information << ",";
//   my_file_local_information << ",";
//
//   for(int i = 0; i < num_ranks; i++)
//   {
//
//     for(int j = 0; j < components_for_each_process_for_this_set_of_trials; j++)
//     {
//       my_file_local_information << "Comp ";
//       my_file_local_information << j;
//       my_file_local_information << ",";
//     }
//
//     my_file_local_information << "Up Rate";
//     my_file_local_information << ",";
//     my_file_local_information << "It Count";
//     my_file_local_information << ",";
//     my_file_local_information << "For Err";
//     my_file_local_information << ",";
//
//   }
//
//   my_file_local_information << "\n";
//
//   my_file_local_information << "receive_buffer: ";
//   my_file_local_information << ",";
//   my_file_local_information << ",";
//
//   for(int i = 0 ; i < buffer_size; i++)
//   {
//     my_file_local_information << receive_buffer[i];
//     my_file_local_information << ",";
//     // if(i%message_size == 0&& i > 0)
//     // {
//     //   my_file_local_information << ",";
//     //
//     // }
//
//   }
//
//   my_file_local_information << "\n";
//
//   my_file_local_information << "all_local_information: ";
//   my_file_local_information << ",";
//   my_file_local_information << ",";
//
//   for(int i = 0 ; i < buffer_size; i++)
//   {
//     my_file_local_information << all_local_information[i];
//     my_file_local_information << ",";
//     // if(i%message_size == 1 && i > 0)
//     // {
//     //   my_file_local_information << ",";
//     //
//     // }
//   }
//
//   my_file_local_information << "\n";
//   my_file_local_information << "\n";
//
//
//
//   //this count tracks the output placement in the .csv file for ease of viewing
//   int upCount = 0;
//   //this tracks the name of the component for output
//   int processName = 0;
//
//
//   my_file_local_information << ",";
//   my_file_local_information << ",";
//
//   for(int i = 0 ; i < buffer_size; i++)
//   {
//     if(i % message_size == 0)
//     {
//       my_file_local_information << "Comp ";
//       my_file_local_information << processName;
//       my_file_local_information << " Est";
//       my_file_local_information << ",";
//       upCount++;
//     }
//     else if(i % message_size == 1)
//     {
//       my_file_local_information << "Source of ";
//       my_file_local_information << processName;
//       my_file_local_information << ",";
//       my_file_local_information << ",";
//       processName++;
//       upCount++;
//     }
//     else if(i % message_size == 2 )
//     {
//       my_file_local_information << "Up Rate for Comp";
//       upCount++;
//     }
//     else
//     {
//       my_file_local_information << ",";
//     }
//   }
//
//   my_file_local_information << "\n";
//
//   my_file_local_information << "information_for_iteration: ";
//   my_file_local_information << ",";
//   my_file_local_information << ",";
//
//   upCount = 0;
//
//   for(int i = 0 ; i < buffer_size; i++)
//   {
//     if(i%message_size == 0)
//     {
//       my_file_local_information << information_for_iteration[upCount];
//       my_file_local_information << ",";
//       upCount++;
//     }
//     else if(i % message_size == 1)
//     {
//       my_file_local_information << information_for_iteration[upCount];
//       my_file_local_information << ",";
//       my_file_local_information << ",";
//       upCount++;
//     }
//     else if(i % message_size == 2 )
//     {
//       my_file_local_information << information_for_iteration[upCount];
//
//
//       upCount++;
//     }
//     else
//     {
//       my_file_local_information << ",";
//     }
//
//
//
// }
// my_file_local_information << information_for_iteration[information_for_iteration_size-1];
// my_file_local_information << ",";
//
//   my_file_local_information << "\n";
//   my_file_local_information << "\n";
//   my_file_local_information << "\n";
//
//
//     // if(rank == 0 ){
//     //   std::cout << "Process " << rank << " has information_for_iteration: ";
//     //   for(int i=0 ; i<information_for_iteration_size; i++){
//     //     std::cout << information_for_iteration[i] << " ";
//     //   }
//     //   std::cout << std::endl;
//     // }
//
// }
//
//
// static void endSaveToFile(std::ofstream & my_file_local_information)
// {
//   my_file_local_information << "eof";
// }
