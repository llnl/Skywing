#ifndef SKYNET_INTERNAL_DEVICES_SOCKET_COMMUNICATOR_HPP
#define SKYNET_INTERNAL_DEVICES_SOCKET_COMMUNICATOR_HPP

#include <cstddef>
#include <cstdint>
#include <optional>

namespace skynet::internal
{
  /** \brief Enum returned from communication functions for connection status
   */
  enum class ConnectionError
  {
    /// The call has fully succeeded, no more work needs to be done
    no_error,

    /// The call would block
    would_block,

    /// An error occurred with communication that has left the connection
    /// in an unusable state
    unrecoverable,

    /// The connection has closed
    closed
  }; // enum class ConnectionError

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
    std::optional<SocketCommunicator> accept() noexcept;

    /** \brief Listens for requests on the specified port
     *
     * \param port The port to listen for connections on
     */
    ConnectionError set_to_listen(std::uint16_t port) noexcept;

    /** \brief Connects to a server
     *
     * \param address The address to connect to
     * \param port The port to connect on
     */
    ConnectionError connect_to_server(const char* address, std::uint16_t port) noexcept;

    /** \brief Sends a message on the socket
     *
     * \param message The message to send
     * \param size The size of the message
     */
    ConnectionError send_message(const std::byte* message, std::size_t size) noexcept;

    /** \brief Recieve a message from the socket if one is available
     *
     * If there is no message to read (ConnectionError::would_block is returned)
     * then the buffer is left in an unspecified state.
     *
     * \param buffer The buffer to write to
     * \param size The size of the buffer / number of bytes to read
     */
    ConnectionError read_message(std::byte* buffer, std::size_t size) noexcept;

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
} // namespace skynet::internal

#endif // SKYNET_INTERNAL_DEVICES_SOCKET_COMMUNICATOR_HPP
