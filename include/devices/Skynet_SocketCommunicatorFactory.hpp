#ifndef SKYNET_SOCKETCOMMUNICATORFACTORY_HPP__
#define SKYNET_SOCKETCOMMUNICATORFACTORY_HPP__

// #include "mpi.h"
#include <memory>
#include "Skynet_CommunicatorFactory.hpp"
#include "Skynet_SocketCommunicator.hpp"

namespace skynet
{
  class SocketCommunicatorFactory : public CommunicatorFactory
  {
  public:
    std::unique_ptr<DeviceCommunicator> 
    create_new_communicator(std::vector<std::string> comm_config_info)
    {
      std::string ip_address = "192.0.0.1";
      return SocketCommunicator(ip_address);
    }

  private:

  }; // SocketCommunicatorFactory
} // namespace skynet

#endif /* SKYNET_SOCKETCOMMUNICATORFACTORY_HPP__ */
