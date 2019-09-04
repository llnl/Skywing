#ifndef SKYNET_SOCKET_HPP__
#define SKYNET_SOCKET_HPP__

#include <arpa/inet.h>
#include <strings.h>
#include <unistd.h>
#include <iostream>

namespace skynet
{
  class Socket
  {
  public:

    static const int IPv4 = AF_INET; // redefinition of AF_INET const

    /** \brief Construct a new Socket.
     *
     * \param address_type Specifies the address type to be used.
     */
    Socket(const int address_type, const uint16_t port)
      : address_type_(address_type),
        port_(port)
    {
      confirm_supported_address_type();
      // socket create and verification
      if ((socket_handle_ = socket(address_type_, SOCK_STREAM, 0)) == -1)
      {
        perror("socket");
        exit(-1);
      }
    }

    /** \brief Construct a new Socket.
     *
     * \param listener The Socket object that is listening for new connections
     */
    // TODO: This originally took a non-const reference and the normal copy constructor
    //       was deleted below; this is very strange, was it an oversight?
    Socket(const Socket& listener)
      : address_type_(listener.address_type_)
    {
      sockaddr_in client_address_struct;
      // len can't be const as accept takes a non-const pointer
      socklen_t len = sizeof(client_address_struct);

      // Accept the data packet from client and verification
      socket_handle_ = accept(listener.socket_handle_, (sockaddr*) &client_address_struct, &len);
      if (socket_handle_ < 0)
      {
        perror("accept");
        exit(-1);
      }

      switch(address_type_)
      {
        case Socket::IPv4:
          address_ = inet_ntop(Socket::IPv4, &client_address_struct.sin_addr, ipv4Buf_, INET_ADDRSTRLEN);
          break;
      }
    }

    ~Socket()
    { close(socket_handle_); socket_handle_ = -1; }

    /** \brief Delete copy & move constructors and copy & move assignment operators
     */
    // See TODO above about why this is commented out
    //Socket(const Socket& other) = delete;
    Socket& operator=(const Socket& other) = delete;
    Socket(Socket&& other) = delete;
    Socket& operator=(Socket&& other) = delete;

    uint16_t bind_to_port(uint16_t port, const bool try_other_ports, const char* const client_address)
    {
      // Socket stucture
      sockaddr_in servaddr;
      bzero(&servaddr, sizeof(servaddr));

      switch(address_type_)
      {
        case Socket::IPv4:
          servaddr.sin_family = Socket::IPv4;
          if (client_address != NULL)
            inet_pton(Socket::IPv4, client_address, &servaddr.sin_addr);
          else
            servaddr.sin_addr.s_addr = INADDR_ANY;
          break;
      }

      bool bound = false;
      do
      {
        servaddr.sin_port = htons(port);
        // Binding newly created socket to given IP and verification
        if (bind(socket_handle_, reinterpret_cast<sockaddr*>(&servaddr), sizeof(servaddr)) == 0)
        {
          bound = true;
          port_ = port;
        }
        else
        {
          if (try_other_ports)
          {
            ++port;
          }
          else
          {
            perror("bind");
            exit(-1);
          }
        }
      } while (!bound && port < UINT16_MAX);

      return port;
    }

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
        case Socket::IPv4:
          servaddr.sin_family = Socket::IPv4;

          inet_pton(Socket::IPv4, server_address, &servaddr.sin_addr);
          break;
      }

      servaddr.sin_port = ntohs(port);

      // connect the client socket to server socket
      if (connect(socket_handle_, reinterpret_cast<sockaddr*>(&servaddr), sizeof(servaddr)) != 0)
      {
        perror("connect");
        exit(-1);
      }
    }

    int query_queue()
    {
      fd_set set;
      timeval timeout;
      FD_ZERO(&set);
      FD_SET(socket_handle_, &set);
      timeout.tv_sec = 0;
      timeout.tv_usec = 0;

      return select(socket_handle_ + 1, &set, NULL, NULL, &timeout);
    }

    void send_message(const void* const message, const std::size_t message_size) const
    { write(socket_handle_, message, message_size); }

    void read_message(void* const buffer, const std::size_t buffer_size) const
    {
      if (read(socket_handle_, buffer, buffer_size) == -1)
      {
        perror("read");
        exit(-1);
      }
    }

    /** \brief Returns port number of socket.
     *
     */
    uint16_t get_port() const
    {
      return port_;
    }
  private:

    /** \brief Confirm that this object's address type is supported.
     *
     * Exits with an error message if the type is not supported
     */
    void confirm_supported_address_type()
    {
      if (address_type_ != IPv4)
      {
        std::cout << "Incorrect address type " << address_type_ << " in Socket\n";
        exit(-1);
      }
    }

    int socket_handle_;
    int address_type_;
    uint16_t port_;
    const char* address_;
    char ipv4Buf_[INET_ADDRSTRLEN];

  }; // class Socket
} // namespace skynet


#endif /* SKYNET_SOCKET_HPP__ */
