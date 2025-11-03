#ifndef SKYWING_SRC_SOCKET_WRAPPERS_HPP
#define SKYWING_SRC_SOCKET_WRAPPERS_HPP

// OSX has to go through a few more steps to init non-blocking sockets, so
// these wrappers are to help with that

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/ioctl.h>

namespace skywing::internal
{
/** \brief Creates a socket in non-blocking mode
 */
int create_non_blocking(int address_family) noexcept;

/** \brief Accepts on a socket and puts the connection in non-blocking mode
 */
int accept_make_non_blocking(const int sockfd,
                             sockaddr* addr,
                             socklen_t* addrlen) noexcept;
} // namespace skywing::internal

#endif // SKYWING_SRC_SOCKET_WRAPPERS_HPP
