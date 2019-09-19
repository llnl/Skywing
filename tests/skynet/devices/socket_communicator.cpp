#include <catch2/catch.hpp>

#include "skynet/internal/devices/socket_communicator.hpp"

#include <thread>
#include <cstring>
#include <array>
#include <chrono>

using namespace skynet;
using namespace skynet::internal;

constexpr std::uint16_t port = 40000;
constexpr int value_to_send = 3871;

void server()
{
  SocketCommunicator conn;
  conn.set_to_listen(port);
  // Wait for the client to connect
  SocketCommunicator with_client = [&]() {
    while (true)
    {
      if (auto val = conn.accept())
      {
        return std::move(*val);
      }
    }
  }();
  // Wait for an int to be send
  std::array<char, sizeof(int)> int_buffer;
  while (with_client.read_message(int_buffer.data(), int_buffer.size()) != ConnectionError::no_error)
  {
    // empty
  }
  // verify that it's the same
  int read_value;
  std::memcpy(&read_value, int_buffer.data(), sizeof(int));
  REQUIRE(read_value == value_to_send);
}

void client()
{
  SocketCommunicator conn;
  REQUIRE(conn.connect_to_server("127.0.0.1", port) == ConnectionError::no_error);
  std::array<char, sizeof(int)> int_buffer;
  std::memcpy(int_buffer.data(), &value_to_send, sizeof(int));
  conn.send_message(int_buffer.data(), int_buffer.size());
}

TEST_CASE("Communicating between sockets works", "[Skynet_SocketCommunicator]")
{
  using namespace std::chrono_literals;
  std::thread s(server);
  // Allow the server to start
  std::this_thread::sleep_for(10ms);
  std::thread c(client);
  s.join();
  c.join();
}
