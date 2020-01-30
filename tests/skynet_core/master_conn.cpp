#include <catch2/catch.hpp>

#include "skynet_core/master.hpp"

#include <thread>

using namespace skynet;

constexpr std::uint16_t server_port = 10000;
constexpr std::uint16_t client_port = 20000;
constexpr const char* server_id = "server";
constexpr const char* client_id = "client";

std::mutex catch_mutex;

void server()
{
  Master server{server_port, server_id};
  while (server.number_of_neighbors() == 0)
  {
    server.accept_pending_connections();
  }
  std::lock_guard g{catch_mutex};
  REQUIRE(server.number_of_neighbors() == 1);
}

void client()
{
  Master client{client_port, client_id};
  // This will block until it connects
  std::lock_guard g{catch_mutex};
  REQUIRE(client.connect_to_server("127.0.0.1", server_port));
  REQUIRE(client.number_of_neighbors() == 1);
}

TEST_CASE("Connecting Masters works", "[Skynet_MasterConn]")
{
  std::thread server_thread{server};
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  std::thread client_thread{client};
  server_thread.join();
  client_thread.join();
}
