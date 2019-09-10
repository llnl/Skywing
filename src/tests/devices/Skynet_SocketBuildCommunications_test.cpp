#include "catch2/catch.hpp"
#include "devices/Skynet_SocketGateway.hpp"

#include <thread>
#include <iostream>
#include <iterator>
#include <algorithm>

using namespace skynet;

constexpr int skynet_port_dev1 = 5000;
constexpr int skynet_port_dev2 = 5100;

void device1()
{
  // This device starts first and has nobody to connect to
  {
    std::ofstream outfile("test_config_dev11.txt");
    outfile
      << "skynet_port\t" << skynet_port_dev1 << '\n'
      << "skynet_ip_address\t127.0.0.1\n"
      << "number_of_devices\t0\n"
      << "address_type\tIPv4\n";
  }
  std::vector<std::unique_ptr<CommunicatorFactory>> factories;

  // Create SocketGateway to listen for new clients
  std::cout << "create gateway on Device 1\n";
  const KeyValueReader skynet_config("test_config_dev11.txt", "\t");
  SocketGateway gateway(skynet_config);

  // Have SocketGateway connect to existing devices (there are none)
  (void)gateway.create_initial_connections();

  // Periodically have gateway collect new connections (in this case we will
  // only do this untill Device 1 is connected to Device 2)
  while (true)
  {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (auto factory = gateway.collect_new_connection())
    {
      factories.push_back(std::move(factory));
      break;
    }
  }
  std::cout << "Device 2 connected to Device 1\n";

  // Create new Socket Communicators from the constucted factories
  std::cout << "Device 1 is creating communicators......\n";
  std::vector<std::unique_ptr<DeviceCommunicator>> communicators;
  while (communicators.size() != 1)
  {
    for (auto&& factory : factories)
    {
      const std::vector<std::string> comm_config;
      if (auto comm = factory->create_new_communicator(comm_config))
      {
        communicators.push_back(std::move(comm));
      }
    }
  }

  std::cout << "Device 1 is checking if factories have requests....\n";

  // Create new Socket Communicators from the constucted factories
  while (communicators.size() != 2)
  {
    for (auto&& factory : factories)
    {
      if (auto comm = factory->create_requested_communicator())
      {
        communicators.push_back(std::move(comm));
      }
    }
  }

  /*Check that two Socket Communicatiors were created */
  std::cout << "Device 1 has " << communicators.size() << " Socket Communicators\n";
  REQUIRE(communicators.size() == 2);
}

void device2()
{
    // This device starts second and knows it needs to connect to dev1
    {
      std::ofstream outfile("test_config_dev22.txt");
      outfile
        << "skynet_port\t" << skynet_port_dev2 << '\n'
        << "skynet_ip_address\t127.0.0.1\n"
        << "number_of_devices\t1\n"
        << "address_type\tIPv4\n"
        << "device1_ip_address\t127.0.0.1\n"
        << "device1_port\t" << skynet_port_dev1 << '\n';
    }

    // Create SocketGateway to listen for new clients
    std::cout << "create gateway on Device 2\n";
    const KeyValueReader skynet_config("test_config_dev22.txt", "\t");
    SocketGateway gateway(skynet_config);

    // Have SocketGateway connect to existing devices (dev1)
    std::vector<std::unique_ptr<CommunicatorFactory>> factories =
      gateway.create_initial_connections();

    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::cout << "Device 2 is checking if factories have requests....\n";

    std::vector<std::unique_ptr<DeviceCommunicator>> communicators;
    // Create new Socket Communicators from the constucted factories
    while (communicators.size() != 1)
    {
      for (auto&& factory : factories)
      {
        if (auto comm = factory->create_requested_communicator())
        {
          communicators.push_back(std::move(comm));
        }
      }
    }

    std::cout << "Device 2 is creating communicators......\n";
    while (communicators.size() != 2)
    {
      for (auto&& factory : factories)
      {
        const std::vector<std::string> comm_config;
        if (auto comm = factory->create_new_communicator(comm_config))
        {
          communicators.push_back(std::move(comm));
        }
      }
    }
    std::cout << "Device 2 has " << communicators.size() << " Socket Communicators\n";
    // Check that two Socket Communicatiors were created
    REQUIRE(communicators.size() == 2);

}


TEST_CASE("Communication methods work", "[Skynet_SocketCommunicator]")
{
  std::cout << "create threads for testing....\n";

  std::cout << "Starting Device 1\n";
  std::thread dev1_thread(&device1);
  std::this_thread::sleep_for(std::chrono::seconds(5));

  std::cout << "Starting Device 2\n";
  std::thread dev2_thread(&device2);
  std::this_thread::sleep_for(std::chrono::seconds(5));

  dev1_thread.join();
  dev2_thread.join();
}
