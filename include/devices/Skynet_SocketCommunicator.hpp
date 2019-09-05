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
     * Creates a connected socket based on the supplied address and port
     *
     * \param address_type Specifies the address type to be used.
     * \param server_address The address to connect to
     * \param port The port to connect to
     */
    SocketCommunicator(
      const int address_type,
      const char* const server_address,
      const std::uint16_t port
    )
      : socket_(address_type)
    {
      socket_.connect_to_server(server_address, port);
    }

    /** \brief Construct a new SocketCommunicator.
     *
     * Creates a communicator based off an already existing connection, taking ownership
     *
     * \param conn An already existing connection to handle
     */
    SocketCommunicator(Socket conn)
      : socket_(std::move(conn))
    {}

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
