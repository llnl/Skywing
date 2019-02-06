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
     * creates a new Socket object
     *
     * \param address_type Specifies the address type to be used.
     */
    SocketCommunicator(int address_type)
    { socket_ = std::make_unique<Socket>(address_type); }

    /** \brief Construct a new SocketCommunicator.
     *
     * uses existing Socket object
     *
     * \param socket The existing Socket object
     * \param address Specifies the address that socket is connected to
     */
    SocketCommunicator(std::unique_ptr<Socket> socket)
    { socket_ = std::move(socket); }

    /** \brief Connect to a server SocketCommunicator.
     *
     * \param server_address The address of the server SocketCommunicator.
     * \param port Which port number to connect to on the server.
     */
    void connect_to_server(const char * server_address, uint16_t port)
    { socket_->connect_to_server(server_address, port); }

  private:

    void do_send_to_(const void* data, std::size_t data_size) const override
    {
      uint16_t networkLen = htons(data_size); // convert to network byte order
      socket_->send_message(&networkLen, sizeof(networkLen)); //sends the size of the data first
      socket_->send_message(data, data_size); //sends the seralized data
    }


    std::vector<char> do_receive_from_() const override
    {

    //[TODO] AF: this could be more efficent, the read() fucntion seems to only take a array of char and not a vector....
      // char msg[1024];
      // std::cout<<"in receiving "<<std::endl;

      uint16_t networkLen;
      socket_->read_message(&networkLen, sizeof(networkLen));
      // std::cout<<"networkLen "<<networkLen<<std::endl;

      uint16_t len = ntohs(networkLen); // convert back to host byte order
      char msg[len];

      socket_->read_message(msg, len);
      msg[len] = '\0';

      //Hack way to get it into the correct format for the serialzier.
      std::vector<char> data(len);
      for(int i = 0; i<len; i++){
        data[i] = msg[i];
        // std::cout<<"data[i] ="<<data[i]<<std::endl;
      }
      return data;
    }

    std::unique_ptr<Socket> socket_;

  }; // class SocketCommunicator
} // namespace skynet


#endif /* SKYNET_SOCKETCOMMUNICATOR_HPP__ */
