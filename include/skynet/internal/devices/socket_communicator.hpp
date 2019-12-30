#ifndef SKYNET_INTERNAL_DEVICES_SOCKET_COMMUNICATOR_HPP
#define SKYNET_INTERNAL_DEVICES_SOCKET_COMMUNICATOR_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

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
    [[nodiscard]] ConnectionError set_to_listen(std::uint16_t port) noexcept;

    /** \brief Connects to a server
     *
     * \param address The address to connect to
     * \param port The port to connect on
     */
    [[nodiscard]] ConnectionError connect_to_server(const char* address, std::uint16_t port) noexcept;

    /** \brief Connects to a server given an address:port string
     */
    [[nodiscard]] ConnectionError connect_to_server(std::string_view address) noexcept;

    /** \brief Sends a message on the socket
     *
     * \param message The message to send
     * \param size The size of the message
     */
    [[nodiscard]] ConnectionError send_message(const std::byte* message, std::size_t size) noexcept;

    /** \brief Recieve a message from the socket if one is available
     *
     * If there is no message to read (ConnectionError::would_block is returned)
     * then the buffer is left in an unspecified state.
     *
     * \param buffer The buffer to write to
     * \param size The size of the buffer / number of bytes to read
     */
    [[nodiscard]] ConnectionError read_message(std::byte* buffer, std::size_t size) noexcept;

    /** \brief Returns the IP address and port of the socket
     */
    std::pair<std::string, std::uint16_t> ip_address_and_port() const noexcept;

  private:
    // Tag for using the raw handle constructor
    struct WithRawHandle{};

    // Construct a socket using a pre-exising handle
    SocketCommunicator(WithRawHandle, const int handle) noexcept;

    // The handle to the raw socket
    int handle_;
  }; // class SocketCommunicator

  /** \brief Read a message in chunks from a SocketCommunicator.
   */
  std::vector<std::byte> read_chunked(SocketCommunicator& conn, std::size_t num_bytes) noexcept;

  /** \brief Splits a "ip:port" address into its parts
   * The string is empty if the input was invalid
   */
  std::pair<std::uint16_t, std::string> split_address(const std::string_view address) noexcept;

  /** \brief Subscription for recieving data from a publisher
   */
  class Subscription
  {
  public:
    /** \brief Attempt to create a subscription to the specified address
     */
    static std::optional<Subscription> try_to_create(std::string_view address) noexcept;

    /** \brief Recieve a message from the socket if one is available
     *
     * If there is no message to read (ConnectionError::would_block is returned)
     * then the buffer is left in an unspecified state.
     *
     * \param buffer The buffer to write to
     * \param size The size of the buffer / number of bytes to read
     */
    [[nodiscard]] ConnectionError read_message(std::byte* buffer, std::size_t size) noexcept;

    /** \brief Read a message in chunks
     */
    std::vector<std::byte> read_chunked(const std::size_t num_bytes) noexcept;

    /** \brief Returns the IP address and port of the socket
     */
    std::pair<std::string, std::uint16_t> ip_address_and_port() const noexcept;

  private:
    // Don't allow external construction
    explicit Subscription() = default;

    SocketCommunicator conn_;
  }; // class Subscription

  /** \brief Publication channel
   */
  class PublicationChannel
  {
  public:
    /** \brief Create a publication channel on the specified port
     */
    PublicationChannel(std::uint16_t port) noexcept;

    /** \brief Accepts any pending subscriptions
     */
    void accept_subscriptions() noexcept;

    /** \brief Sends a message on the socket
     *
     * \param message The message to send
     * \param size The size of the message
     */
    void send_message(const std::byte* message, std::size_t size) noexcept;

    /** \brief Returns the number of subscriptions that are present
     */
    int num_subscriptions() const noexcept;

  private:
    SocketCommunicator conn_;

    std::vector<SocketCommunicator> subscriptions_;
  };
} // namespace skynet::internal

#endif // SKYNET_INTERNAL_DEVICES_SOCKET_COMMUNICATOR_HPP
