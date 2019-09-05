#ifndef SKYNET_SOCKETLISTENER_HPP__
#define SKYNET_SOCKETLISTENER_HPP__

#include "Skynet_Socket.hpp"
#include "Skynet_SocketCommunicator.hpp"

namespace skynet
{
  class SocketListener
  {
  public:
    static constexpr int queue_length = 10;

    /** \brief Construct a new SocketListener.
     *
     * \param address_type Specifies the address type to be used.
     * \param port The port to bind to
     * \param try_other_ports If other ports should be tried if the initial one is taken
     * \param client_address The address to listen to
     */
    SocketListener(
      const int address_type,
      const uint16_t port,
      const bool try_other_ports,
      const char* const client_address = nullptr
    )
      : socket_(address_type),
        port_(socket_.bind_to_port(port, try_other_ports, client_address))
    {
      socket_.set_to_listen(queue_length);
    }

    /** \brief Connect a communicator in a pending Device
     *
     * \return a connected DeviceCommunicator
     */
    SocketCommunicator connect_communicator_to_client() const
    {
      return SocketCommunicator(socket_.accept());
    }

    /** \brief Count the number of connection requests that are pending.
     *
     * \return Number of connection requests that are pending.
     */
    int count_pending_clients() const
    { return socket_.query_queue(); }

    /** \brief Obtain the port this listener bound to
     *
     * \return port number
     */
    uint16_t get_socket_port() const
    { return port_; }

  private:
    Socket socket_;
    std::uint16_t port_;
  }; // class SocketListener
} // namespace skynet


#endif /* SKYNET_SOCKETLISTENER_HPP__ */
