#ifndef SKYNET_SOCKETGATEWAY_HPP__
#define SKYNET_SOCKETGATEWAY_HPP__

#include <arpa/inet.h>
#include "Skynet_Gateway.hpp"
#include "Skynet_SocketCommunicatorFactory.hpp"
#include "Skynet_SocketListener.hpp"

namespace skynet
{
  /** \class SocketGateway
   * \brief Implementation of Gateway for SocketCommunicatorFactory
   *
   */
  class SocketGateway : public Gateway
  {
  public:

    /** \brief Construct a new SocketGateway.
     *
     * \param type Specifies the address type to be used (IPv4).
     * \param skynet_port The specific port that all SocketGateways will be
     * listening on
     */
    SocketGateway(int type, uint16_t skynet_port) :
      type_(type), skynet_port_(skynet_port),
      listener_(type, skynet_port, false)
    { }

  private:

    /** \brief Determine if there is a connection request
     *
     * \return whether there is a connection request
     */
    bool client_requesting_connection()
    { return listener_.count_pending_clients() > 0; }

    /** \brief Create new SocketCommunicatorFactory using a provided
     *  SocketDeviceCommunicator
     *
     * \return a unique_ptr to a new SocketCommunicatorFactory
     */
    std::unique_ptr<CommunicatorFactory> create_new_factory()
    {
      // create a new SocketListener to be used by SocketCommunicatorFactory
      SocketListener new_listener(type_, skynet_port_+1, true);
      // create a new SocketCommunicator connected to client and send
      // SocketListener port number back to the client
      std::unique_ptr<SocketCommunicator> handshake =
        listener_.connect_communicator_to_client();
      handshake->send_to<uint16_t>(new_listener.get_port());
      // create SocketCommunicatorFactory
      return std::make_unique<SocketCommunicatorFactory>(type_, std::move(new_listener));
    }

    int type_;
    uint16_t skynet_port_;
    SocketListener listener_;

  }; // class SocketGateway
} // namespace skynet

#endif /* SKYNET_SOCKETGATEWAY_HPP__ */
