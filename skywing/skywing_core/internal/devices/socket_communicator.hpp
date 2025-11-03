#ifndef SKYWING_INTERNAL_DEVICES_SOCKET_COMMUNICATOR_HPP
#define SKYWING_INTERNAL_DEVICES_SOCKET_COMMUNICATOR_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "skywing_core/types.hpp"

namespace skywing::internal
{
/** \brief Enum returned from communication functions for connection status
 */
enum class [[nodiscard]] ConnectionError
{ /// The call has fully succeeded, no more work needs to be done
    no_error,

    /// The call would block
    would_block,

    /// Non-blocking connected has been initiated
    connection_in_progress = would_block,

    /// An error occurred with communication that has left the connection
    /// in an unusable state
    unrecoverable,

    /// The connection has closed
    closed
}; // enum class ConnectionError

// Forward declaration
class SocketImplBase;

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
    ~SocketCommunicator() noexcept;

    /** \brief Connects to a server
     *
     * \param address The address to connect to
     * \param port The port to connect on
     */
    ConnectionError connect_to_server(SocketAddr const& address) noexcept;

    /** \brief Initiates a non-blocking connection to a server
     *
     *  \warning Unix sockets will still block. This should be very
     *  low overhead, though, as we're just waiting for the
     *  filesystem/kernel to catch up. There is a risk of deadlock,
     *  however, if a neighbor agent fails to appear (e.g., if it's
     *  spelled wrong or if it dies prematurely or the like). As a
     *  corollary, there could be an issue if a single process tries
     *  to manage multiple agents (please don't do this) and the
     *  Managers are not setup properly.
     */
    ConnectionError connect_non_blocking(SocketAddr const& address) noexcept;

    /** \brief Returns status on a pending connection
     *
     * \pre A connection has been initiated
     */
    ConnectionError connection_progress_status() noexcept;

    /** \brief Sends a message on the socket
     *
     * \param message The message to send
     * \param size The size of the message
     */
    ConnectionError send_message(const std::byte* message,
                                 std::size_t size) noexcept;

    /** \brief Recieve a message from the socket if one is available
     *
     * If there is no message to read (ConnectionError::would_block is returned)
     * then the buffer is left in an unspecified state.
     *
     * \param buffer The buffer to write to
     * \param size The size of the buffer / number of bytes to read
     */
    ConnectionError read_message(std::byte* buffer, std::size_t size) noexcept;

    /** \brief Returns the IP address and port of the socket's peer
     */
    SocketAddr ip_address_and_port() const noexcept;

    /** \brief Returns the IP address and port of the host end of the socket
     */
    SocketAddr host_ip_address_and_port() const noexcept;
    // FIXME: This doesn't actually matter! It's just used for logging in
    // manager.cpp.

private:
    friend class SocketListener;

    SocketCommunicator(std::unique_ptr<SocketImplBase> socket);
    std::unique_ptr<SocketImplBase> m_handle;
}; // class SocketCommunicator

/** @brief Just a little class to sit and listen for incoming connections. */
class SocketListener
{
    std::unique_ptr<SocketImplBase> m_handle;

public:
    SocketListener(SocketAddr const& local_addr);
    SocketListener(std::string const& local_addr);
    SocketListener(unsigned short port);
    SocketListener(SocketListener const&) = delete;
    SocketListener& operator=(SocketListener const&) = delete;
    SocketListener(SocketListener&& other) noexcept = default;
    SocketListener& operator=(SocketListener&& other) noexcept = default;
    ~SocketListener();

    SocketAddr listening_addr() const;
    std::optional<SocketCommunicator> accept();
}; // class SocketListener

/** \brief Read a message in chunks from a SocketCommunicator.
 */
std::vector<std::byte> read_chunked(SocketCommunicator& conn,
                                    std::size_t num_bytes) noexcept;

/** \brief Splits an "ip:port" address into its parts
 * The string is empty if the input was invalid
 */
SocketAddr split_address(const std::string_view address) noexcept;

/** \brief Attempts to read a network size from a connection
 *
 * Returns either the network size or the error that occurred
 */
std::variant<NetworkSizeType, ConnectionError>
read_network_size(SocketCommunicator& conn) noexcept;

/** \brief Returns an "IP:Port" string from a given address
 */
std::string to_ip_port(const SocketAddr& addr) noexcept;

/** \brief Converts an SocketAddr to the canonical representation
 */
SocketAddr to_canonical(const SocketAddr& addr) noexcept;
} // namespace skywing::internal

#endif // SKYWING_INTERNAL_DEVICES_SOCKET_COMMUNICATOR_HPP
