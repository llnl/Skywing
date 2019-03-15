#ifndef SKYNET_SOCKETLISTENER_HPP__
#define SKYNET_SOCKETLISTENER_HPP__

#include "Skynet_Socket.hpp"
#include "Skynet_SocketCommunicator.hpp"

namespace skynet
{
  class SocketListener
  {
  public:

    static const int QUEUE_LENGTH = 10;

    /** \brief Construct a new SocketListener.
     *
     * \param address_type Specifies the address type to be used.
     */
    SocketListener(int address_type, uint16_t port, bool try_other_ports, const char* client_address = NULL) :
      socket_(address_type, port)
    {
      socket_.bind_to_port(socket_.get_port(), try_other_ports, client_address);
      socket_.set_to_listen(QUEUE_LENGTH);
    }

    /** \brief Connect a communicator in a pending Device
     *
     * \return a connected DeviceCommunicator
     */
    std::unique_ptr<SocketCommunicator> connect_communicator_to_client()
    {
      return std::make_unique<SocketCommunicator>(socket_);
    }

    /** \brief Count the number of connection requests that are pending.
     *
     * \return Number of connection requests that are pending.
     */
    int count_pending_clients()
    { return socket_.query_queue(); }

    /** \brief Obtain the port this listener bound to
     *
     * \return port number
     */
    uint16_t get_socket_port() const
    { return socket_.get_port(); }

  private:

    Socket socket_;

  }; // class SocketListener
} // namespace skynet


#endif /* SKYNET_SOCKETLISTENER_HPP__ */
