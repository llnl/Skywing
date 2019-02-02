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
      socket_(address_type), port_(port)
    {
      socket_.bind_to_port(port_, try_other_ports, client_address);
      socket_.set_to_listen(QUEUE_LENGTH);
    }


    /** \brief Connect a communicator in a pending Device
     *
     * \return a connect DeviceCommunicator
     */
    std::unique_ptr<SocketCommunicator> connect_communicator_to_client()
    {
      struct sockaddr_in client_address_struct;
      const char * client_address;
      int new_socket = connect_new_socket();
      switch(socket_.get_address_type())
      {
        case Socket::IPv4:
          client_address = inet_ntop(Socket::IPv4, &(client_address_struct.sin_addr), ipv4Buf_, INET_ADDRSTRLEN);
          break;
        default:
          // this should never be reached because a check in Socket constructor
          printf("Error in SocketDeviceListener.hpp:89\n");
          exit(-1);
      }

      return std::make_unique<SocketCommunicator>(socket_.get_address_type(), new_socket, client_address);
    }

    /** \brief Count the number of connection requests that are pending.
     *
     * \return Number of connection requests that are pending.
     */
    int count_pending_clients()
    {
      fd_set set;
      struct timeval timeout;
      FD_ZERO(&set);
      FD_SET(socket_.get_handle(), &set);
      timeout.tv_sec = 0;
      timeout.tv_usec = 0;

      return select(socket_.get_handle() + 1, &set, NULL, NULL, &timeout);
    }

    /** \brief Obtain the port this listener bound to
     *
     * \return port number
     */
    uint16_t get_port() const
    { return port_; }

    /** \brief Obtain the socket handle for this listener
     *
     * \return socket handle
     */
    int get_socket_handle() const
    { return socket_.get_handle(); }

  private:

    int connect_new_socket()
    {
      struct sockaddr_in client_address_struct;
      socklen_t len = sizeof(client_address_struct);

      // Accept the data packet from client and verification
      int new_socket = accept(socket_.get_handle(), (struct sockaddr *) &client_address_struct, &len);
      if (new_socket < 0)
      {
        perror("accept");
        exit(-1);
      }

      return new_socket;
    }

    Socket socket_;
    char ipv4Buf_[INET_ADDRSTRLEN];
    uint16_t port_;

  }; // class SocketListener
} // namespace skynet


#endif /* SKYNET_SOCKETLISTENER_HPP__ */
