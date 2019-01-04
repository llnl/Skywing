#ifndef SKYNET_SOCKETDEVICELISTENER_HPP__
#define SKYNET_SOCKETDEVICELISTENER_HPP__

#include "Skynet_DeviceListener.hpp"
#include "Skynet_Socket.hpp"
#include "Skynet_SocketCommunicator.hpp"

namespace skynet
{
  class SocketDeviceListener : public DeviceListener, public Socket
  {
  public:

    static const int QUEUE_LENGTH = 10;

    /** \brief Construct a new SocketDeviceListener.
     *
     * \param type Specifies the address type to be used.
     */
    SocketDeviceListener(int type, uint16_t port, bool try_other_ports, const char* client_address = NULL) :
      Socket(type), port_(port)
    {
      //Socket stucture
      struct sockaddr_in servaddr;
      bzero(&servaddr, sizeof(servaddr));

      switch(type_)
      {
        case Socket::IPv4:
          servaddr.sin_family = Socket::IPv4;
          if (client_address != NULL)
            inet_pton(Socket::IPv4, client_address, &(servaddr.sin_addr));
          else
            servaddr.sin_addr.s_addr = INADDR_ANY;
          break;
        default:
          // this should never be reached because a check in Socket constructor
          printf("Error in SocketDeviceListener:38\n");
          exit(-1);
      }

      bool bound = false;
      do
      {
        servaddr.sin_port = htons(port_);
        // Binding newly created socket to given IP and verification
        if (bind(socket_, (struct sockaddr*)&servaddr, sizeof(servaddr)) == 0)
          bound = true;
        else
        {
          if (try_other_ports)
            port_++;
          else
          {
            perror("bind");
            exit(-1);
          }
        }
      } while (!bound && port_ < UINT16_MAX);

      listen(socket_, QUEUE_LENGTH);
    }


    /** \brief Connect a communicator in a pending Device
     *
     * \return a connect DeviceCommunicator
     */
    std::unique_ptr<DeviceCommunicator> connect_communicator_to_client()
    {
      struct sockaddr_in client_address_struct;
      const char * client_address;
      int new_socket = connect_new_socket();
      switch(type_)
      {
        case Socket::IPv4:
          client_address = inet_ntop(Socket::IPv4, &(client_address_struct.sin_addr), ipv4Buf_, INET_ADDRSTRLEN);
          break;
        default:
          // this should never be reached because a check in Socket constructor
          printf("Error in SocketDeviceListener.hpp:89\n");
          exit(-1);
      }

      return std::make_unique<SocketCommunicator>(type_, new_socket, client_address);
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
      FD_SET(socket_, &set);
      timeout.tv_sec = 0;
      timeout.tv_usec = 0;

      return select(socket_ + 1, &set, NULL, NULL, &timeout);
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
    { return socket_; }

  private:

    int connect_new_socket()
    {
      struct sockaddr_in client_address_struct;
      socklen_t len = sizeof(client_address_struct);

      // Accept the data packet from client and verification
      int new_socket = accept(socket_, (struct sockaddr *) &client_address_struct, &len);
      if (new_socket < 0)
      {
        perror("accept");
        exit(-1);
      }

      return new_socket;
    }

    char ipv4Buf_[INET_ADDRSTRLEN];
    uint16_t port_;

  }; // class SocketDeviceListener
} // namespace skynet


#endif /* SKYNET_DEVICELISTENER_HPP__ */
