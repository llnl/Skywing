#ifndef SKYNET_SOCKET_HPP__
#define SKYNET_SOCKET_HPP__

#include <arpa/inet.h>
#include <strings.h>
#include <unistd.h>

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
    Socket(int address_type)
    {
      address_type_ = address_type;
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
     * \param address_type Specifies the address type to be used.
     * \param socket_handle Specifies socket handle to existing connected socket
     * \param address Specifies the address that socket is connected to
     */
    Socket(int address_type, int socket_handle, const char* address)
    {
      address_type_ = address_type;
      socket_handle_ = socket_handle;
      address_ = address;
    }

    void bind_to_port(int port, bool try_other_ports, const char* client_address)
    {
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
    }

    void set_to_listen(int queue_length)
    {
      listen(socket_handle_, queue_length);
    }

    int get_handle() const
    {
      return socket_handle_;
    }

    int get_address_type() const
    {
      return address_type_;
    }


    ~Socket()
    { /*close(socket_handle_);*/ } //TODO: figure out why this causes problems

    const char * get_address() const
    { return address_; }


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
    const char * address_;
  }; // class Socket
} // namespace skynet


#endif /* SKYNET_SOCKET_HPP__ */
