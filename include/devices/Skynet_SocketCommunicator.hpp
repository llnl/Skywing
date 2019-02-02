#ifndef SKYNET_SOCKETCOMMUNICATOR_HPP__
#define SKYNET_SOCKETCOMMUNICATOR_HPP__

#include "Skynet_DeviceCommunicator.hpp"
#include "Skynet_Socket.hpp"

namespace skynet
{
  class SocketCommunicator : public DeviceCommunicator
  {
  public:

    /** \brief Construct a new SocketCommunicator.
     *
     * \param type Specifies the address type to be used.
     */
    SocketCommunicator(int type) : socket_(type)
    {}

    /** \brief Construct a new SocketCommunicator.
     *
     * \param address_type Specifies the address type to be used.
     * \param socket_handle Specifies socket handle to existing connected socket
     * \param address Specifies the address that socket is connected to
     */
    SocketCommunicator(int address_type, int socket_handle, const char * address) :
      socket_(address_type, socket_handle, address)
    {}

    /** \brief Connect to a server SocketCommunicator.
     *
     * \param server_address The address of the server SocketCommunicator.
     * \param port Which port number to connect to on the server.
     */
    void connect_to_server(const char * server_address, uint16_t port)
    {
      //Socket stucture
      struct sockaddr_in servaddr;
      bzero(&servaddr, sizeof(servaddr));

      switch(socket_.get_address_type())
      {
        case Socket::IPv4:
          servaddr.sin_family = Socket::IPv4;
          inet_pton(Socket::IPv4, server_address, &(servaddr.sin_addr));
          break;
        default:
          // This should not be reachable because of a check in Socket constructor
          printf("Error in SocketCommunicator.hpp:41\n");
          exit(-1);
      }
      servaddr.sin_port = ntohs(port);

      // connect the client socket to server socket
      if (connect(socket_.get_handle(), (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0)
      {
        perror("connect");
        exit(-1);
      }
    }

  private:

    void do_send_to_(const void* data, std::size_t data_size) const override
    {
      uint16_t networkLen = htons(data_size); // convert to network byte order
      write(socket_.get_handle(), &networkLen, sizeof(networkLen)); //sends the size of the data first
      write(socket_.get_handle(), data, data_size); //sends the seralized data
    }


    std::vector<char> do_receive_from_() const override
    {

    //[TODO] AF: this could be more efficent, the read() fucntion seems to only take a array of char and not a vector....
      // char msg[1024];
      // std::cout<<"in receiving "<<std::endl;

      uint16_t networkLen;
      read(socket_.get_handle(), &networkLen, sizeof(networkLen));
      // std::cout<<"networkLen "<<networkLen<<std::endl;

      uint16_t len = ntohs(networkLen); // convert back to host byte order
      char msg[len];

      int a = read(socket_.get_handle(), msg, len);
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

    Socket socket_;

  }; // class SocketCommunicator
} // namespace skynet


#endif /* SKYNET_SOCKETCOMMUNICATOR_HPP__ */
