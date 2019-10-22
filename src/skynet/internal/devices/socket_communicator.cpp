#include "skynet/internal/devices/socket_communicator.hpp"

#include "socket_wrappers.hpp"

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

namespace skynet { namespace internal
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
    , address_{std::move(other.address_)}
    , port_{other.port_}
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

    return SocketCommunicator(WithRawHandle{}, raw_handle);
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
    port_ = port;
    return ConnectionError::no_error;
  }

  ConnectionError SocketCommunicator::connect_to_server(const char* const address, const std::uint16_t port) noexcept
  {
    sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, address, &servaddr.sin_addr);
    servaddr.sin_port = ntohs(port);
    if (connect(handle_, reinterpret_cast<sockaddr*>(&servaddr), sizeof(servaddr)) != 0)
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
      }
      else
      {
        // std::perror("SocketCommunicator::connect_to_server - connect");
        // std::exit(-1);
        return ConnectionError::unrecoverable;
      }
    }
    address_ = address;
    port_ = port;
    return ConnectionError::no_error;
  }

  ConnectionError SocketCommunicator::send_message(const std::byte* const message, const std::size_t size) noexcept
  {
    if (write(handle_, reinterpret_cast<const char*>(message), size) < 0)
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
    const auto written = read(handle_, reinterpret_cast<char*>(buffer), size);
    if (written < 0)
    {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
      {
        return ConnectionError::would_block;
      }
      // std::perror("SocketCommunicator::read_message - read");
      // std::exit(-1);
      return ConnectionError::unrecoverable;
    }
    return written == 0 ? ConnectionError::closed : ConnectionError::no_error;
  }
} } // namespace skynet::internal
