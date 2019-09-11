#ifndef SKYNET_SOCKETCOMMUNICATOR_HPP__
#define SKYNET_SOCKETCOMMUNICATOR_HPP__

#include "Skynet_DeviceCommunicator.hpp"
#include "Skynet_Socket.hpp"

#include <memory>
#include <thread>
#include <iostream>

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
      socket_.wait_to_connect();
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
    bool do_send(const void* data, const std::size_t data_size) override
    {
      // convert to network byte order
      const uint16_t networkLen = htons(data_size);
      // sends the size of the data first
      if (!socket_.send_message(&networkLen, sizeof(networkLen)))
      {
        return false;
      }
      // sends the seralized data
      return socket_.send_message(data, data_size);
    }


    std::vector<char> do_receive() override
    {
      uint16_t networkLen;
      if (!socket_.read_message(&networkLen, sizeof(networkLen)))
      {
        return {};
      }

      // convert back to host byte order
      const uint16_t len = ntohs(networkLen);
      std::vector<char> data(len);
      // Need to block until a message is recieved
      while (!socket_.read_message(data.data(), len))
      {
        std::this_thread::sleep_for(std::chrono::microseconds(10));
      }
      return data;
    }

    Socket socket_;
  }; // class SocketCommunicator
} // namespace skynet


#endif /* SKYNET_SOCKETCOMMUNICATOR_HPP__ */
