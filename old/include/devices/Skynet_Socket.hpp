#ifndef SKYNET_SOCKET_HPP__
#define SKYNET_SOCKET_HPP__

#include <arpa/inet.h>
#include <strings.h>
#include <unistd.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <iostream>
#include <string>

namespace skynet
{
  /** \brief Simple wrapper for low-level sockets
   */
  class Socket
  {
  public:
    static constexpr int ipv4 = AF_INET; // redefinition of AF_INET const

    /** \brief Construct a new Socket.
     *
     * \param address_type Specifies the address type to be used.
     */
    Socket(const int address_type) :
      address_type_(address_type)
    {
      confirm_supported_address_type();
      // socket create and verification
      if ((socket_handle_ = socket(address_type_, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1)
      {
        perror("socket");
        exit(-1);
      }
    }

    /** \brief Delete copy constructor and copy assignment operators
     */
    Socket(const Socket& other) = delete;
    Socket& operator=(const Socket& other) = delete;

    // Move constructor
    Socket(Socket&& other) noexcept
      : socket_handle_(other.socket_handle_),
        address_type_(other.address_type_)
    {
      other.socket_handle_ = -1;
    }

    // Move assignment operator
    Socket& operator=(Socket&& other) noexcept
    {
      // Do this in a round-about way to handle self-assignment
      const int handle = other.socket_handle_;
      other.socket_handle_ = -1;
      socket_handle_ = handle;
      return *this;
    }

    // Destructor
    ~Socket()
    {
      if (socket_handle_ != -1)
      {
        close(socket_handle_);
      }
    }

    /** \brief Accepts an incoming connection
     *
     * \return A Socket with the new connection
     */
    Socket accept() const
    {
      sockaddr_in client_address_struct;
      // len can't be const as accept takes a non-const pointer
      socklen_t len = sizeof(client_address_struct);

      // Accept the data packet from client and verification
      int raw_handle = ::accept4(socket_handle_, reinterpret_cast<sockaddr*>(&client_address_struct), &len, SOCK_NONBLOCK);
      if (raw_handle < 0)
      {
        perror("accept");
        exit(-1);
      }

      // TODO: Is this address needed for any reason?
      // Can maybe move this into the switch somehow
      // char buffer[INET_ADDRSTRLEN];
      // std::string address;
      // switch(address_type_)
      // {
      //   case ipv4:
      //     address = inet_ntop(ipv4, &client_address_struct.sin_addr, buffer, INET_ADDRSTRLEN);
      //     break;
      // }
      // return std::make_pair(Socket(raw_handle), address);

      return Socket(with_raw_handle{}, raw_handle);
    }

    /** \brief Binds the socket to a port/address
     *
     * \param port The port to bind to
     * \param try_other_ports If other ports should be tried if the initial one fails
     * \param client_address The address to bind to, if any
     * \return The port that was bound to
     */
    uint16_t bind_to_port(const uint16_t port, const bool try_other_ports, const char* const client_address)
    {
      // Socket stucture
      sockaddr_in servaddr;
      bzero(&servaddr, sizeof(servaddr));

      switch(address_type_)
      {
        case ipv4:
          servaddr.sin_family = ipv4;
          if (client_address != nullptr)
            inet_pton(ipv4, client_address, &servaddr.sin_addr);
          else
            servaddr.sin_addr.s_addr = INADDR_ANY;
          break;
      }

      for (auto test_port = port; test_port < UINT16_MAX; ++test_port)
      {
        servaddr.sin_port = htons(test_port);
        if (bind(socket_handle_, reinterpret_cast<sockaddr*>(&servaddr), sizeof(servaddr)) == 0)
        {
          return test_port;
        }
        else if (!try_other_ports)
        {
          perror("bind");
          exit(-1);
        }
      }
      // TODO: What to do here?
      std::cout << "Ports exhausted\n";
      exit(-1);
    }

    /** \brief Set the socket to listen for incoming connections
     */
    void set_to_listen(const int queue_length)
    {
      listen(socket_handle_, queue_length);
    }

    /** \brief Connect to a server socket.
     *
     * \param server_address The address of the server socket.
     * \param port Which port number to connect to on the server.
     */
    void connect_to_server(const char* const server_address, const uint16_t port)
    {
      //Socket stucture
      sockaddr_in servaddr;
      bzero(&servaddr, sizeof(servaddr));

      switch(address_type_)
      {
        case ipv4:
          servaddr.sin_family = ipv4;

          inet_pton(ipv4, server_address, &servaddr.sin_addr);
          break;
      }

      servaddr.sin_port = ntohs(port);

      // connect the client socket to server socket
      if (connect(socket_handle_, reinterpret_cast<sockaddr*>(&servaddr), sizeof(servaddr)) != 0)
      {
        if (errno == EINPROGRESS)
        {
          return;
        }
        perror("connect");
        exit(-1);
      }
    }

    /** \brief Returns the number of connections in the queue
     */
    int query_queue() const
    {
      fd_set set;
      timeval timeout;
      FD_ZERO(&set);
      FD_SET(socket_handle_, &set);
      timeout.tv_sec = 0;
      timeout.tv_usec = 0;

      return select(socket_handle_ + 1, &set, nullptr, nullptr, &timeout);
    }

    /** \brief Sends a message using the socket
     *
     * \param message The message to send
     * \param message_size The size of the message
     * \return true if the message was sent, false if it would block
     */
    bool send_message(const void* const message, const std::size_t message_size) const
    {
      if (write(socket_handle_, message, message_size) == -1)
      {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
          return false;
        }
        perror("write");
        exit(-1);
      }
      return true;
    }

    /** \brief Reads a message from the socket
     *
     * \param buffer The buffer to read into
     * \param buffer_size The size of the buffer
     * \return true if a message was read, false if it would have blocked
     */
    bool read_message(void* const buffer, const std::size_t buffer_size) const
    {
      if (read(socket_handle_, buffer, buffer_size) == -1)
      {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
          return false;
        }
        perror("read");
        exit(-1);
      }
      return true;
    }

    /** \brief Waits until a connection is complete and ready to write to
     */
    void wait_to_connect()
    {
      pollfd to_poll;
      to_poll.fd = socket_handle_;
      to_poll.events = POLLOUT;
      if (poll(&to_poll, 1, -1) == -1)
      {
        perror("poll");
        exit(-1);
      }
    }

  private:
    // Tag for using the raw handle constructor
    struct with_raw_handle{};

    /** \brief Create a socket from a raw handle
     *
     * \param handle The raw handle
     */
    Socket(with_raw_handle, const int handle) :
      socket_handle_(handle)
    {}

    /** \brief Confirm that this object's address type is supported.
     *
     * Exits with an error message if the type is not supported
     */
    void confirm_supported_address_type()
    {
      if (address_type_ != ipv4)
      {
        std::cout << "Incorrect address type " << address_type_ << " in Socket\n";
        exit(-1);
      }
    }

    // Raw socket handle
    int socket_handle_ = -1;
    int address_type_;
  }; // class Socket
} // namespace skynet


#endif /* SKYNET_SOCKET_HPP__ */
