#ifndef SKYNET_SOCKET_HPP__
#define SKYNET_SOCKET_HPP__

#include <unistd.h>

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
      // socket create and verification
      if ((socket_ = socket(type_, SOCK_STREAM, 0)) == -1)
      {
        perror("socket");
        exit(-1);
      }
    }

    /** \brief Construct a new Socket.
     *
     * \param type Specifies the address type to be used.
     * \param socket Specifies socket handle to existing connected socket
     * \param address Specifies the address that socket is connected to
     */
    Socket(int type, int socket, const char* address)
    {
      type_ = type;
      socket_ = socket;
      address_ = address;
    }


    ~Socket()
    { /*close(socket_);*/ } //TODO: figure out why this causes problems

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
        printf("Incorrect address type %d in Socket\n", type_);
        exit(-1);
      }
    }
  }; // class Socket
} // namespace skynet


#endif /* SKYNET_SOCKET_HPP__ */
