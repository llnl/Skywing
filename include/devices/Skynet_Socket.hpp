#ifndef SKYNET_SOCKET_HPP__
#define SKYNET_SOCKET_HPP__

//#include <cstdint>
//#include <stdio.h>
//#include <stdlib.h>
#include <unistd.h>
//#include <string.h>
//#include <sys/types.h>
//#include <sys/socket.h>
//#include <netinet/in.h>
//#include <netdb.h>
//#include <netinet/in.h>
//#include <stdlib.h>
//#include <string.h>
//#include <sys/socket.h>
//#include <sys/types.h>
//#include <arpa/inet.h>
//#define MAX 80
//#define SA struct sockaddr

namespace skynet
{
  class Socket
  {
  public:

    static const int IPv4 = AF_INET; // redefinition of AF_INET const

    const char * get_address() const
    { return address_; }

  protected:
    /** \brief Construct a new Socket.
     *
     * \param type Specifies the address type to be used.
     */
    Socket(int type)
    {
      type_ = type;
      confirm_supported_type();
      socket_ = socket(type_, SOCK_STREAM, 0);
      // socket create and verification
      if (socket_ == -1) {
        printf("socket creation failed...\n");
        exit(0);
      }
      else
        printf("Socket successfully created..\n");
    }

    /** \brief Construct a new Socket.
     *
     * \param type Specifies the address type to be used.
     * \param socket Specifies socket handle to existing socket
     */
    Socket(int type, int socket, const char* address)
    {
      type_ = type;
      socket_ = socket;
      address_ = address;
    }


    ~Socket()
    { close(socket_); }

    int socket_;
    int type_;
    const char * address_;

  private:

    /** \brief Confirm that this object's address type is supported.
     *
     * Exits with an error message if the type is not supported
     */
    void confirm_supported_type()
    {
      if (type_ != IPv4)
      {
        printf("Incorrect socket type in SocketCommunicator\n");
        exit(-1);
      }
    }
  }; // class Socket
} // namespace skynet


#endif /* SKYNET_SOCKET_HPP__ */
