#include "catch2/catch.hpp"
#include "devices/Skynet_SocketGatekeeper.hpp"
#include "devices/Skynet_SocketCommunicatorFactory.hpp"

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
    SocketGatekeeper gatekeeper(SocketCommunicator::IPv4, SKYNET_PORT_DEV1);

    // Periodically have gatekeeper collect new connections
    while (true)
    {
      std::this_thread::sleep_for (std::chrono::seconds(1));
      gatekeeper.collect_connections();
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
    SocketCommunicatorFactory client_factory(SocketCommunicator::IPv4, dev1_ip, SKYNET_PORT_DEV1);
    comm_list.push_back((client_factory.create_new_communicator(config)));

    // Create SocketGatekeeper to listen for new clients
    SocketGatekeeper gatekeeper(SocketCommunicator::IPv4, SKYNET_PORT_DEV2);

    // Periodically have gatekeeper collect new connections
    while (true)
    {
      std::this_thread::sleep_for (std::chrono::seconds(1));
      gatekeeper.collect_connections();
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
    SocketCommunicatorFactory client_factory1(SocketCommunicator::IPv4, dev1_ip, SKYNET_PORT_DEV1);
    comm_list.push_back((client_factory1.create_new_communicator(config)));

    // Create second client communicator for dev2
    SocketCommunicatorFactory client_factory2(SocketCommunicator::IPv4, dev2_ip, SKYNET_PORT_DEV2);
    comm_list.push_back((client_factory2.create_new_communicator(config)));

    // Create SocketGatekeeper to listen for new clients
    SocketGatekeeper gatekeeper(SocketCommunicator::IPv4, SKYNET_PORT_DEV3);

    // Periodically have gatekeeper collect new connections
    while (true)
    {
      std::this_thread::sleep_for (std::chrono::seconds(1));
      gatekeeper.collect_connections();
    }

    pthread_exit(NULL);
}

TEST_CASE( "Communication methods work", "[Skynet_SocketCommunicator]" )
{
    std::cout<<"Creating threads for testing...."<<std::endl;
    pthread_t thread_id1= 0;
    pthread_t thread_id2= 0;
    pthread_t thread_id3= 0;


    pthread_create(&thread_id1, NULL, dev1, NULL);
    std::this_thread::sleep_for (std::chrono::seconds(5));
    pthread_create(&thread_id2, NULL, dev2, NULL);
    std::this_thread::sleep_for (std::chrono::seconds(5));
    pthread_create(&thread_id3, NULL, dev3, NULL);


    std::this_thread::sleep_for (std::chrono::seconds(10));


}
