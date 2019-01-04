#ifndef SKYNET_SOCKETDEVICELISTENER_HPP__
#define SKYNET_SOCKETDEVICELISTENER_HPP__

#include "Skynet_DeviceListener.hpp"
#include "Skynet_Socket.hpp"
#include "Skynet_SocketCommunicator.hpp"

//#include <cstdint>
//#include <stdio.h>
//#include <stdlib.h>
//#include <unistd.h>
//#include <string.h>
//#include <sys/types.h>
//#include <sys/socket.h>
//#include <netinet/in.h>
//#include <netdb.h>
//#include <netinet/in.h>
//#include <stdlib.h>
//#include <string.h>
//#include <sys/socket.h>
//#include <sys/types.h>
//#include <arpa/inet.h>
//#define SA struct sockaddr

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
        case AF_INET:
          servaddr.sin_family = AF_INET;
          if (client_address != NULL)
            inet_pton(AF_INET, client_address, &(servaddr.sin_addr));
          else
            servaddr.sin_addr.s_addr = INADDR_ANY;
          break;
        default:
          // TODO: error handling
          printf("Incorrect socket type in SocketCommunnicator::bind_communicator\n");
          exit(-1);
      }

      bool bound = false;
      do
      {
        printf("trying server port %d\n", port_);
        servaddr.sin_port = htons(port_);
        // Binding newly created socket to given IP and verification
        if (bind(socket_, (struct sockaddr*)&servaddr, sizeof(servaddr)) == 0)
          bound = true;
        else
        {
          printf("socket bind failed...\n");
          if (try_other_ports)
            port_++;
          else
            exit(-1);
        }
      } while (!bound && port_ < UINT16_MAX);
      printf("Server Socket successfully bound\n");
    }

    /** \brief Set socket that belongs to this communictor to listen for clients
     *
     */
    void listen_for_clients()
    {
      //listening for a connection
      listen(socket_, QUEUE_LENGTH);
    }

    /** \brief Wait for client to connect this communicator
     *
     * \param listener_socket Identifier that specifies listener socket.
     * \param buf Optional buffer used to store client address.
     *
     * \return Address of client that has connected.
     *
     * Note that this function will block until a client connects.
     */
    std::unique_ptr<DeviceCommunicator> connect_communicator_to_client() override
    {
      struct sockaddr_in client_address_struct;
      const char * client_address;
      int new_socket = connect_new_socket();
      switch(type_)
      {
        case AF_INET:
          client_address = inet_ntop(AF_INET, &(client_address_struct.sin_addr), ipv4Buf_, INET_ADDRSTRLEN);
        default:
          printf("Incorrect socket type in SocketCommunicator::wait_for_client\n");
          exit(-1);
      }
      // TODO: figure out a way to make use of client address

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
        printf("server acccept failed...\n");
        exit(-1);
      }
      else
        printf("server acccept the client...\n");

      return new_socket;
    }

    char ipv4Buf_[INET_ADDRSTRLEN];
    uint16_t port_;

  }; // class SocketDeviceListener
} // namespace skynet


#endif /* SKYNET_DEVICELISTENER_HPP__ */
