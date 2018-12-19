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
    SocketCommunicator(uint16_t port, int type){
      printf("In Server....\n");

      port_ = port;
      //Socket stucture
      int connfd;
      struct sockaddr_in servaddr, cli;

      // socket create and verification
      connfd = socket(type, SOCK_STREAM, 0);
      if (sockfd_ == -1) {
        printf("socket creation failed...\n");
        exit(0);
      }
      else
        printf("Socket successfully created..\n");
      bzero(&servaddr, sizeof(servaddr));


      //[TODO] AF: Need to make this universal for IP!
      // assign IP, PORT
      switch(type)
      {
        case AF_INET:
          servaddr.sin_family = AF_INET;
          servaddr.sin_addr.s_addr= INADDR_ANY;
          break;
        default:
          // TODO: error handling
          printf("incorrect socket type\n");
          exit(-1);
      }
      std::cout<<"Server port = "<<port_<<std::endl;
      servaddr.sin_port = htons(port_);
      // Binding newly created socket to given IP and verification
      if ((bind(connfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
        printf("socket bind failed...\n");
        exit(0);
      }
      else
      printf("Server Socket successfully binded..\n");
      //listening for a connection
      listen(connfd, 5);

      //[TODO] AF: Vericatication was not working, need to look into this!
      // Now server is ready to listen and verification
      // if ((listen(sockfd_, 5)) != 0) {
      //     printf("Listen failed...\n");
      //     exit(0);
      // }
      // else
      //     printf("Server listening..\n");
      socklen_t len = sizeof(cli);

      // Accept the data packet from client and verification
      sockfd_ = accept(connfd, (struct sockaddr *) &cli, &len);
      if (sockfd_ < 0) {
        printf("server acccept failed...\n");
        exit(0);
      }
      else
        printf("server acccept the client...\n");

       //[TODO] AF: Typically the sockets are closed at some point but I am not sure where or when yet to do that... May cause problems.
      // close(sockfd__);
    }


    // for IPv4 addresses
    SocketCommunicator(const char * ip_address, uint16_t port, int type)
    // : ip_address_{std::move(ip_address)}
    {
      printf("In Client....\n");

      port_ = port;
      std::cout<<"Client port = "<<port_<<std::endl;

      //Socket stucture
      struct sockaddr_in servaddr;

      // socket create and varification
      sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
      if (sockfd_ == -1) {
        printf("socket creation failed...\n");
        exit(0);
      }
      else
        printf("Socket successfully created..\n");
      bzero(&servaddr, sizeof(servaddr));


       //[TODO] AF: Need to make this universal
      // assign IP, PORT
      switch(type)
      {
        case AF_INET:
          servaddr.sin_family = AF_INET;
          inet_pton(AF_INET, ip_address, &(servaddr.sin_addr));
          //servaddr.sin_addr.s_addr = inet_addr(ip_address);
          break;
        default:
          // TODO: error handling
          printf("incorrect socket type\n");
          exit(-1);
      }
      servaddr.sin_port = ntohs(port);

      // connect the client socket to server socket
      if (connect(sockfd_, (SA*)&servaddr, sizeof(servaddr)) != 0) {
        printf("connection with the server failed...\n");
        exit(0);
      }
      else
        printf("connected to the server..\n");

    }


  private:

    void do_send_to_(const void* data, std::size_t data_size) const override
    {

    uint16_t networkLen = htons(data_size); // convert to network byte order
    write(sockfd_, &networkLen, sizeof(networkLen)); //sends the size of the data first
    write(sockfd_, data, data_size); //sends the seralized data

    }


    std::vector<char> do_receive_from_() const override
    {

    //[TODO] AF: this could be more efficent, the read() fucntion seems to only take a array of char and not a vector....
      // char msg[1024];
      // std::cout<<"in receiving "<<std::endl;

      uint16_t networkLen;
      read(sockfd_, &networkLen, sizeof(networkLen));
      // std::cout<<"networkLen "<<networkLen<<std::endl;

      uint16_t len = ntohs(networkLen); // convert back to host byte order
      char msg[len];

      int a = read(sockfd_, msg, len);
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
    int sockfd_;
    int port_;

  }; // class MPICommunicator
} // namespace skynet


#endif /* SKYNET_MPICOMMUNICATOR_HPP__ */
