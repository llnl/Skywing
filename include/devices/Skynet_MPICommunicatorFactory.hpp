#ifndef SKYNET_MPICOMMUNICATORFACTORY_HPP__
#define SKYNET_MPICOMMUNICATORFACTORY_HPP__

#include "mpi.h"
#include <memory>
#include "Skynet_CommunicatorFactory.hpp"
#include "Skynet_MPICommunicator.hpp"

namespace skynet
{
  class MPICommunicatorFactory : public CommunicatoryFactory
  {
  public:
    MPICommunicatorFactory(int other_rank)
      : comm_(MPI_COMM_WORLD), tag_(0), other_rank_(other_rank)
    { }

    MPICommunicatorFactory(MPI_Comm comm, int other_rank)
      : comm_(comm), tag_(0), other_rank_(other_rank)
    { }

    std::unique_ptr<DeviceCommunicator> 
    create_new_communicator(std::vector<std::string> comm_config_info)
    {

      return MPICommunicator(newcomm);
    }

  private:
    MPI_Comm comm_;
    int tag_;
    int other_rank_;

  }; // MPICommunicatorFactory
} // namespace skynet

#endif /* SKYNET_MPICOMMUNICATORFACTORY_HPP__ */
