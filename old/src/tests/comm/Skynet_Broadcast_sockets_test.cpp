#include <catch2/catch.hpp>

#include <array>
#include <string>
#include <fstream>
#include <thread>
#include <vector>
#include <cstring>
#include <chrono>
#include <algorithm>
#include <iostream>

#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>

// Mostly copy-pasted from Skynet but with a few changes to make them non-blocking
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
  Socket(Socket&& other) noexcept :
    socket_handle_(other.socket_handle_)
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
    int raw_handle = ::accept(socket_handle_, reinterpret_cast<sockaddr*>(&client_address_struct), &len);
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
   */
  void send_message(const void* const message, const std::size_t message_size) const
  { write(socket_handle_, message, message_size); }

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

  // Waits until the connection is ready to write to
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

  friend bool operator==(const Socket& lhs, const Socket& rhs) noexcept
  {
    return lhs.socket_handle_ == rhs.socket_handle_ && lhs.address_type_ == rhs.address_type_;
  }
}; // class Socket

bool operator!=(const Socket& lhs, const Socket& rhs) noexcept
{
  return !(lhs == rhs);
}

// Emulate multiple machines just with multiple threads at this point
// The network looks like this:
//  M1   +--M2
//   |   |   |
//  M3--M4--M5
//   |       |
//   +-------+
// Where higher number devices know about lower numbered connected ones

constexpr std::array<std::uint16_t, 5> ports{
  6000,
  6100,
  6200,
  6300,
  6400
};

constexpr std::array<std::size_t, 5> machine_counts{1, 2, 3, 3, 3};

// default machine connections to make
constexpr std::array<std::array<int, 3>, 5> to_connect{
  std::array<int, 3>{-1, -1, -1},
  std::array<int, 3>{-1, -1, -1},
  std::array<int, 3>{ 0, -1, -1},
  std::array<int, 3>{ 1,  2, -1},
  std::array<int, 3>{ 1,  2,  3}
};

struct BroadcastMessage
{
  int id;
  int data;
};

constexpr bool operator==(const BroadcastMessage& lhs, const BroadcastMessage& rhs) noexcept
{
  return lhs.id == rhs.id && lhs.data == rhs.data;
}

void machine_task(const std::size_t index)
{
  using namespace std::chrono_literals;
  Socket listener(Socket::ipv4);
  listener.bind_to_port(ports[index], false, nullptr);
  listener.set_to_listen(10);
  std::vector<Socket> connections;
  // make default connections
  for (const auto machine : to_connect[index])
  {
    if (machine == -1)
    {
      break;
    }
    Socket new_conn(Socket::ipv4);
    new_conn.connect_to_server("127.0.0.1", ports[machine]);
    new_conn.wait_to_connect();
    connections.push_back(std::move(new_conn));
  }
  // listen for connections until all connections are made
  while (connections.size() != machine_counts[index])
  {
    if (listener.query_queue() > 0)
    {
      connections.push_back(listener.accept());
      connections.back().wait_to_connect();
    }
    else
    {
      std::this_thread::sleep_for(1ms);
    }
  }
  // In real code the broadcast checking (and other things) could be combined
  // with the above in a loop
  int last_heard = 0;
  for (std::size_t send_index = 0; send_index < ports.size(); ++send_index)
  {
    const BroadcastMessage to_broadcast{
      static_cast<int>(5 + send_index),
      static_cast<int>(50 + 20 * send_index)
    };
    // send the broadcast if the index matches
    if (index == send_index)
    {
      for (auto&& conn : connections)
      {
        conn.send_message(static_cast<const void*>(&to_broadcast), sizeof(to_broadcast));
      }
      last_heard = to_broadcast.id;
    }
    else
    {
      // I don't like this but with nested loops the alternative is a seperate
      // function
      bool done = false;
      // keep trying to recieve the broadcast until it comes through
      while (!done)
      {
        for (auto&& conn : connections)
        {
          BroadcastMessage message;
          if (conn.read_message(static_cast<void*>(&message), sizeof(message)))
          {
            // ignore old message ID's
            if (message.id <= last_heard)
            {
              continue;
            }
            last_heard = message.id;
            // otherwise the result should be the same
            REQUIRE(message == to_broadcast);
            // and then relay it to all neighbors
            // aside from the sending one
            for (auto&& neighbor : connections)
            {
              if (neighbor != conn)
              {
                neighbor.send_message(static_cast<const void*>(&to_broadcast), sizeof(to_broadcast));
              }
            }
            done = true;
            break;
          }
        }
      }
    }
  }
}

TEST_CASE("Broadcast with raw sockets works", "[Skynet_Broadcast_Sockets]")
{
  using namespace std::chrono_literals;
  std::vector<std::thread> threads;
  for (std::size_t i = 0; i < ports.size(); ++i)
  {
    threads.emplace_back(machine_task, i);
    // Give each task time to start
    std::this_thread::sleep_for(10ms);
  }
  for (auto&& thread : threads)
  {
    thread.join();
  }
}