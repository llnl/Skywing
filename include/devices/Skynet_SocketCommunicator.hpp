#ifndef SKYNET_SOCKETCOMMUNICATOR_HPP__
#define SKYNET_SOCKETCOMMUNICATOR_HPP__

#include "Skynet_DeviceCommunicator.hpp"
// #include "Skynet_SocketCommunicatorFactory.hpp"

#include <cstdint>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
 #include <arpa/inet.h>
#define MAX 80
#define SA struct sockaddr

namespace skynet
{
  class SocketCommunicator : public DeviceCommunicator
  {
  public:

    static const int IPv4 = AF_INET; // redefinition of AF_INET const
    static const int QUEUE_LENGTH = 10; // length of listening socket queue

    /** \brief Construct a new SocketCommunicator.
     *
     * \param type Specifies the address type to be used.
     */
    SocketCommunicator(int type)
    {
      type_ = type;
      confirm_supported_type();
      socket_ = socket(type_, SOCK_STREAM, 0);
      // socket create and verification
      if (socket_ == -1) {
        printf("socket creation failed...\n");
        exit(0);
      }
      else
        printf("Socket successfully created..\n");
    }

    /** \brief Bind the socket that belongs to this communicator.
     *
     * \param port Which port number to start trying to bind to.
     * \param client_address Optional parameter to specify client address.
     */
    uint16_t bind_communicator(uint16_t port, const char * client_address = NULL)
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
      while (!bound && port < UINT16_MAX)
      {
        printf("trying server port %d\n", port);
        servaddr.sin_port = htons(port);
        // Binding newly created socket to given IP and verification
        if (bind(socket_, (SA*)&servaddr, sizeof(servaddr)) == 0)
          bound = true;
        else
        {
          printf("socket bind failed...\n");
          port++;
        }
      }
      printf("Server Socket successfully bound\n");

      return port;
    }

    /** \brief Set socket that belongs to this communictor to listen for clients
     *
     */
    void listen_for_clients()
    {
      //listening for a connection
      listen(socket_, QUEUE_LENGTH);
      //[TODO] AF: Vericatication was not working, need to look into this!
      // Now server is ready to listen and verification
      // if ((listen(sockfd_, 5)) != 0) {
      //     printf("Listen failed...\n");
      //     exit(0);
      // }
      // else
      //     printf("Server listening..\n");
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
    const char * wait_for_client(int listener_socket, char * buf = NULL)
    {
      struct sockaddr_in client_address;
      socklen_t len = sizeof(client_address);

      // Accept the data packet from client and verification
      socket_ = accept(listener_socket, (struct sockaddr *) &client_address, &len);
      if (socket_ < 0)
      {
        printf("server acccept failed...\n");
        exit(0);
      }
      else
        printf("server acccept the client...\n");

      if (buf != NULL)
      {
        switch(type_)
        {
          case AF_INET:
            return inet_ntop(AF_INET, &(client_address.sin_addr), buf, INET_ADDRSTRLEN);
          default:
            printf("Incorrect socket type in SocketCommunicator::wait_for_client\n");
            exit(-1);
        }
      }
      return NULL;
    }

    /** \brief Connect the socket that belongs to this communicator.
     *
     * \param server_address The address of the server communicator.
     * \param port Which port number to connect to on the server.
     */
    void connect_to_server(const char * server_address, uint16_t port)
    {
      //Socket stucture
      struct sockaddr_in servaddr;
      bzero(&servaddr, sizeof(servaddr));

      switch(type_)
      {
        case AF_INET:
          servaddr.sin_family = AF_INET;
          inet_pton(AF_INET, server_address, &(servaddr.sin_addr));
          //servaddr.sin_addr.s_addr = inet_addr(ip_address);
          break;
        default:
          printf("Incorrect socket type in SocketCommunicator::connect_to_server\n");
          exit(-1);
      }
      servaddr.sin_port = ntohs(port);

      // connect the client socket to server socket
      if (connect(socket_, (SA*)&servaddr, sizeof(servaddr)) != 0)
      {
        printf("connection with the server failed...\n");
        exit(-1);
      }
      printf("connected to the server..\n");
    }

    /** \brief Count the number of connection requests that are pending.
     *
     * \param port Which port number to start trying to bind to.
     * \param client_address Optional parameter to specify client address.
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

    int get_socket_handle()
    { return socket_; }

    void close_communicator()
    { close(socket_); }

  private:

    /** \brief Confirm that this object's address type is supported.
     *
     * Exits with an error message if the type is not supported
     */
    void confirm_supported_type()
    {
      if (type_ != IPv4)
      {
        printf("Incorrect socket type in SocketCommunicator\n");
        exit(-1);
      }
    }


    void do_send_to_(const void* data, std::size_t data_size) const override
    {
      uint16_t networkLen = htons(data_size); // convert to network byte order
      write(socket_, &networkLen, sizeof(networkLen)); //sends the size of the data first
      write(socket_, data, data_size); //sends the seralized data
    }


    std::vector<char> do_receive_from_() const override
    {

    //[TODO] AF: this could be more efficent, the read() fucntion seems to only take a array of char and not a vector....
      // char msg[1024];
      // std::cout<<"in receiving "<<std::endl;

      uint16_t networkLen;
      read(socket_, &networkLen, sizeof(networkLen));
      // std::cout<<"networkLen "<<networkLen<<std::endl;

      uint16_t len = ntohs(networkLen); // convert back to host byte order
      char msg[len];

      int a = read(socket_, msg, len);
      if(a ==-1){
        std::cout<<"Error in reading data"<<std::endl;
      }
      msg[len] = '\0';

      //Hack way to get it into the correct format for the serialzier.
      std::vector<char> data(len);
      for(int i = 0; i<len; i++){
        data[i] = msg[i];
        // std::cout<<"data[i] ="<<data[i]<<std::endl;
      }
      return data;
    }

  private:
    int socket_;
    int type_;

  }; // class SocketCommunicator
} // namespace skynet


#endif /* SKYNET_MPICOMMUNICATOR_HPP__ */
