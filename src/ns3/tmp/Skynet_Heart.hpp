#ifndef SKYNET_HEART_HPP__
#define SKYNET_HEART_HPP__

#include <utility>
#include <vector>
#include <thread>
#include "devices/Skynet_SocketCommunicatorFactory.hpp"

namespace skynet
{
  /** \class Temporary Heart Class
   *  \brief This is only to be a temporary container to get ns3 working
   *
   */
  class Heart
  {
  public:
    Heart()
    {}

    ~Heart()
    {}

    /** \brief Begin the heartbeat. */
    void begin_heartbeat(std::vector<const char *> ip_addresses, uint16_t port)
    {
      std::vector<std::string> config(0);

      // for each ip_address, create a client communicator
      for (uint i = 0; i < ip_addresses.size(); i++)
      {
        SocketCommunicatorFactory client_factory(
          SocketCommunicatorFactory::IPv4, ip_addresses[i], port);
        comm_list.push_back(client_factory.create_new_communicator(config));
      }

      // continuous create server communicators as clients connect to them
      SocketCommunicatorFactory server_factory(SocketCommunicatorFactory::IPv4, port);
      while (true)
        comm_list.push_back(server_factory.create_new_communicator(config));
    }

  private:
    std::vector<std::unique_ptr<DeviceCommunicator>> comm_list;

  }; // class Heart

} // namespace skynet

#endif /* SKYNET_HEART_HPP__ */
