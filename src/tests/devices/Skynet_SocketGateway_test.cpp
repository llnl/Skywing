#include "catch2/catch.hpp"
#include "devices/Skynet_SocketGateway.hpp"

#include <thread>
#include <iostream>
#include <pthread.h>
using namespace skynet;


#define SKYNET_PORT_DEV1 5000
#define SKYNET_PORT_DEV2 6000
#define SKYNET_PORT_DEV3 7000

// A normal C function that is executed as a thread
// when its name is specified in pthread_create()

void *dev1(void *vargp)
{
    // This device starts first and has nobody to connect to
    std::ofstream outfile("test_config_dev1.txt");
    outfile << "skynet_port\t" << SKYNET_PORT_DEV1 << std::endl;
    outfile << "number_of_devices\t0" << std::endl;
    outfile << "address_type\tIPv4" << std::endl;
    outfile.close();
    std::vector<std::string> comm_config(0);
    std::vector<std::unique_ptr<CommunicatorFactory>> connections;


    // Create SocketGateway to listen for new clients
    std::cout << "create gateway on Device 1" << std::endl;
    KeyValueReader skynet_config("test_config_dev1.txt", "\t");
    SocketGateway gateway(skynet_config);

    // Have SocketGateway connect to existing devices (there are none)
    connections = gateway.create_initial_connections();
    while (!connections.empty())
    {
      connections.back()->create_new_communicator(comm_config);
      std::cout << "created initial connection on Device 1" << std::endl;
      connections.pop_back();
    }

    // Periodically have gateway collect new connections
    while (true)
    {
      std::this_thread::sleep_for (std::chrono::seconds(1));
      connections = gateway.collect_new_connections();
      while (!connections.empty())
      {
        connections.back()->create_new_communicator(comm_config);
        std::cout << "new connection to Device 1" << std::endl;
        connections.pop_back();
      }
    }

    pthread_exit(NULL);
}

void *dev2(void *vargp)
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
    std::vector<std::unique_ptr<CommunicatorFactory>> connections;

    // Create SocketGateway to listen for new clients
    std::cout << "create gateway on Device 2" << std::endl;
    KeyValueReader skynet_config("test_config_dev2.txt", "\t");
    SocketGateway gateway(skynet_config);

    // Have SocketGateway connect to existing devices (dev1)
    connections = gateway.create_initial_connections();
    while (!connections.empty())
    {
      connections.back()->create_new_communicator(comm_config);
      std::cout << "created initial connection on Device 2" << std::endl;
      connections.pop_back();
    }

    // Periodically have gateway collect new connections
    while (true)
    {
      std::this_thread::sleep_for (std::chrono::seconds(1));
      connections = gateway.collect_new_connections();
      while (!connections.empty())
      {
        connections.back()->create_new_communicator(comm_config);
        std::cout << "new connection to Device 2" << std::endl;
        connections.pop_back();
      }
    }

    pthread_exit(NULL);
}

void *dev3(void *vargp)
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
    std::vector<std::unique_ptr<CommunicatorFactory>> connections;

    // Create SocketGateway to listen for new clients
    std::cout << "create gateway on Device 3" << std::endl;
    KeyValueReader skynet_config("test_config_dev3.txt", "\t");
    SocketGateway gateway(skynet_config);

    // Have SocketGateway connect to existing devices (dev1 and dev2)
    connections = gateway.create_initial_connections();
    while (!connections.empty())
    {
      connections.back()->create_new_communicator(comm_config);
      std::cout << "created initial connection on Device 3" << std::endl;
      connections.pop_back();
    }

    // Periodically have gateway collect new connections
    while (true)
    {
      std::this_thread::sleep_for (std::chrono::seconds(1));
      connections = gateway.collect_new_connections();
      while (!connections.empty())
      {
        connections.back()->create_new_communicator(comm_config);
        std::cout << "new connection to Device 3" << std::endl;
        connections.pop_back();
      }
    }

    pthread_exit(NULL);
}

TEST_CASE( "Communication methods work", "[Skynet_SocketCommunicator]" )
{
    std::cout<<"create threads for testing...."<<std::endl;
    pthread_t thread_id1= 0;
    pthread_t thread_id2= 0;
    pthread_t thread_id3= 0;


    std::cout << "Starting Device 1" << std::endl;
    pthread_create(&thread_id1, NULL, dev1, NULL);
    std::this_thread::sleep_for (std::chrono::seconds(5));
    std::cout << "Starting Device 2" << std::endl;
    pthread_create(&thread_id2, NULL, dev2, NULL);
    std::this_thread::sleep_for (std::chrono::seconds(5));
    std::cout << "Starting Device 3" << std::endl;
    pthread_create(&thread_id3, NULL, dev3, NULL);


    std::this_thread::sleep_for (std::chrono::seconds(10));


}
