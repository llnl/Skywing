#include "catch2/catch.hpp"
#include "devices/Skynet_SocketGateway.hpp"

#include <thread>
#include <iostream>
using namespace skynet;


#define SKYNET_PORT_DEV1 5000
#define SKYNET_PORT_DEV2 5100
#define SKYNET_PORT_DEV3 5200

bool online;

void dev1()
{
  // This device starts first and has nobody to connect to
  std::ofstream outfile("test_config_dev1.txt");
  outfile << "skynet_port\t" << SKYNET_PORT_DEV1 << std::endl;
  outfile << "number_of_devices\t0" << std::endl;
  outfile << "address_type\tIPv4" << std::endl;
  outfile.close();
  std::vector<std::string> comm_config(0);
  std::vector<std::unique_ptr<CommunicatorFactory>> factories;
  std::vector<std::unique_ptr<CommunicatorFactory>> new_factories;
  std::vector<std::unique_ptr<DeviceCommunicator>> communicators;


  // Create SocketGateway to listen for new clients
  std::cout << "create gateway on Device 1" << std::endl;
  KeyValueReader skynet_config("test_config_dev1.txt", "\t");
  SocketGateway gateway(skynet_config);

  // Have SocketGateway connect to existing devices (there are none)
  factories = gateway.create_initial_connections();
  for (uint i=0; i < factories.size(); i++)
  {
    communicators.push_back(factories[i]->create_new_communicator(comm_config));
    std::cout << "created initial connection on Device 1" << std::endl;
  }

  // Periodically have gateway collect new connections
  while (online)
  {
    std::this_thread::sleep_for (std::chrono::seconds(1));
    new_factories = gateway.collect_new_connections();
    for (uint i=0; i < new_factories.size(); i++)
    {
      communicators.push_back(new_factories[i]->create_new_communicator(comm_config));
      std::cout << "new connection to Device 1" << std::endl;
      factories.push_back(std::move(new_factories[i]));
    }
  }

  REQUIRE( communicators.size() == 2 );
  std::cout << "Device 1 shutting down" << std::endl;
}

void dev2()
{
  // This device starts second and knows it needs to connect to dev1
  std::ofstream outfile("test_config_dev2.txt");
  outfile << "skynet_port\t" << SKYNET_PORT_DEV2 << std::endl;
  outfile << "number_of_devices\t1" << std::endl;
  outfile << "address_type\tIPv4" << std::endl;
  outfile << "device1_ip_address\t127.0.0.1" << std::endl;
  outfile << "device1_port\t" << SKYNET_PORT_DEV1 << std::endl;
  outfile.close();
  std::vector<std::string> comm_config(0);
  std::vector<std::unique_ptr<CommunicatorFactory>> factories;
  std::vector<std::unique_ptr<CommunicatorFactory>> new_factories;
  std::vector<std::unique_ptr<DeviceCommunicator>> communicators;

  // Create SocketGateway to listen for new clients
  std::cout << "create gateway on Device 2" << std::endl;
  KeyValueReader skynet_config("test_config_dev2.txt", "\t");
  SocketGateway gateway(skynet_config);

  // Have SocketGateway connect to existing devices (dev1)
  factories = gateway.create_initial_connections();
  for (uint i=0; i < factories.size(); i++)
  {
    communicators.push_back(factories[i]->create_new_communicator(comm_config));
    std::cout << "created initial connection on Device 2" << std::endl;
  }

  // Periodically have gateway collect new connections
  while (online)
  {
    std::this_thread::sleep_for (std::chrono::seconds(1));
    new_factories = gateway.collect_new_connections();
    for (uint i=0; i < new_factories.size(); i++)
    {
      communicators.push_back(new_factories[i]->create_new_communicator(comm_config));
      std::cout << "new connection to Device 2" << std::endl;
      factories.push_back(std::move(new_factories[i]));
    }
  }

  REQUIRE( communicators.size() == 2 );
  std::cout << "Device 2 shutting down" << std::endl;
}

void dev3()
{
  // This device starts third and knows it needs to connect to dev1 and dev2
  std::ofstream outfile("test_config_dev3.txt");
  outfile << "skynet_port\t" << SKYNET_PORT_DEV3 << std::endl;
  outfile << "number_of_devices\t2" << std::endl;
  outfile << "address_type\tIPv4" << std::endl;
  outfile << "device1_ip_address\t127.0.0.1" << std::endl;
  outfile << "device1_port\t" << SKYNET_PORT_DEV1 << std::endl;
  outfile << "device2_ip_address\t127.0.0.1" << std::endl;
  outfile << "device2_port\t" << SKYNET_PORT_DEV2 << std::endl;
  outfile.close();
  std::vector<std::string> comm_config(0);
  std::vector<std::unique_ptr<CommunicatorFactory>> factories;
  std::vector<std::unique_ptr<CommunicatorFactory>> new_factories;
  std::vector<std::unique_ptr<DeviceCommunicator>> communicators;

  // Create SocketGateway to listen for new clients
  std::cout << "create gateway on Device 3" << std::endl;
  KeyValueReader skynet_config("test_config_dev3.txt", "\t");
  SocketGateway gateway(skynet_config);

  // Have SocketGateway connect to existing devices (dev1 and dev2)
  factories = gateway.create_initial_connections();
  for (uint i=0; i < factories.size(); i++)
  {
    communicators.push_back(factories[i]->create_new_communicator(comm_config));
    std::cout << "created initial connection on Device 3" << std::endl;
  }

  // Periodically have gateway collect new connections
  while (online)
  {
    std::this_thread::sleep_for (std::chrono::seconds(1));
    new_factories = gateway.collect_new_connections();
    for (uint i=0; i < new_factories.size(); i++)
    {
      communicators.push_back(new_factories[i]->create_new_communicator(comm_config));
      std::cout << "new connection to Device 3" << std::endl;
      factories.push_back(std::move(new_factories[i]));
    }
  }

  REQUIRE( communicators.size() == 2 );
  std::cout << "Device 3 shutting down" << std::endl;
}

TEST_CASE( "Communication methods work", "[Skynet_SocketGateway]" )
{
  online = true;

  std::cout << "Starting Device 1" << std::endl;
  std::thread dev1_thread = std::thread(&dev1);
  std::this_thread::sleep_for (std::chrono::seconds(5));

  std::cout << "Starting Device 2" << std::endl;
  std::thread dev2_thread = std::thread(&dev2);
  std::this_thread::sleep_for (std::chrono::seconds(5));

  std::cout << "Starting Device 3" << std::endl;
  std::thread dev3_thread = std::thread(&dev3);
  std::this_thread::sleep_for (std::chrono::seconds(5));

  online = false;
  dev1_thread.join();
  dev2_thread.join();
  dev3_thread.join();
}
