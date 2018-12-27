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
    
    SocketCommunicator(int type) 
    { 
      type_ = type; 
      socket_ = socket(AF_INET, SOCK_STREAM, 0);
      // socket create and verification
      if (socket_ == -1) {
        printf("socket creation failed...\n");
        exit(0);
      }
      else
        printf("Socket successfully created..\n");
    }

    uint16_t bind_communicator(uint16_t port, const char * client_address = "")
    {
      //Socket stucture
      struct sockaddr_in servaddr;
      bzero(&servaddr, sizeof(servaddr));

      switch(type_)
      {
        case AF_INET:
          servaddr.sin_family = AF_INET;
          if (std::strcmp(client_address, "") != 0)
            inet_pton(AF_INET, client_address, &(servaddr.sin_addr));
          else
            servaddr.sin_addr.s_addr = INADDR_ANY;
          break;
        default:
          // TODO: error handling
          printf("incorrect socket type\n");
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

    void wait_for_client()
    { wait_for_client_and_get_address(); }

    const char * wait_for_client(char * buf)
    {
      struct sockaddr_in client_address = wait_for_client_and_get_address();
      switch(type_)
      {
        case AF_INET:
          return inet_ntop(AF_INET, &(client_address.sin_addr), buf, INET_ADDRSTRLEN);
        default:
          printf("incorrect socket type\n");
          exit(-1);
      }
    }

    void connect_to_server(const char * server_address, uint16_t port)
    // : ip_address_{std::move(ip_address)}
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
          printf("incorrect socket type\n");
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

  private:

    struct sockaddr_in wait_for_client_and_get_address()
    {
      struct sockaddr_in client_address;

      //listening for a connection
      listen(socket_, 5);
      //[TODO] AF: Vericatication was not working, need to look into this!
      // Now server is ready to listen and verification
      // if ((listen(sockfd_, 5)) != 0) {
      //     printf("Listen failed...\n");
      //     exit(0);
      // }
      // else
      //     printf("Server listening..\n");
      socklen_t len = sizeof(client_address);

      // Accept the data packet from client and verification
      socket_ = accept(socket_, (struct sockaddr *) &client_address, &len);
      if (socket_ < 0) {
        printf("server acccept failed...\n");
        exit(0);
      }
      else
        printf("server acccept the client...\n");
 
       //[TODO] AF: Typically the sockets are closed at some point but I am not sure where or when yet to do that... May cause problems.
      // close(sockfd__);
      return client_address;
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
    // char ip_address_[4];
    int socket_;
    int type_;

    // TODO: replace this with error checking
    bool success_;

  }; // class MPICommunicator
} // namespace skynet


#endif /* SKYNET_MPICOMMUNICATOR_HPP__ */
