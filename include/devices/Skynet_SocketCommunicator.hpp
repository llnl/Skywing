#ifndef SKYNET_SOCKETCOMMUNICATOR_HPP__
#define SKYNET_SOCKETCOMMUNICATOR_HPP__

#include "Skynet_DeviceCommunicator.hpp"
#include "Skynet_Socket.hpp"

#include <memory>

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
    SocketCommunicator(const int address_type, const uint16_t port = 1)
      : socket_(address_type, port)
    {}

    /** \brief Construct a new SocketCommunicator.
     *
     * creates a connected Socket object
     * this communicator acts as a server
     *
     * \param listener The Socket that is listening for new connections
     */
    SocketCommunicator(const Socket& listener)
      : socket_(listener)
    {}

    /** \brief Connect to a server SocketCommunicator.
     *
     * \param server_address The address of the server SocketCommunicator.
     * \param port Which port number to connect to on the server.
     */

    void connect_to_server(const char* const server_address, const uint16_t port)
    { socket_.connect_to_server(server_address, port); }



  private:

    void do_send_to_(const void* data, const std::size_t data_size) override
    {
      const uint16_t networkLen = htons(data_size); // convert to network byte order
      socket_.send_message(&networkLen, sizeof(networkLen)); // sends the size of the data first
      socket_.send_message(data, data_size); // sends the seralized data
    }


    std::vector<char> do_receive_from_() override
    {
      uint16_t networkLen;
      socket_.read_message(&networkLen, sizeof(networkLen));

      const uint16_t len = ntohs(networkLen); // convert back to host byte order
      std::vector<char> data(len);
      socket_.read_message(data.data(), len);
      return data;
    }

    Socket socket_;

  }; // class SocketCommunicator
} // namespace skynet


#endif /* SKYNET_SOCKETCOMMUNICATOR_HPP__ */
