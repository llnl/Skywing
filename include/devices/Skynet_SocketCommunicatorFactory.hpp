#ifndef SKYNET_SOCKETCOMMUNICATORFACTORY_HPP__
#define SKYNET_SOCKETCOMMUNICATORFACTORY_HPP__

// #include "mpi.h"
#include <memory>
#include "Skynet_CommunicatorFactory.hpp"
#include "Skynet_SocketCommunicator.hpp"

namespace skynet
{
  class SocketCommunicatorFactory : public CommunicatorFactory
  {
  public:
 /** \brief Create a new SocketCommunicatorFactory.
     *
     * The ip_address of the device we wish to communicated is needed.
     */
    // SocketCommunicatorFactory()
    // { 
    //   // intconnfd; 
    //   struct sockaddr_in servaddr, cli; 
  
    //   // socket create and verification 
    //   sockfd_ = socket(AF_INET, SOCK_STREAM, 0); 
    //   if (sockfd_ == -1) { 
    //     printf("socket creation failed...\n"); 
    //     exit(0); 
    //   } 
    //   else
    //     printf("Socket successfully created..\n"); 
    //   bzero(&servaddr, sizeof(servaddr)); 
  
    //   // assign IP, PORT 
    //   servaddr.sin_family = AF_INET; 
    //   servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    //   servaddr.sin_port = htons(PORT); 
  
    //   // Binding newly created socket to given IP and verification 
    //   if ((bind(sockfd_, (SA*)&servaddr, sizeof(servaddr))) != 0) { 
    //     printf("socket bind failed...\n"); 
    //     exit(0); 
    //   } 
    //   else
    //     printf("Socket successfully binded..\n"); 
    //     listen(sockf_, 5);

    //           // Now server is ready to listen and verification 
    //   // if ((listen(sockfd, 5)) != 0) { 
    //   //     printf("Listen failed...\n"); 
    //   //     exit(0); 
    //   // } 
    //   // else
    //   //     printf("Server listening..\n"); 
    //   socklen_t len = sizeof(cli); 
  
    //   // Accept the data packet from client and verification 
    //   connfd_ = accept(sockfd_, (struct sockaddr *) &cli, &len); 
    //   if (connfd_ < 0) { 
    //     printf("server acccept failed...\n"); 
    //     exit(0); 
    //   } 
    //   else
    //     printf("server acccept the client...\n"); 

    // }

    //  /** \brief Create a new SocketCommunicatorFactory.
    //  *
    //  * The ip_address of the device we wish to communicated is needed.
    //  */
    // SocketCommunicatorFactory(std::string d1_ip_add)
    //   : d1_ip_address_(d1_ip_add), 
    // { 

    //   td::cout<<"In Client send_to"<<std::endl; 

    //   struct sockaddr_in servaddr; //, cli; 
  
    //   // socket create and varification 
    //   sockfd_ = socket(AF_INET, SOCK_STREAM, 0); 
    //   if (sockfd_ == -1) { 
    //     printf("socket creation failed...\n"); 
    //     exit(0); 
    //   } 
    //   else
    //   printf("Socket successfully created..\n"); 
    //   bzero(&servaddr, sizeof(servaddr)); 
  
    //   // assign IP, PORT 
    //   servaddr.sin_family = AF_INET; 
    //   servaddr.sin_addr.s_addr = inet_addr(d1_ip_address_); 
    //   servaddr.sin_port = htons(PORT); 
  
    //   // connect the client socket to server socket 
    //   if (connect(sockfd1, (SA*)&servaddr, sizeof(servaddr1)) != 0) { 
    //     printf("connection with the server failed...\n"); 
    //     exit(0); 
    //   } 
    //   else
    //     printf("connected to the server..\n"); 

    // }


    std::unique_ptr<DeviceCommunicator> 
    create_new_communicator(std::vector<std::string> comm_config_info)
    {
      std::string ip_address = "192.0.0.1";
      return SocketCommunicator(ip_address);
    }

  private:
  std::string d1_ip_address_;
  // std::string d2_ip_address_
  // int sockfd_; 
  // int connfd_ = -1 ; 
  }; // SocketCommunicatorFactory
} // namespace skynet

#endif /* SKYNET_SOCKETCOMMUNICATORFACTORY_HPP__ */
