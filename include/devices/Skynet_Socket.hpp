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
    Socket(int address_type, uint16_t port) : address_type_(address_type), port_(port)
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
    Socket(Socket& listener)
    {
      address_type_ = listener.address_type_;
      struct sockaddr_in client_address_struct;
      socklen_t len = sizeof(client_address_struct);

      // Accept the data packet from client and verification
      socket_handle_ = accept(listener.socket_handle_, (struct sockaddr *) &client_address_struct, &len);
      if (socket_handle_ < 0)
      {
        perror("accept");
        exit(-1);
      }

      switch(address_type_)
      {
        case Socket::IPv4:
          address_ = inet_ntop(Socket::IPv4, &(client_address_struct.sin_addr), ipv4Buf_, INET_ADDRSTRLEN);
          break;
      }
    }

    ~Socket()
    { close(socket_handle_);socket_handle_ = -1; }

    /** \brief Delete copy & move constructors and copy & move assignment operators
     */
    Socket(const Socket& other) = delete;
    Socket& operator=(const Socket& other) = delete;
    Socket(Socket&& other) = delete;
    Socket& operator=(Socket&& other) = delete;

    /** \brief Construct a new Socket.
     *
     * \param address_type Specifies the address type to be used.
     * \param socket_handle Specifies socket handle to existing connected socket
     * \param address Specifies the address that socket is connected to
     */
    Socket(int address_type, int socket_handle, const char* address)
    {
      std::cout<<" port = "<<port_<<std::endl;

      address_type_ = address_type;
      socket_handle_ = socket_handle;
      address_ = address;
    }

    uint16_t bind_to_port(uint16_t port, bool try_other_ports, const char* client_address)
    {
      port_ = port;
      //Socket stucture
      struct sockaddr_in servaddr;
      bzero(&servaddr, sizeof(servaddr));

      switch(address_type_)
      {
        case Socket::IPv4:
          servaddr.sin_family = Socket::IPv4;
          if (client_address != NULL)
            inet_pton(Socket::IPv4, client_address, &(servaddr.sin_addr));
          else
            servaddr.sin_addr.s_addr = INADDR_ANY;
          break;
      }

      bool bound = false;
      do
      {
        servaddr.sin_port = htons(port);
        port_ = port;
        // Binding newly created socket to given IP and verification
        if (bind(socket_handle_, (struct sockaddr*)&servaddr, sizeof(servaddr)) == 0)
          bound = true;
        else
        {
          if (try_other_ports)
            port++;
          else
          {
            perror("bind");
            exit(-1);
          }
        }
      } while (!bound && port < UINT16_MAX);

      return port;
    }

    void set_to_listen(int queue_length)
    {
      listen(socket_handle_, queue_length);
    }

    /** \brief Connect to a server socket.
     *
     * \param server_address The address of the server socket.
     * \param port Which port number to connect to on the server.
     */
    void connect_to_server(const char * server_address, uint16_t port)
    {
      //Socket stucture
      struct sockaddr_in servaddr;
      bzero(&servaddr, sizeof(servaddr));

      switch(address_type_)
      {
        case Socket::IPv4:
          servaddr.sin_family = Socket::IPv4;

          inet_pton(Socket::IPv4, server_address, &(servaddr.sin_addr));
          break;
      }

      servaddr.sin_port = ntohs(port);

      // connect the client socket to server socket
      if (connect(socket_handle_, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0)
      {
        perror("connect");
        exit(-1);
      }
    }

    std::unique_ptr<Socket> connect_new_socket_to_client()
    {
      const char * client_address;
      struct sockaddr_in client_address_struct;
      socklen_t len = sizeof(client_address_struct);

      // Accept the data packet from client and verification
      int new_socket_handle = accept(socket_handle_, (struct sockaddr *) &client_address_struct, &len);
      if (new_socket_handle < 0)
      {
        perror("accept");
        exit(-1);
      }

      // TODO: actually retrieve address of client
      switch(address_type_)
      {
        case Socket::IPv4:
          client_address = inet_ntop(Socket::IPv4, &(client_address_struct.sin_addr), ipv4Buf_, INET_ADDRSTRLEN);
          break;
      }

      return std::make_unique<Socket>(address_type_, new_socket_handle, client_address);
    }

    int query_queue()
    {
      fd_set set;
      struct timeval timeout;
      FD_ZERO(&set);
      FD_SET(socket_handle_, &set);
      timeout.tv_sec = 0;
      timeout.tv_usec = 0;

      return select(socket_handle_+1, &set, NULL, NULL, &timeout);
    }

    void send_message(const void* message, std::size_t message_size) const
    { write(socket_handle_, message, message_size); }

    void read_message(void* buffer, std::size_t buffer_size) const
    {
      if (read(socket_handle_, buffer, buffer_size) == -1)
      {
        perror("read");
        exit(-1);
      }
    }

    /** \brief Closes socket and sets handle to -1.
     *
     */
    void close_socket(){
      close(socket_handle_);
      socket_handle_ = -1;
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
        printf("Incorrect address type %d in Socket\n", address_type_);
        exit(-1);
      }
    }

    int socket_handle_;
    int address_type_;
    uint16_t port_;
    const char * address_;
    char ipv4Buf_[INET_ADDRSTRLEN];

  }; // class Socket
} // namespace skynet


#endif /* SKYNET_SOCKET_HPP__ */
