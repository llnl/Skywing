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
    MPICommunicatorFactory()
      : tag_(0)
    {}

    std::unique_ptr<DeviceCommunicator> 
    create_new_communicator(std::vector<std::string> comm_config_info)
    {
      MPI_Comm newcomm;
      MPI_Comm_create_group(world_comm_, p2p_group_, 0, &newcomm);
      return MPICommunicator(newcomm);
    }

  private:
    MPI_Comm world_comm_;
    MPI_Group p2p_group_; // group containing just this and the other device
    int tag_;

  }; // MPICommunicatorFactory
} // namespace skynet

#endif /* SKYNET_MPICOMMUNICATORFACTORY_HPP__ */
