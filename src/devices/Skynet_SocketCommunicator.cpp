#include "devices/Skynet_SocketCommunicator.hpp"

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
  SocketCommunicator::SocketCommunicator(uint16_t port, int type)
  {
    printf("In Server....\n");
    success_ = false;

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
    std::cout<<"Server port = "<<port<<std::endl;
    servaddr.sin_port = htons(port);
    // Binding newly created socket to given IP and verification
    if ((bind(connfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
      printf("socket bind failed...\n");
      return;
      //exit(0);
    }
    else
    printf("Server Socket successfully binded..\n");
    //listening for a connection
    listen(connfd, 5); //TODO: is this needed? CJV

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

    success_ = true;
  }

  SocketCommunicator::SocketCommunicator(const char * ip_address, uint16_t port, int type)
  {
    printf("In Client....\n");
    success_ = false;

    std::cout<<"Client port = "<<port<<std::endl;

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
      //exit(0);
      return;
    }
    else
      printf("connected to the server..\n");
    success_ = true;

  }

  void SocketCommunicator::do_send_to_(const void* data, std::size_t data_size) const
  {

    uint16_t networkLen = htons(data_size); // convert to network byte order
    write(sockfd_, &networkLen, sizeof(networkLen)); //sends the size of the data first
    write(sockfd_, data, data_size); //sends the seralized data
  }


  std::vector<char> SocketCommunicator::do_receive_from_() const
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
} // namespace skynet
