#ifndef SKYNET_SOCKETCOMMUNICATORSERVERFACTORY_HPP__
#define SKYNET_SOCKETCOMMUNICATORSERVERFACTORY_HPP__

#include <arpa/inet.h>
#include "Skynet_CommunicatorFactory.hpp"
#include "Skynet_SocketCommunicator.hpp"


namespace skynet
{
  class SocketCommunicatorFactory : public CommunicatorFactory
  {
  public:
    using data_type = std::vector<std::unique_ptr<DeviceCommunicator>>;
    static const int IPv4 = AF_INET;
    static const int QUEUE_LENGTH = 10;
  public:
      SocketCommunicatorFactory() = default;
 /** \brief Create a new SocketCommunicatorFactory.
     *
     * The ip_address of the device we wish to communicated is needed.
     * For now the ip_addresses are a list of its that can be sorted easily.
     * This will need to change once we are not working on one machine.
     */
    SocketCommunicatorFactory(int type, const char * client_address,
      SocketCommunicator gateway) : type_(type), client_address_(client_address)
    {
      is_server = true;
      gateway_ = std::make_unique<SocketCommunicator>(std::move(gateway));
      gateway_->listen_for_clients(QUEUE_LENGTH);
    }

    SocketCommunicatorFactory(int type, const char * server_address,
      uint16_t skynet_port) : type_(type), server_address_(server_address)
    {
      confirm_supported_type();
      is_server = false;
      // connect to Gatekeeper on server device using handshake SocketCommunicator
      std::unique_ptr<SocketCommunicator> handshake =
        std::make_unique<SocketCommunicator>(type_);
      handshake->connect_to_server(server_address_, skynet_port);
      // obtain gateway port on server via server Gatekeeper, then close
      // handshake SocketCommunicator
      server_port_ = handshake->receive_from<uint16_t>();
      handshake->close_communicator();
    }

    std::unique_ptr<DeviceCommunicator>
    create_new_communicator(std::vector<std::string> /* comm_config_info*/)
    {
      if (is_server)
        return create_new_communicator_as_server();
      else
        return create_new_communicator_as_client();
    }


  private:

    void confirm_supported_type() const
    {
      // check that socket type is supported
      if (type_ != IPv4)
      {
        printf("Incorrect socket type in SocketCommunicatorFactory\n");
        exit(-1);
      }
    }

    std::unique_ptr<SocketCommunicator>
    create_new_communicator_as_server()
    {
      std::unique_ptr<SocketCommunicator> new_communicator;
      // Create new communicator and accept client via gateway socket
      new_communicator = std::make_unique<SocketCommunicator>(type_);
      new_communicator->wait_for_client(gateway_->get_socket_handle());
      return new_communicator;
    }

    std::unique_ptr<SocketCommunicator>
    create_new_communicator_as_client()
    {
      std::unique_ptr<SocketCommunicator> new_communicator;
      // Create new communicator and connect via server gateway
      new_communicator = std::make_unique<SocketCommunicator>(type_);
      new_communicator->connect_to_server(server_address_, server_port_);
      return new_communicator;
    }

    int type_;
    const char * client_address_;
    const char * server_address_;
    bool is_server;
    uint16_t server_port_;
    std::unique_ptr<SocketCommunicator> gateway_;
    char ipv4Buf_[INET_ADDRSTRLEN];
  }; // SocketCommunicatorFactory
} // namespace skynet

#endif /* SKYNET_SOCKETCOMMUNICATORSERVERFACTORY_HPP__ */
