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
     /** \brief Create a new SocketCommunicatorFactory.
     *
     * The ip_address of the device we wish to communicated is needed.
     */
    SocketCommunicatorFactory(std::string ip_add)
      : ip_address_(ip_add)
    { }


    std::unique_ptr<DeviceCommunicator> 
    create_new_communicator(std::vector<std::string> comm_config_info)
    {
      // std::string ip_address = "192.0.0.1";
      return SocketCommunicator(ip_address);
    }

  private:
  std::string ip_address_
  }; // SocketCommunicatorFactory
} // namespace skynet

#endif /* SKYNET_SOCKETCOMMUNICATORFACTORY_HPP__ */
