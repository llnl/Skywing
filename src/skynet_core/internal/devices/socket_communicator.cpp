#include "skynet_core/internal/devices/socket_communicator.hpp"

#include "socket_wrappers.hpp"

#include "skynet_core/internal/utility/logging.hpp"
#include "generated/socket_no_sigpipe.hpp"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>

namespace
{
  constexpr int invalid_handle = -1;
} // namespace {anonymous}

namespace skynet::internal
{
  SocketCommunicator::SocketCommunicator() noexcept
    : handle_{create_non_blocking()}
  {
    if (handle_ == invalid_handle)
    {
      std::perror("SocketCommunicator::SocketCommunicator - socket");
      std::exit(-1);
    }
  }

  SocketCommunicator::SocketCommunicator(SocketCommunicator&& other) noexcept
    : handle_{other.handle_}
  {
    other.handle_ = invalid_handle;
  }

  SocketCommunicator& SocketCommunicator::operator=(SocketCommunicator&& other) noexcept
  {
    // Do this in a roundabout way to handle self-assignment
    const auto new_handle = other.handle_;
    other.handle_ = invalid_handle;
    handle_ = new_handle;
    return *this;
  }

  SocketCommunicator::~SocketCommunicator()
  {
    if (handle_ != invalid_handle)
    {
      close(handle_);
    }
  }

  std::optional<SocketCommunicator> SocketCommunicator::accept() noexcept
  {
    sockaddr_in client_address_struct;
    // len can't be const as accept takes a non-const pointer
    socklen_t len = sizeof(client_address_struct);

    const int raw_handle = accept_make_non_blocking(handle_, reinterpret_cast<sockaddr*>(&client_address_struct), &len);
    if (raw_handle == invalid_handle)
    {
      // No connection to be made
      if (errno == EAGAIN || errno == EWOULDBLOCK)
      {
        return {};
      }
      // This should never happen and is a programming bug if it's reached
      // Not 100% sure how to handle it, but forcefully quitting with a message
      // seems to be fine for now
      std::perror("SocketCommunicator::accept - accept");
      std::exit(-1);
    }

    // Read the address
    return SocketCommunicator(
      WithRawHandle{},
      raw_handle
    );
  }

  ConnectionError SocketCommunicator::set_to_listen(const std::uint16_t port) noexcept
  {
    constexpr int listen_queue_size = 10;
    sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);
    if (bind(handle_, reinterpret_cast<sockaddr*>(&servaddr), sizeof(servaddr)) < 0)
    {
      // std::perror("SocketCommunicator::set_to_listen - bind");
      // std::exit(-1);
      return ConnectionError::unrecoverable;
    }
    if (listen(handle_, listen_queue_size) < 0)
    {
      // std::perror("SocketCommunicator::set_to_listen - listen");
      // std::exit(-1);
      return ConnectionError::unrecoverable;
    }
    return ConnectionError::no_error;
  }

  ConnectionError SocketCommunicator::connect_to_server(const char* const address, const std::uint16_t port) noexcept
  {
    sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, address, &servaddr.sin_addr);
    servaddr.sin_port = ntohs(port);
    if (connect(handle_, reinterpret_cast<sockaddr*>(&servaddr), sizeof(servaddr)) == -1)
    {
      if (errno == EINPROGRESS)
      {
        // wait for the connection to finish
        pollfd to_poll;
        to_poll.fd = handle_;
        to_poll.events = POLLOUT;
        if (poll(&to_poll, 1, -1) < 0)
        {
          // perror("SocketCommunicator::connect_to_server - poll");
          // exit(-1);
          return ConnectionError::unrecoverable;
        }
        // Check if any error occured
        constexpr auto err_mask = POLLERR | POLLHUP | POLLNVAL;
        if ((to_poll.revents & err_mask) != 0)
        {
          return ConnectionError::unrecoverable;
        }
      }
      else
      {
        // std::perror("SocketCommunicator::connect_to_server - connect");
        // std::exit(-1);
        return ConnectionError::unrecoverable;
      }
    }
    return ConnectionError::no_error;
  }

  ConnectionError SocketCommunicator::connect_to_server(const std::string_view address) noexcept
  {
    const auto [port, address_str] = split_address(address);
    if (address_str.empty())
    {
      return ConnectionError::unrecoverable;
    }
    return connect_to_server(address_str.c_str(), port);
  }

  ConnectionError SocketCommunicator::send_message(const std::byte* const message, const std::size_t size) noexcept
  {
    if (send(handle_, message, size, SKYNET_NO_SIGPIPE) < 0)
    {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
      {
        return ConnectionError::would_block;
      }
      // std::perror("SocketCommunicator::send_message - write");
      // std::exit(-1);
      return ConnectionError::unrecoverable;
    }
    return ConnectionError::no_error;
  }

  ConnectionError SocketCommunicator::read_message(std::byte* const buffer, const std::size_t size) noexcept
  {
    const auto read_bytes = read(handle_, reinterpret_cast<char*>(buffer), size);
    if (read_bytes < 0)
    {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
      {
        return ConnectionError::would_block;
      }
      // std::perror("SocketCommunicator::read_message - read");
      // std::exit(-1);
      return ConnectionError::unrecoverable;
    }
    return read_bytes == 0 ? ConnectionError::closed : ConnectionError::no_error;
  }

  std::pair<std::string, std::uint16_t> SocketCommunicator::ip_address_and_port() const noexcept
  {
    sockaddr_in client_address;
    socklen_t len = sizeof(client_address);
    getsockname(handle_, reinterpret_cast<sockaddr*>(&client_address), &len);
    return {inet_ntoa(client_address.sin_addr), client_address.sin_port};
  }

  SocketCommunicator::SocketCommunicator(WithRawHandle, const int handle) noexcept
    : handle_{handle}
  {}

  std::vector<std::byte> read_chunked(SocketCommunicator& conn, const std::size_t num_bytes) noexcept
  {
    // Size of memory to allocate/read each step
    constexpr std::size_t read_step_size     = 0x0'1000;
    constexpr std::size_t allocate_step_size = read_step_size * 16;
    // How often memory needs to be resized
    constexpr std::size_t resize_every_n_steps = allocate_step_size / read_step_size;
    // Ensure that the allocate size is evenly divisible by the read size
    static_assert(allocate_step_size % read_step_size == 0);
    static_assert(allocate_step_size >= read_step_size);
    // To prevent overallocation of memory, don't allocate a ton of memory to start
    std::vector<std::byte> read_bytes;
    // The final bytes to read in the end
    const int final_read_size = num_bytes % read_step_size;
    // Read memory in 4KiB chunks
    const int num_iters = num_bytes / read_step_size + (final_read_size == 0 ? 0 : 1);
    for (int i = 0; i < num_iters; ++i)
    {
      if (i % resize_every_n_steps == 0)
      {
        // Allocate more memory
        const std::size_t mem_left_to_read = num_bytes - read_bytes.size();
        const std::size_t additional_size =
          mem_left_to_read > allocate_step_size
            ? allocate_step_size
            : mem_left_to_read;
        read_bytes.resize(read_bytes.size() + additional_size);
      }
      const std::size_t num_bytes_to_read = (i == num_iters - 1 ? final_read_size : read_step_size);
      // Allocate more memory if needed
      if (conn.read_message(&read_bytes[i * read_step_size], num_bytes_to_read) != ConnectionError::no_error)
      {
        return {};
      }
    }
    return read_bytes;
  }

  std::pair<std::uint16_t, std::string> split_address(const std::string_view address) noexcept
  {
    // Split the address by the colon
    const auto colon_loc = address.find(':');
    if (colon_loc == std::string_view::npos) { return {}; }
    const auto port_str = address.substr(colon_loc + 1);
    // Try to parse the port
    char* end;
    const auto port = strtol(port_str.data(), &end, 10);
    // Check that the entire string was parsed and that the port is valid
    if (end != port_str.data() + port_str.size() || port < 0 || port > 0xFFFF) { return {}; }
    // Try to connect to the publisher
    // Need to make a std::string to ensure that it is null-terminated
    const std::string address_str{address.begin(), address.begin() + colon_loc};
    return {port, address_str};
  }

  std::optional<Subscription> Subscription::try_to_create(const std::string_view address) noexcept
  {
    Subscription to_ret;
    if (to_ret.conn_.connect_to_server(address) != ConnectionError::no_error)
    {
      return {};
    }
    return std::optional<Subscription>{std::move(to_ret)};
  }

  ConnectionError Subscription::read_message(std::byte* const buffer, const std::size_t size) noexcept
  {
    const auto error = conn_.read_message(buffer, size);
    if (error != ConnectionError::no_error && error != ConnectionError::would_block)
    {
      is_disconnected_ = true;
    }
    return error;
  }

  std::vector<std::byte> Subscription::read_chunked(const std::size_t num_bytes) noexcept
  {
    return ::skynet::internal::read_chunked(conn_, num_bytes);
  }

  std::pair<std::string, std::uint16_t> Subscription::ip_address_and_port() const noexcept
  {
    return conn_.ip_address_and_port();
  }

  bool Subscription::is_disconnected() const noexcept
  {
    return is_disconnected_;
  }

  PublicationChannel::PublicationChannel(const std::uint16_t port) noexcept
  {
    if (conn_.set_to_listen(port) != ConnectionError::no_error)
    {
      std::perror("PublicationChannel::PublicationChannel");
      std::exit(-1);
    }
  }

  void PublicationChannel::accept_subscriptions() noexcept
  {
    while (auto new_sub = conn_.accept())
    {
      subscriptions_.push_back(std::move(*new_sub));
    }
  }

  void PublicationChannel::send_message(const std::byte* const message, const std::size_t size) noexcept
  {
    for (std::size_t i = 0; i < subscriptions_.size(); ++i)
    {
      auto& sub = subscriptions_[i];
      if (sub.send_message(message, size) != ConnectionError::no_error)
      {
        SKYNET_DEBUG_LOG(
          "Message from {} failed to be send to {}",
          conn_.ip_address_and_port(),
          sub.ip_address_and_port()
        );
        // Delete the subscription
        using std::swap;
        swap(sub, subscriptions_.back());
        subscriptions_.pop_back();
        // The size decreased, so must also reduce the loop iterator
        --i;
      }
    }
  }

  int PublicationChannel::num_subscriptions() const noexcept
  {
    return static_cast<int>(subscriptions_.size());
  }

} // namespace skynet::internal
