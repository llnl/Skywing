#include "skywing_core/internal/devices/socket_communicator.hpp"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <iostream>

#include "generated/socket_no_sigpipe.hpp"
#include "skywing_core/internal/utility/logging.hpp"
#include "skywing_core/internal/utility/network_conv.hpp"
#include "socket_wrappers.hpp"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>

// NOTE (trb): I like abstract sockets. Especially on clusters that
// might have a variety of filesystems mounted, this saves some
// headache if the filesystem is slow or otherwise disgruntled. While
// the Linux implementation allows a mixture of "abstract" and
// "pathname" sockets to coexist peacefully, Skywing doesn't care --
// Linux will only use "abstract" sockets and non-Linux will use
// "pathname" sockets.
#ifdef __linux__
#define SKYWING_ABSTRACT_SOCKETS 1
#define SKYWING_LINUX_NOEXCEPT noexcept
#else
#define SKYWING_ABSTRACT_SOCKETS 0
#define SKYWING_LINUX_NOEXCEPT
#endif

namespace
{
constexpr int invalid_handle = -1;

struct addrinfo_deleter
{
    void operator()(addrinfo* info) const noexcept { freeaddrinfo(info); }
};

using addrinfo_ptr = std::unique_ptr<addrinfo, addrinfo_deleter>;

addrinfo_ptr resolve_ipv4_addr(skywing::SocketAddr const& addr) noexcept
{
    addrinfo* result;
    addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_IP;
    const auto port_str = std::to_string(addr.port());
    const auto resaddr =
        getaddrinfo(addr.address().c_str(), port_str.c_str(), &hints, &result);
    if (resaddr != 0) {
        std::cerr << "resolve_ipv4_addr - getaddrinfo failed (address=\""
                  << addr.address() << "\", port=" << addr.port()
                  << "): " << gai_strerror(resaddr) << std::endl;
        std::terminate();
    }
    return {result, {}};
}

int init_ipv4_connection(const int sockfd,
                         skywing::SocketAddr const& addr) noexcept
{
    // TODO: What is the correct address-acquisition approach?

    // This isn't super robust, but I'm not sure how to handle looking up a
    // bunch of different address in an asynchronous context
    // const auto result = resolve_addr(address, port);
    // return connect(sockfd, result->ai_addr,
    // static_cast<int>(result->ai_addrlen));
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(addr.port());
    if (inet_pton(AF_INET, addr.address().c_str(), &serv_addr.sin_addr) <= 0) {
        SKYWING_ERROR_LOG("Invalid IPv4 address \"{}\"", addr);
        std::terminate();
    }
    return connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
}
} // namespace

namespace skywing::internal
{

// NOTE (trb): This interface extracts essentially the core
// capabilities we need from the socket API. Concrete implementations
// will be provided for "SOCK_STREAM" type in the IPv4 (AF_INET) and
// Unix/Local (AF_LOCAL) families. One will notice that I've left
// essentially all of the interface abstract in the base class, even
// though the concrete implementations share considerable similarity.
// The fact is, though, that some element of virtuality is needed in
// each of the functions (generally the specification of the concrete
// "sockaddr" type), and it doesn't much matter where the virtuality
// hits. IMO, the fully self-contained concrete implementations were
// easier to ready and rather simple to implement. It also led to
// greater consistency across the few instances were there is some
// difference more subtle than just a sockaddr type.
//
// It is also possible to do this without any sort of virtuality with
// explicit branching based on
// `SocketAddr::is_unix()`/`SocketAddr::is_ipv4()`, using
// `sockaddr_storage` as the common address type that could be passed
// around when needed. However, none of the functions implemented here
// are actually performance-critical -- may as well let C++'s virtual
// functions try to be useful. It also seems that, if we add some
// other sort of socket impl (IPv6, e.g.), the virtual inteface would
// be easier to extend.
class SocketImplBase
{
protected:
    int m_handle = -1;

public:
    SocketImplBase(int address_family);
    virtual ~SocketImplBase();

    int get() const noexcept { return m_handle; }

    virtual void set_to_listen(SocketAddr const&) = 0;
    virtual std::unique_ptr<SocketImplBase> accept() const noexcept = 0;

    virtual ConnectionError connect_blocking(SocketAddr const& addr) = 0;
    virtual ConnectionError connect_non_blocking(SocketAddr const& addr) = 0;

    virtual ConnectionError connection_progress_status() noexcept = 0;

    virtual SocketAddr ip_addr_and_port() const noexcept = 0;
    virtual SocketAddr host_ip_addr_and_port() const noexcept = 0;
};

class IPv4Socket final : public SocketImplBase
{
public:
    IPv4Socket() noexcept : SocketImplBase{AF_INET} {};
    ~IPv4Socket() final = default;

    void set_to_listen(SocketAddr const&) final;
    std::unique_ptr<SocketImplBase> accept() const noexcept final;

    ConnectionError connect_blocking(SocketAddr const& addr) final;
    ConnectionError connect_non_blocking(SocketAddr const& addr) final;

    ConnectionError connection_progress_status() noexcept final;

    SocketAddr ip_addr_and_port() const noexcept final;
    SocketAddr host_ip_addr_and_port() const noexcept final;
}; // class IPv4Socket

class UnixSocket final : public SocketImplBase
{
public:
    UnixSocket() noexcept : SocketImplBase{AF_LOCAL} {}

    ~UnixSocket() SKYWING_LINUX_NOEXCEPT final;

    void set_to_listen(SocketAddr const&) final;
    std::unique_ptr<SocketImplBase> accept() const noexcept final;

    ConnectionError connect_blocking(SocketAddr const& addr) final;
    ConnectionError connect_non_blocking(SocketAddr const& addr) final;

    ConnectionError connection_progress_status() noexcept final;

    SocketAddr ip_addr_and_port() const noexcept final;
    SocketAddr host_ip_addr_and_port() const noexcept final;
}; // class UnixSocket

SocketImplBase::SocketImplBase(int address_family)
    : m_handle{create_non_blocking(address_family)}
{}

SocketImplBase::~SocketImplBase()
{
    if (m_handle != invalid_handle) {
        shutdown(m_handle, SHUT_RDWR);
        close(m_handle);
        m_handle = invalid_handle;
    }
}

void IPv4Socket::set_to_listen(SocketAddr const& addr)
{
    constexpr int listen_queue_size = 10;

    sockaddr_in servaddr{};
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(addr.port());

    int optval = 1;
    if (setsockopt(m_handle, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))
        < 0)
    {
        SKYWING_DEBUG_LOG("setsockopt(SO_REUSEADDR) failed: {}",
                          strerror(errno));
        throw std::runtime_error("IPv4Socket - setsockopt failed.");
    }

    if (bind(m_handle, reinterpret_cast<sockaddr*>(&servaddr), sizeof(servaddr))
        < 0)
    {
        std::perror("IPv4Socket::set_to_listen - bind");
        throw std::runtime_error("IPv4Socket - bind failed.");
    }
    if (listen(m_handle, listen_queue_size) < 0) {
        std::perror("IPv4Socket::set_to_listen - listen");
        throw std::runtime_error("IPv4Socket - listen failed.");
    }
}

std::unique_ptr<SocketImplBase> IPv4Socket::accept() const noexcept
{
    sockaddr_in client_address_struct;
    socklen_t len = sizeof(client_address_struct);

    const int raw_handle = accept_make_non_blocking(
        m_handle, reinterpret_cast<sockaddr*>(&client_address_struct), &len);
    if (raw_handle == invalid_handle) {
        // No connection to be made
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return nullptr;
        }
        // This should never happen and is a programming bug if it's reached
        // Not 100% sure how to handle it, but forcefully quitting with a
        // message seems to be fine for now
        SKYWING_DEBUG_LOG(
            "accept had m_handle {}, raw_handle {}, threw error: {}",
            m_handle,
            raw_handle,
            strerror(errno));
        std::perror("IPv4Socket::accept - accept");
        std::terminate();
    }

    auto out = std::make_unique<IPv4Socket>();
    out->m_handle = raw_handle;
    return out;
}

ConnectionError IPv4Socket::connect_blocking(SocketAddr const& addr)
{
    // FIXME (trb): Can we just throw for errors? What happens with
    // ConnectionError::unrecoverable?
    auto const connect_status = init_ipv4_connection(m_handle, addr);
    if (connect_status == -1) {
        if (errno == EINPROGRESS) {
            // wait for the connection to finish
            pollfd to_poll;
            to_poll.fd = m_handle;
            to_poll.events = POLLOUT;
            if (poll(&to_poll, 1, -1) < 0) {
                return ConnectionError::unrecoverable;
            }
            // Check if any error occured
            constexpr auto err_mask = POLLERR | POLLHUP | POLLNVAL;
            if ((to_poll.revents & err_mask) != 0) {
                return ConnectionError::unrecoverable;
            }
        }
        else {
            return ConnectionError::unrecoverable;
        }
    }
    return ConnectionError::no_error;
}

ConnectionError IPv4Socket::connect_non_blocking(SocketAddr const& addr)
{
    auto const connect_status = init_ipv4_connection(m_handle, addr);

    if (connect_status == 0) {
        return ConnectionError::no_error;
    }
    if (connect_status == -1 && errno == EINPROGRESS) {
        return ConnectionError::connection_in_progress;
    }
    return ConnectionError::unrecoverable;
}

ConnectionError IPv4Socket::connection_progress_status() noexcept
{
    pollfd to_poll;
    to_poll.fd = m_handle;
    to_poll.events = POLLOUT | POLLIN;
    if (poll(&to_poll, 1, 0) < 0) {
        // std::perror("SOCKET POLL ERROR: ");
        // This is also required?
        if (errno == EINPROGRESS || errno == EAGAIN) {
            return ConnectionError::connection_in_progress;
        }
        return ConnectionError::unrecoverable;
    }
    constexpr auto err_mask = POLLERR | POLLHUP | POLLNVAL;
    if ((to_poll.revents & err_mask) != 0) {
        // std::printf("SOCKET ERR FLAGS %i - COMP %i %i %i\n", to_poll.revents,
        // POLLERR, POLLHUP, POLLNVAL);
        return ConnectionError::unrecoverable;
    }
    if ((to_poll.revents & (POLLOUT | POLLIN)) != 0) {
        return ConnectionError::no_error;
    }
    return ConnectionError::connection_in_progress;
}

SocketAddr IPv4Socket::ip_addr_and_port() const noexcept
{
    sockaddr_in client_address;
    socklen_t len = sizeof(client_address);
    int err = getpeername(m_handle, (struct sockaddr*) &client_address, &len);
    if (err != 0)
        SKYWING_DEBUG_LOG("IPv4Socket: ip_address_and_port threw error: {}",
                          strerror(errno));

    return {inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port)};
}

SocketAddr IPv4Socket::host_ip_addr_and_port() const noexcept
{
    sockaddr_in host_address;
    socklen_t len = sizeof(host_address);
    auto err = getsockname(m_handle, (struct sockaddr*) &host_address, &len);
    if (err != 0) {
        SKYWING_DEBUG_LOG(
            "IPv4Socket: host_ip_address_and_port threw error: {}",
            strerror(errno));
    }
    return {inet_ntoa(host_address.sin_addr), ntohs(host_address.sin_port)};
}

// UnixSocket impl

UnixSocket::~UnixSocket() SKYWING_LINUX_NOEXCEPT
{
#if !SKYWING_ABSTRACT_SOCKETS
    auto const addr = host_ip_addr_and_port();
    if (!addr.address().empty())
        ::unlink(addr.address().c_str());
#endif
}

void UnixSocket::set_to_listen(SocketAddr const& addr)
{
    constexpr int listen_queue_size = 10;

    sockaddr_un servaddr;
    std::memset(&servaddr, 0, sizeof(sockaddr_un));
    servaddr.sun_family = AF_LOCAL;
#if SKYWING_ABSTRACT_SOCKETS
    std::strncpy(&servaddr.sun_path[1],
                 addr.address().c_str(),
                 std::size(servaddr.sun_path) - 2);
    // NOTE (trb): _technically_ the final null terminator doesn't
    // matter. However, it simplifies things to have it (e.g., just
    // casting the "char*" into a std::string). Also, Skywing is not
    // supporting mid-socket-name null characters, since these
    // wouldn't be valid for "pathname" sockets.
#else
    std::strncpy(servaddr.sun_path,
                 addr.address().c_str(),
                 std::size(servaddr.sun_path) - 1);
#endif

    int optval = 1;
    if (setsockopt(m_handle, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval))
        < 0)
    {
        SKYWING_DEBUG_LOG("setsockopt(SO_REUSEADDR) failed: {}",
                          strerror(errno));
        throw std::runtime_error("UnixSocket - setsockopt failed.");
    }

    if (bind(m_handle, (sockaddr*) &servaddr, sizeof(servaddr)) < 0) {
        std::perror("UnixSocket::set_to_listen - bind");
        throw std::runtime_error("UnixSocket - bind failed.");
    }

    if (listen(m_handle, listen_queue_size) < 0) {
        std::perror("UnixSocket::set_to_listen - listen");
        throw std::runtime_error("UnixSocket - listen failed.");
    }
}

std::unique_ptr<SocketImplBase> UnixSocket::accept() const noexcept
{
    sockaddr_un client_address_struct;
    socklen_t len = sizeof(client_address_struct);

    const int raw_handle = accept_make_non_blocking(
        m_handle, (sockaddr*) &client_address_struct, &len);
    if (raw_handle == invalid_handle) {
        // No connection to be made
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return nullptr;
        }
        // This should never happen and is a programming bug if it's reached
        // Not 100% sure how to handle it, but forcefully quitting with a
        // message seems to be fine for now
        SKYWING_DEBUG_LOG(
            "accept had m_handle {}, raw_handle {}, threw error: {}",
            m_handle,
            raw_handle,
            strerror(errno));
        std::perror("UnixSocket::accept - accept");
        std::terminate();
    }

    auto out = std::make_unique<UnixSocket>();
    out->m_handle = raw_handle;
    return out;
}

ConnectionError UnixSocket::connect_blocking(SocketAddr const& addr)
{
    sockaddr_un servaddr;
    std::memset(&servaddr, 0, sizeof(sockaddr_un));
    servaddr.sun_family = AF_LOCAL;
#if SKYWING_ABSTRACT_SOCKETS
    std::strncpy(&servaddr.sun_path[1],
                 addr.address().c_str(),
                 std::size(servaddr.sun_path) - 2);
#else
    std::strncpy(servaddr.sun_path,
                 addr.address().c_str(),
                 std::size(servaddr.sun_path) - 1);
#endif

    while (::connect(m_handle, (sockaddr const*) &servaddr, sizeof(servaddr))
           == -1)
    {
        if (errno == ENOENT || errno == ECONNREFUSED)
            continue;
        return ConnectionError::unrecoverable;
    }
    return ConnectionError::no_error;
}

ConnectionError UnixSocket::connect_non_blocking(SocketAddr const& addr)
{
    // NOTE (trb): This isn't super easy to emulate, so just do a
    // blocking connect for now. There's also less overhead on the
    // backend here, so this shouldn't block for long anyway.
    return this->connect_blocking(addr);
}

ConnectionError UnixSocket::connection_progress_status() noexcept
{
    // NOTE (trb): Because all connections are blocking, this is
    // always "no_error". If the above function changes, this will
    // need to be updated.
    return ConnectionError::no_error;
}

SocketAddr UnixSocket::ip_addr_and_port() const noexcept
{
    sockaddr_un client_address;
    socklen_t len = sizeof(client_address);
    int err = getpeername(m_handle, (struct sockaddr*) &client_address, &len);
    if (err != 0)
        SKYWING_DEBUG_LOG("UnixSocket: ip_address_and_port threw error: {}",
                          strerror(errno));

#if SKYWING_ABSTRACT_SOCKETS
    return {&client_address.sun_path[1], 0};
#else
    return {client_address.sun_path, 0};
#endif
}

SocketAddr UnixSocket::host_ip_addr_and_port() const noexcept
{
    sockaddr_un host_address;
    socklen_t len = sizeof(host_address);
    getsockname(m_handle, (struct sockaddr*) &host_address, &len);
#if SKYWING_ABSTRACT_SOCKETS
    return {&host_address.sun_path[1], 0};
#else
    return {host_address.sun_path, 0};
#endif
}

// SocketCommunicator

SocketCommunicator::SocketCommunicator() noexcept = default;
SocketCommunicator::SocketCommunicator(SocketCommunicator&&) noexcept = default;
SocketCommunicator&
SocketCommunicator::operator=(SocketCommunicator&&) noexcept = default;

SocketCommunicator::~SocketCommunicator() noexcept = default;

namespace
{
std::unique_ptr<SocketImplBase> make_suitable_socket(SocketAddr const& addr)
{
    if (addr.is_ipv4())
        return std::make_unique<IPv4Socket>();
    else if (addr.is_unix())
        return std::make_unique<UnixSocket>();
    return nullptr;
}
} // namespace

ConnectionError
SocketCommunicator::connect_to_server(SocketAddr const& addr) noexcept
{
    m_handle = make_suitable_socket(addr);
    return m_handle->connect_blocking(addr);
}

ConnectionError
SocketCommunicator::connect_non_blocking(SocketAddr const& addr) noexcept
{
    m_handle = make_suitable_socket(addr);
    return m_handle->connect_non_blocking(addr);
}

ConnectionError SocketCommunicator::connection_progress_status() noexcept
{
    return m_handle->connection_progress_status();
}

ConnectionError
SocketCommunicator::send_message(const std::byte* const message,
                                 const std::size_t size) noexcept
{
    if (send(m_handle->get(), message, size, SKYWING_NO_SIGPIPE) < 0) {
        SKYWING_DEBUG_LOG("send_message threw error: {}", strerror(errno));
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return ConnectionError::would_block;
        }
        return ConnectionError::unrecoverable;
    }
    return ConnectionError::no_error;
}

ConnectionError
SocketCommunicator::read_message(std::byte* const buffer,
                                 const std::size_t size) noexcept
{
    const auto read_bytes =
        read(m_handle->get(), reinterpret_cast<char*>(buffer), size);
    if (read_bytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return ConnectionError::would_block;
        }

        SKYWING_DEBUG_LOG("read_message threw error: {}", strerror(errno));
        return ConnectionError::unrecoverable;
    }
    return (read_bytes == 0 ? ConnectionError::closed
                            : ConnectionError::no_error);
}

SocketAddr SocketCommunicator::ip_address_and_port() const noexcept
{
    return m_handle->ip_addr_and_port();
}

SocketAddr SocketCommunicator::host_ip_address_and_port() const noexcept
{
    return m_handle->host_ip_addr_and_port();
}

SocketCommunicator::SocketCommunicator(std::unique_ptr<SocketImplBase> socket)
    : m_handle{std::move(socket)}
{}

// SocketListener impl

SocketListener::SocketListener(std::string const& addr)
    : m_handle{std::make_unique<UnixSocket>()}
{
    m_handle->set_to_listen({addr, 0});
}

SocketListener::SocketListener(unsigned short port)
    : m_handle{std::make_unique<IPv4Socket>()}
{
    m_handle->set_to_listen({"", port});
}

SocketListener::SocketListener(SocketAddr const& addr)
{
    if (addr.is_unix())
        m_handle = std::make_unique<UnixSocket>();
    else
        m_handle = std::make_unique<IPv4Socket>();
    m_handle->set_to_listen(addr);
}

SocketListener::~SocketListener()
{}

SocketAddr SocketListener::listening_addr() const
{
    auto out = m_handle->host_ip_addr_and_port();
    if (out.is_ipv4())
        return {"", out.port()};
    return out;
}

std::optional<SocketCommunicator> SocketListener::accept()
{
    auto socket = m_handle->accept();
    if (!socket)
        return std::nullopt;

    return SocketCommunicator{std::move(socket)};
}

std::vector<std::byte> read_chunked(SocketCommunicator& conn,
                                    const std::size_t num_bytes) noexcept
{
    // Size of memory to allocate/read each step
    constexpr std::size_t read_step_size = 0x0'1000;
    constexpr std::size_t allocate_step_size = read_step_size * 16;
    // How often memory needs to be resized
    constexpr std::size_t resize_every_n_steps =
        allocate_step_size / read_step_size;
    // Ensure that the allocate size is evenly divisible by the read size
    static_assert(allocate_step_size % read_step_size == 0);
    static_assert(allocate_step_size >= read_step_size);
    // To prevent overallocation of memory, don't allocate a ton of memory to
    // start
    std::vector<std::byte> read_bytes;
    // The final bytes to read in the end
    const int final_read_size = num_bytes % read_step_size;
    // Read memory in 4KiB chunks
    const int num_iters =
        num_bytes / read_step_size + (final_read_size == 0 ? 0 : 1);
    for (int i = 0; i < num_iters; ++i) {
        if (i % resize_every_n_steps == 0) {
            // Allocate more memory
            const std::size_t mem_left_to_read = num_bytes - read_bytes.size();
            const std::size_t additional_size =
                mem_left_to_read > allocate_step_size ? allocate_step_size
                                                      : mem_left_to_read;
            read_bytes.resize(read_bytes.size() + additional_size);
        }
        const std::size_t num_bytes_to_read =
            (i == num_iters - 1 ? final_read_size : read_step_size);
        // Allocate more memory if needed
        if (conn.read_message(&read_bytes[i * read_step_size],
                              num_bytes_to_read)
            != ConnectionError::no_error)
        {
            return {};
        }
    }
    return read_bytes;
}

SocketAddr split_address(const std::string_view address) noexcept
{
    // Split the address by the colon
    const auto colon_loc = address.find(':');
    if (colon_loc == std::string_view::npos) {
        return {};
    }
    const auto port_str = address.substr(colon_loc + 1);
    // Try to parse the port
    char* end;
    const auto port = strtol(port_str.data(), &end, 10);
    // Check that the entire string was parsed and that the port is valid
    if (end != port_str.data() + port_str.size() || port < 0 || port > 0xFFFF) {
        return {};
    }
    // Try to connect to the publisher
    // Need to make a std::string to ensure that it is null-terminated
    return SocketAddr{std::string{address.begin(), address.begin() + colon_loc},
                      static_cast<uint16_t>(port)};
}

std::variant<NetworkSizeType, ConnectionError>
read_network_size(SocketCommunicator& conn) noexcept
{
    std::array<std::byte, sizeof(NetworkSizeType)> size_buffer;
    const auto err = conn.read_message(size_buffer.data(), size_buffer.size());
    if (err == ConnectionError::no_error) {
        return from_network_bytes(size_buffer);
    }
    return err;
}

std::string to_ip_port(const SocketAddr& addr) noexcept
{
    return to_canonical(addr).str();
}

SocketAddr to_canonical(const SocketAddr& addr) noexcept
{
    if (addr.is_unix())
        return addr;

    const auto result = resolve_ipv4_addr(addr);
    sockaddr_in* info = reinterpret_cast<sockaddr_in*>(result->ai_addr);
    const std::string to_ret =
        std::to_string((info->sin_addr.s_addr & 0x000000FF) >> 0) + '.'
        + std::to_string((info->sin_addr.s_addr & 0x0000FF00) >> 8) + '.'
        + std::to_string((info->sin_addr.s_addr & 0x00FF0000) >> 16) + '.'
        + std::to_string((info->sin_addr.s_addr & 0xFF000000) >> 24);
    return {to_ret, addr.port()};
}
} // namespace skywing::internal
