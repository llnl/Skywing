#include "catch2/catch.hpp"
#include "devices/Skynet_SocketGateway.hpp"

#include <thread>
#include <iostream>
#include <atomic>

using namespace skynet;

constexpr int skynet_port_dev1 = 4000;
constexpr int skynet_port_dev2 = 4100;
constexpr int skynet_port_dev3 = 4200;

std::atomic<bool> online;

void dev1()
{
  // This device starts first and has nobody to connect to
  {
    std::ofstream outfile("test_config_dev1.txt");
    outfile
      << "skynet_port\t" << skynet_port_dev1 << '\n'
      << "number_of_devices\t0\n"
      << "address_type\tIPv4\n";
  }

  // Create SocketGateway to listen for new clients
  std::cout << "create gateway on Device 1\n";
  const KeyValueReader skynet_config("test_config_dev1.txt", "\t");
  SocketGateway gateway(skynet_config);

  // Have SocketGateway connect to existing devices (there are none)
  std::vector<std::unique_ptr<CommunicatorFactory>> factories =
    gateway.create_initial_connections();

  // Periodically have gateway collect new connections
  while (online)
  {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::vector<std::unique_ptr<CommunicatorFactory>> new_factories
      = gateway.collect_new_connections();
    for (auto&& factory : new_factories)
    {
      factories.push_back(std::move(factory));
      std::cout << "new connection to Device 1\n";
    }
  }

  REQUIRE(factories.size() == 2);
  std::cout << "Device 1 shutting down\n";
}

void dev2()
{
  // This device starts second and knows it needs to connect to dev1
  {
    std::ofstream outfile("test_config_dev2.txt");
    outfile
      << "skynet_port\t" << skynet_port_dev2 << '\n'
      << "number_of_devices\t1\n"
      << "address_type\tIPv4\n"
      << "device1_ip_address\t127.0.0.1\n"
      << "device1_port\t" << skynet_port_dev1 << '\n';
  }

  // Create SocketGateway to listen for new clients
  std::cout << "create gateway on Device 2\n";
  const KeyValueReader skynet_config("test_config_dev2.txt", "\t");
  SocketGateway gateway(skynet_config);

  // Have SocketGateway connect to existing devices (dev1)
  std::vector<std::unique_ptr<CommunicatorFactory>> factories
    = gateway.create_initial_connections();

  // Periodically have gateway collect new connections
  while (online)
  {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::vector<std::unique_ptr<CommunicatorFactory>> new_factories =
      gateway.collect_new_connections();
    for (auto&& factory : new_factories)
    {
      factories.push_back(std::move(factory));
      std::cout << "new connection to Device 2\n";
    }
  }

  REQUIRE(factories.size() == 2);
  std::cout << "Device 2 shutting down\n";
}

void dev3()
{
  // This device starts third and knows it needs to connect to dev1 and dev2
  {
    std::ofstream outfile("test_config_dev3.txt");
    outfile
      << "skynet_port\t" << skynet_port_dev3 << '\n'
      << "number_of_devices\t2\n"
      << "address_type\tIPv4\n"
      << "device1_ip_address\t127.0.0.1\n"
      << "device1_port\t" << skynet_port_dev1 << '\n'
      << "device2_ip_address\t127.0.0.1\n"
      << "device2_port\t" << skynet_port_dev2 << '\n';
  }
  // Create SocketGateway to listen for new clients
  std::cout << "create gateway on Device 3\n";
  const KeyValueReader skynet_config("test_config_dev3.txt", "\t");
  SocketGateway gateway(skynet_config);

  // Have SocketGateway connect to existing devices (dev1 and dev2)
  std::vector<std::unique_ptr<CommunicatorFactory>> factories =
    gateway.create_initial_connections();

  // Periodically have gateway collect new connections
  while (online)
  {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::vector<std::unique_ptr<CommunicatorFactory>> new_factories =
      gateway.collect_new_connections();
    for (auto&& factory : new_factories)
    {
      factories.push_back(std::move(factory));
      std::cout << "new connection to Device 3\n";
    }
  }

  REQUIRE(factories.size() == 2);
  std::cout << "Device 3 shutting down\n";
}

TEST_CASE("Gateway connection methods work", "[Skynet_SocketGateway]")
{
  online = true;

  std::cout << "Starting Device 1\n";
  std::thread dev1_thread = std::thread(&dev1);
  std::this_thread::sleep_for(std::chrono::seconds(5));

  std::cout << "Starting Device 2\n";
  std::thread dev2_thread = std::thread(&dev2);
  std::this_thread::sleep_for(std::chrono::seconds(5));

  std::cout << "Starting Device 3\n";
  std::thread dev3_thread = std::thread(&dev3);
  std::this_thread::sleep_for(std::chrono::seconds(5));

  online = false;
  dev1_thread.join();
  dev2_thread.join();
  dev3_thread.join();
}
