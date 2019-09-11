#ifndef NS3_SOCKETCOMMUNICATORFACTORY_HPP__
#define NS3_SOCKETCOMMUNICATORFACTORY_HPP__

#include "devices/Skynet_DeviceCommunicator.hpp"
#include "devices/Skynet_SocketCommunicatorFactory.hpp"
#include "ns3/object-factory.h"
#include "ns3/ptr.h"
#include "ns3/socket.h"
#include "Ns3_SocketCommunicator.hpp"

namespace ns3
{
  class SocketCommunicatorFactory : public skynet::SocketCommunicatorFactory
  {
  public:

    SocketCommunicatorFactory(int type, uint16_t port_start, Ptr<Node> node)
      : skynet::SocketCommunicatorFactory(type, port_start), node_(node)
    { factory_.SetTypeId(SocketCommunicator::GetTypeId()); }

    SocketCommunicatorFactory(int type, const char * server_address, uint16_t port_start, Ptr<Node> node)
      : skynet::SocketCommunicatorFactory(type, server_address, port_start)
    { factory_.SetTypeId(SocketCommunicator::GetTypeId()); }

    // TODO: try to find a way that this does not need to be reproduced here
    std::unique_ptr<skynet::DeviceCommunicator>
    create_new_communicator(std::vector<std::string> /* comm_config_info*/) override
    {
      std::unique_ptr<SocketCommunicator> sc;
      // if factory is server side
      if ( std::strcmp(server_address_ ,"") == 0)
      {
        factory_.Set("Port", UintegerValue(port_));
        factory_.Set("Address", AddressValue());
        Ptr<Application> app = factory_.Create<SocketCommunicator>();
        node_->AddApplication(app);
        app->SetStartTime(Seconds (0.0));
        //sc = std::make_unique<SocketCommunicator>(SocketCommunicator(port_, type_));
      }
      // if factory is client side
      else
      {
        factory_.Set("Port", UintegerValue(port_));
        factory_.Set("Address", Ipv4AddressValue(server_address_));
      }
      Ptr<Application> app = factory_.Create<SocketCommunicator>();
      node_->AddApplication(app);
      app->SetStartTime(Seconds (0.0));

      return sc;
    }

  private:

    Ptr<Node> node_;
    ObjectFactory factory_;

  }; // class SocketCommunicatorFactory

} // namespace ns3

#endif /* NS3_SOCKETCOMMUNICATORFACTORY_HPP__ */
