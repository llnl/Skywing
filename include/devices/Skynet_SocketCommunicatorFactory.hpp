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
  public:
    /** \brief Create a new server SocketCommunicatorFactory.
     *
     * \param type Specifies the address type to be used (IPv4).
     * \param client_address The IP address of the client to connect to this
     * \param gateway The SocketCommunicator for listening for incoming connections
     */
    SocketCommunicatorFactory(int type, const char * client_address,
      SocketCommunicator&& gateway) : type_(type), client_address_(client_address), gateway_(gateway)
    {
      is_server = true;
      gateway_.listen_for_clients();
    }

    /** \brief Create a new client SocketCommunicatorFactory.
     *
     * \param type Specifies the address type to be used (IPv4).
     * \param server_address The IP address of the server to connect to
     * \param skynet_port The specific port that all SocketGatekeepers will be
     * listening on
     *
     * Note that gateway_ here is instantiated but not used
     */
    SocketCommunicatorFactory(int type, const char * server_address,
      uint16_t skynet_port) : type_(type), server_address_(server_address), gateway_(type)
    {
      is_server = false;
      // connect to Gatekeeper on server device using handshake SocketCommunicator
      SocketCommunicator handshake(type_);
      handshake.connect_to_server(server_address_, skynet_port);
      // obtain gateway port on server via server Gatekeeper, then close
      // handshake SocketCommunicator
      server_port_ = handshake.receive_from<uint16_t>();
      handshake.close_communicator();
    }

    /** \brief Destruct a SocketCommunicatorFactory
     *
     */
    ~SocketCommunicatorFactory()
    { gateway_.close_communicator(); }

    std::unique_ptr<DeviceCommunicator>
    create_new_communicator(std::vector<std::string> /* comm_config_info*/)
    {
      std::unique_ptr<SocketCommunicator> new_communicator;
      new_communicator = std::make_unique<SocketCommunicator>(type_);
      if (is_server) // accept client via gateway socket
        new_communicator->wait_for_client(gateway_.get_socket_handle());
      else // connect via server gateway
        new_communicator = std::make_unique<SocketCommunicator>(type_);
      return new_communicator;
    }

  private:

    int type_;
    const char * client_address_;
    const char * server_address_;
    bool is_server;
    uint16_t server_port_;
    SocketCommunicator gateway_;
    char ipv4Buf_[INET_ADDRSTRLEN];
  }; // SocketCommunicatorFactory
} // namespace skynet

#endif /* SKYNET_SOCKETCOMMUNICATORSERVERFACTORY_HPP__ */
