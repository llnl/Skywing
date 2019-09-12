#include "skynet/devices/socket_communicator.hpp"

#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <cstdio>
#include <cstring>

namespace
{
  constexpr int invalid_handle = -1;
} // namespace {anonymous}

namespace skynet
{
  SocketCommunicator::SocketCommunicator() noexcept
    : handle_{socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)}
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

  Optional<SocketCommunicator> SocketCommunicator::accept() noexcept
  {
    sockaddr_in client_address_struct;
    // len can't be const as accept takes a non-const pointer
    socklen_t len = sizeof(client_address_struct);

    const int raw_handle = accept4(handle_, reinterpret_cast<sockaddr*>(&client_address_struct), &len, SOCK_NONBLOCK);
    if (raw_handle == invalid_handle)
    {
      // No connection to be made
      if (errno == EAGAIN || errno == EWOULDBLOCK)
      {
        return {};
      }
      std::perror("SocketCommunicator::accept - accept");
      std::exit(-1);
    }

    return SocketCommunicator(WithRawHandle{}, raw_handle);
  }

  void SocketCommunicator::set_to_listen(const std::uint16_t port) noexcept
  {
    constexpr int listen_queue_size = 10;
    sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);
    if (bind(handle_, reinterpret_cast<sockaddr*>(&servaddr), sizeof(servaddr)) < 0)
    {
      std::perror("SocketCommunicator::set_to_listen - bind");
      std::exit(-1);
    }
    if (listen(handle_, listen_queue_size) < 0)
    {
      std::perror("SocketCommunicator::set_to_listen - listen");
      std::exit(-1);
    }
  }

  bool SocketCommunicator::connect_to_server(const char* const address, const std::uint16_t port) noexcept
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
          perror("SocketCommunicator::connect_to_server - poll");
          exit(-1);
        }
      }
      else
      {
        // std::perror("SocketCommunicator::connect_to_server - connect");
        // std::exit(-1);
        return false;
      }
    }
    return true;
  }

  void SocketCommunicator::send_message(const char* const message, const std::size_t size) noexcept
  {
    if (write(handle_, message, size) < 0)
    {
      std::perror("SocketCommunicator::send_message - write");
      std::exit(-1);
    }
  }

  bool SocketCommunicator::read_message(char* const buffer, const std::size_t size) noexcept
  {
    if (read(handle_, buffer, size) < 0)
    {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
      {
        return false;
      }
      std::perror("SocketCommunicator::read_message - read");
      std::exit(-1);
    }
    return true;
  }
} // namespace skynet
