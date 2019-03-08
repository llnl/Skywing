#ifndef SKYNET_SOCKETCOMMUNICATORSERVERFACTORY_HPP__
#define SKYNET_SOCKETCOMMUNICATORSERVERFACTORY_HPP__

#include <arpa/inet.h>
#include "Skynet_CommunicatorFactory.hpp"
#include "Skynet_SocketCommunicator.hpp"
#include "Skynet_SocketListener.hpp"

namespace skynet
{
  class SocketCommunicatorFactory : public CommunicatorFactory
  {
  public:
    using data_type = std::vector<std::unique_ptr<DeviceCommunicator>>;
  public:
    /** \brief Create a new server SocketCommunicatorFactory.
     *
     * create a SocketListener for this factory and then use Gateway listener
     * to create a handshake communicator to communicate factory listening port
     * back to client
     *
     * \param type Specifies the address type to be used (IPv4).
     * \param skynet_port Specifies the port used by the Gateway
     */
    SocketCommunicatorFactory(int type, uint16_t skynet_port,
      SocketListener& gateway_listener)
    {
      type_ = type;
      is_server = true;
      listener_ = std::make_unique<SocketListener>(type_, skynet_port+1, true);
      std::unique_ptr<SocketCommunicator> handshake =
        gateway_listener.connect_communicator_to_client();
      handshake->send_to<uint16_t>(listener_->get_port());
    }

    /** \brief Create a new client SocketCommunicatorFactory.
     *
     * communicate with server SocketCommunicatorFactory to determine what port
     * the server SocketListener is listening on
     *
     * \param type Specifies the address type to be used (IPv4).
     * \param server_address The IP address of the server to connect to
     * \param skynet_port The specific port that all SocketGatekeepers will be
     * listening on
     *
     */
    SocketCommunicatorFactory(int type, const char * server_address,
      uint16_t skynet_port) : type_(type), server_address_(server_address)
    {
      // TODO: implement error catching if this process failes
      is_server = false;
      // connect to Gatekeeper on server device using handshake SocketCommunicator
      SocketCommunicator handshake(type_);
      handshake.connect_to_server(server_address_, skynet_port);
      // obtain gateway port on server via server Gatekeeper, then close
      // handshake SocketCommunicator
      server_port_ = handshake.receive_from<uint16_t>();
    }

    std::unique_ptr<DeviceCommunicator>
    create_new_communicator(std::vector<std::string> /* comm_config_info*/)
    {
      if (is_server) // accept client via gateway socket
        return listener_->connect_communicator_to_client();
      else // connect via server gateway
      {
        std::unique_ptr<SocketCommunicator> new_communicator =
          std::make_unique<SocketCommunicator>(type_);
        new_communicator->connect_to_server(server_address_, server_port_);
        return new_communicator;
      }
    }

  private:

    int type_;
    const char * server_address_;
    bool is_server;
    uint16_t server_port_;
    std::unique_ptr<SocketListener> listener_;
  }; // SocketCommunicatorFactory
} // namespace skynet

#endif /* SKYNET_SOCKETCOMMUNICATORFACTORY_HPP__ */
