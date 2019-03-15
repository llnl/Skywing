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
     * creates an unconnected Socket object
     * this communicator acts as a client
     *
     * \param address_type Specifies the address type to be used.
     */
    SocketCommunicator(int address_type) : socket_(address_type)
    {}

    /** \brief Construct a new SocketCommunicator.
     *
     * creates a connected Socket object
     * this communicator acts as a server
     *
     * \param listener The Socket that is listening for new connections
     */
    SocketCommunicator(Socket& listener) : socket_(listener)
    {}

    /** \brief Connect to a server SocketCommunicator.
     *
     * \param server_address The address of the server SocketCommunicator.
     * \param port Which port number to connect to on the server.
     */

    void connect_to_server(const char * server_address, uint16_t port)
    { socket_.connect_to_server(server_address, port); }

    void close_connection( )
    { socket_->close_socket(); }

    void bind_to_port(uint16_t port, const char* client_address = NULL){
      socket_->bind_to_port(port, true,client_address);
    }

    void set_to_listen(int queue_length)
    {
      socket_->set_to_listen(queue_length);
    }

    void connect_new_socket_to_client(){
      socket_->connect_new_socket_to_client(); 
    }

    uint16_t get_port(){
      return socket_->get_port();
    }
  private:

    void do_send_to_(const void* data, std::size_t data_size) const override
    {
      uint16_t networkLen = htons(data_size); // convert to network byte order
      socket_.send_message(&networkLen, sizeof(networkLen)); //sends the size of the data first
      socket_.send_message(data, data_size); //sends the seralized data
    }


    std::vector<char> do_receive_from_() const override
    {

    //[TODO] AF: this could be more efficent, the read() fucntion seems to only take a array of char and not a vector....
      // char msg[1024];
      // std::cout<<"in receiving "<<std::endl;

      uint16_t networkLen;
      socket_.read_message(&networkLen, sizeof(networkLen));
      // std::cout<<"networkLen "<<networkLen<<std::endl;

      uint16_t len = ntohs(networkLen); // convert back to host byte order
      char msg[len];

      socket_.read_message(msg, len);
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
