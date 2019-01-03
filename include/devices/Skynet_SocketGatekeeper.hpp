#ifndef SKYNET_SOCKETGATEKEEPER_HPP__
#define SKYNET_SOCKETGATEKEEPER_HPP__

#include <arpa/inet.h>
#include "Skynet_SocketCommunicator.hpp"
#include "Skynet_SocketCommunicatorFactory.hpp"

namespace skynet
{
  /** \class SocketGatekeeper
   * \brief Object responsible for creating and maintaining a gatekeeper
   * SocketCommunicator.
   *
   */
  class SocketGatekeeper
  {
  public:

    /** \brief Construct a new SocketGatekeeper.
     *
     * \param type Specifies the address type to be used (IPv4).
     * \param skynet_port The specific port that all SocketGatekeepers will be
     * listening on
     */
    SocketGatekeeper(int type, uint16_t skynet_port) :
      type_(type), skynet_port_(skynet_port)
    {
      SocketCommunicator::confirm_supported_type(type_);
      gatekeeper_ = std::make_unique<SocketCommunicator>(type_);
      if (gatekeeper_->bind_communicator(skynet_port) != skynet_port)
      {
        printf("Skynet socket port is not open for gatekeeper SocketCommunicator\n");
        exit(-1);
      }
      gatekeeper_->listen_for_clients();
    }

    /** \brief Deconstruct a SocketGatekeeper.
     *
     */
    ~SocketGatekeeper()
    { gatekeeper_->close_communicator(); }

    /** \brief Collect all connection requests that in the gatekeeper queue.
     *
     * \return Vector of SocketCommunicatorFactory objects, one for each
     * connection request in the gatekeeper queue
     */
    std::vector<std::unique_ptr<SocketCommunicatorFactory>>
    collect_connections()
    {
      int count;
      uint16_t port;
      const char * client_address;
      std::unique_ptr<SocketCommunicator> handshake;
      std::vector<std::unique_ptr<SocketCommunicatorFactory>>
        new_factories(SocketCommunicator::QUEUE_LENGTH);
      do
      {
        count = gatekeeper_->count_pending_clients();
        if (count > 0)
        {
          // have the handshake SocketCommunicator accept the next client
          handshake = std::make_unique<SocketCommunicator>(type_);
          client_address = handshake->wait_for_client(
            gatekeeper_->get_socket_handle(), ipv4Buf_); //TODO: generalize from IPv4
          // bind a new gateway socket
          SocketCommunicator gateway = SocketCommunicator(type_);
          port = gateway.bind_communicator(skynet_port_+1, client_address);
          // Create server SocketCommunicatorFactory with bound gateway socket
          new_factories.push_back(
            std::make_unique<SocketCommunicatorFactory>(type_, client_address,
            gateway));
          // Inform client of port the server SocketCommunicatorFactory is using
          handshake->send_to<uint16_t>(port);
          handshake->close_communicator();
        }
      }while (count > 0);
      return new_factories;
    }

  private:

    int type_;
    uint16_t skynet_port_;
    std::unique_ptr<SocketCommunicator> gatekeeper_;
    char ipv4Buf_[INET_ADDRSTRLEN];

  }; // class SocketGateKeeper
} // namespace ns3

#endif /* SKYNET_SOCKETGATEKEEPER_HPP__ */
