#include <catch2/catch.hpp>

#include "skynet/master.hpp"

#include <thread>

constexpr std::uint16_t server_port = 10000;
constexpr std::uint16_t client_port = 20000;
constexpr std::uint32_t server_id = 1;
constexpr std::uint32_t client_id = 2;

using namespace skynet;

void server()
{
  Master server{server_port, server_id};
  // There's no way to see if new connections have been made
  // (Which I think normally is fine)
  // So just wait long enough to ensure that the client is trying to connect
  std::this_thread::sleep_for(std::chrono::seconds(1));
  server.make_pending_connections();
}

void client()
{
  Master client{client_port, client_id};
  // This will block until it connects
  client.connect_to_server("127.0.0.1", server_port);
}

TEST_CASE("Connecting Masters works", "[Skynet_MasterConn]")
{
  std::thread server_thread{server};
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  std::thread client_thread{client};
  server_thread.join();
  client_thread.join();
}
