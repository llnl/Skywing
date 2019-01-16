#include "catch2/catch.hpp"
#include "devices/Skynet_SocketGatekeeper.hpp"

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
    std::vector<std::string> config(0);
    std::vector<std::unique_ptr<DeviceCommunicator>> comm_list;

    // Create SocketGatekeeper to listen for new clients
    std::cout << "create gatekeeper on Device 1" << std::endl;
    SocketGatekeeper gatekeeper(Socket::IPv4, SKYNET_PORT_DEV1);

    // Periodically have gatekeeper collect new connections
    while (true)
    {
      std::this_thread::sleep_for (std::chrono::seconds(1));
      std::vector<std::unique_ptr<CommunicatorFactory>> connections = 
        gatekeeper.collect_new_connections();
      while (!connections.empty())
      {
        connections.back()->create_new_communicator(config);
        std::cout << "new connection to Device 1" << std::endl;
        connections.pop_back();
      }
    }

    pthread_exit(NULL);
}

void *dev2(void *vargp)
{
    // This device starts seconds and knows it needs to connect to dev1
    const char * dev1_ip = "127.0.0.1";

    std::vector<std::string> config(0);
    std::vector<std::unique_ptr<DeviceCommunicator>> comm_list;

    // Connect to dev1
    std::cout << "create factory on Device 2 to Device 1" << std::endl;
    SocketCommunicatorFactory client_factory(Socket::IPv4, dev1_ip, SKYNET_PORT_DEV1);
    std::cout << "create communicator on Device 2 to Device 1" << std::endl;
    comm_list.push_back(client_factory.create_new_communicator(config));
    std::cout << "Device 2 connected to Device 1" << std::endl;

    // Create SocketGatekeeper to listen for new clients
    std::cout << "create gatekeeper on Device 2" << std::endl;
    SocketGatekeeper gatekeeper(Socket::IPv4, SKYNET_PORT_DEV2);

    // Periodically have gatekeeper collect new connections
    while (true)
    {
      std::this_thread::sleep_for (std::chrono::seconds(1));
      std::vector<std::unique_ptr<CommunicatorFactory>> connections = 
        gatekeeper.collect_new_connections();
      while (!connections.empty())
      {
        connections.back()->create_new_communicator(config);
        std::cout << "new connection to Device 2" << std::endl;
        connections.pop_back();
      }
    }

    pthread_exit(NULL);
}

void *dev3(void *vargp)
{
    // This device starts third and knows it needs to connect to dev1 & dev2
    const char * dev1_ip = "127.0.0.1";
    const char * dev2_ip = "127.0.0.1";

    std::vector<std::string> config(0);
    std::vector<std::unique_ptr<DeviceCommunicator>> comm_list;

    // Create one client communicator for dev1
    std::cout << "create factory on Device 3 to Device 1" << std::endl;
    SocketCommunicatorFactory client_factory1(Socket::IPv4, dev1_ip, SKYNET_PORT_DEV1);
    std::cout << "create communicator on Device 3 to Device 1" << std::endl;
    comm_list.push_back(client_factory1.create_new_communicator(config));
    std::cout << "Device 3 connected to Device 1" << std::endl;

    // Create second client communicator for dev2
    std::cout << "create factory on Device 3 to Device 2" << std::endl;
    SocketCommunicatorFactory client_factory2(Socket::IPv4, dev2_ip, SKYNET_PORT_DEV2);
    std::cout << "create communicator on Device 3 to Device 2" << std::endl;
    comm_list.push_back(client_factory2.create_new_communicator(config));
    std::cout << "Device 3 connect to Device 2" << std::endl;

    // Create SocketGatekeeper to listen for new clients
    std::cout << "create gatekeeper on Device 3" << std::endl;
    SocketGatekeeper gatekeeper(SocketCommunicator::IPv4, SKYNET_PORT_DEV3);

    // Periodically have gatekeeper collect new connections
    while (true)
    {
      std::this_thread::sleep_for (std::chrono::seconds(1));
      std::vector<std::unique_ptr<CommunicatorFactory>> connections = 
        gatekeeper.collect_new_connections();
      while (!connections.empty())
      {
        connections.back()->create_new_communicator(config);
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
