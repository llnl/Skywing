#ifndef SKYNET_SOCKETGATEKEEPER_HPP__
#define SKYNET_SOCKETGATEKEEPER_HPP__

#include <arpa/inet.h>
#include "Skynet_SocketCommunicator.hpp"
#include "Skynet_SocketCommunicatorFactory.hpp"

namespace skynet
{
  class SocketGatekeeper
  {
  public:

    static const int IPv4 = AF_INET;
    static const int QUEUE_LENGTH = 10;

    SocketGatekeeper(int type, uint16_t skynet_port) :
      type_(type), skynet_port_(skynet_port)
    {
      confirm_supported_type();
      gatekeeper_ = std::make_unique<SocketCommunicator>(SocketCommunicator(type_));
      if (gatekeeper_->bind_communicator(skynet_port) != skynet_port)
      {
        printf("Skynet socket port is not open for gatekeeper SocketCommunicator\n");
        exit(-1);
      }
      gatekeeper_->listen_for_clients(QUEUE_LENGTH);
    }

    ~SocketGatekeeper()
    { gatekeeper_->close_communicator(); }

    std::vector<std::unique_ptr<SocketCommunicatorFactory>>
    collect_connections()
    {
      int count;
      uint16_t port;
      const char * client_address;
      std::unique_ptr<SocketCommunicator> tmp;
      std::vector<std::unique_ptr<SocketCommunicatorFactory>> new_factories(QUEUE_LENGTH);
      do
      {
        count = gatekeeper_->count_pending_clients();
        if (count > 0)
        {
          // have the tmp SocketCommunicator accept the next client
          tmp = std::make_unique<SocketCommunicator>(SocketCommunicator(type_));
          client_address = tmp->wait_for_client(gatekeeper_->get_socket_handle(), ipv4Buf_); //TODO: generalize from IPv4
          // bind a new gateway socket
          SocketCommunicator gateway = SocketCommunicator(type_);
          port = gateway.bind_communicator(skynet_port_+1, client_address);
          // Create server SocketCommunicatorFactory with bound gateway socket
          new_factories.push_back(
            std::make_unique<SocketCommunicatorFactory>(
              SocketCommunicatorFactory(type_, client_address, std::make_unique<SocketCommunicator>(gateway))));
          // Inform client of port the server SocketCommunicatorFactory is using
          tmp->send_to<uint16_t>(port);
          tmp->close_communicator();
        }
      }while (count > 0);
      return new_factories;
    }

  private:

    void confirm_supported_type() const
    {
      // check that socket type is supported
      if (type_ != IPv4)
      {
        printf("Incorrect socket type in SocketGatekeeper\n");
        exit(-1);
      }
    }

    int type_;
    uint16_t skynet_port_;
    std::unique_ptr<SocketCommunicator> gatekeeper_;
    char ipv4Buf_[INET_ADDRSTRLEN];

  }; // class SocketGateKeeper
} // namespace ns3

#endif /* SKYNET_SOCKETGATEKEEPER_HPP__ */
