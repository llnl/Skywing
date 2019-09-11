#ifndef SKYNET_DEVICES_SOCKET_COMMUNICATOR_HPP
#define SKYNET_DEVICES_SOCKET_COMMUNICATOR_HPP

#include "skynet/utility/optional.hpp"

#include <cstdint>

namespace skynet
{
  /** \brief Socket based communicator
   */
  class SocketCommunicator
  {
  public:
    /** \brief Create a new socket-based communicator
     */
    SocketCommunicator() noexcept;

    // Can not be copied
    SocketCommunicator(const SocketCommunicator&) = delete;
    SocketCommunicator& operator=(const SocketCommunicator&) = delete;

    // Can be moved
    SocketCommunicator(SocketCommunicator&&) noexcept;
    SocketCommunicator& operator=(SocketCommunicator&&) noexcept;

    // Destructor
    ~SocketCommunicator();

    /** \brief Accepts an incoming connection if one is pending
     */
    Optional<SocketCommunicator> accept() noexcept;

    /** \brief Listens for requests on the specified port
     *
     * \param port The port to listen for connections on
     */
    void set_to_listen(std::uint16_t port) noexcept;

    /** \brief Connects to a server
     *
     * \param address The address to connect to
     * \param port The port to connect on
     * \return True if connecting was successful, false otherwise
     */
    bool connect_to_server(const char* address, std::uint16_t port) noexcept;

    /** \brief Sends a message on the socket
     *
     * \param message The message to send
     * \param size The size of the message
     */
    void send_message(const char* message, std::size_t size) noexcept;

    /** \brief Recieve a message from the socket if one is available
     *
     * \param buffer The buffer to write to
     * \param size The size of the buffer / number of bytes to read
     * \return True if a message was read, false otherwise.
     *         The buffer is left in an unspecified state if false is returned.
     */
    bool read_message(char* buffer, std::size_t size) noexcept;

  private:
    // Tag for using the raw handle constructor
    struct WithRawHandle{};

    // Construct a socket using a pre-exising handle
    SocketCommunicator(WithRawHandle, const int handle) noexcept
      : handle_(handle)
    {}

    // The handle to the raw socket
    int handle_;
  }; // class SocketCommunicator
} // namespace skynet

#endif // SKYNET_DEVICES_SOCKET_COMMUNICATOR_HPP
