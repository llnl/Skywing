#include "catch2/catch.hpp"
// #include "devices/Skynet_SocketCommunicator.hpp"
#include "devices/Skynet_SocketCommunicatorFactory.hpp"

#include <thread>
#include <iostream>
#include <pthread.h>
using namespace skynet;

// A normal C function that is executed as a thread
// when its name is specified in pthread_create()

void *dev1(void *vargp)
{
    // This device starts second and has nobody to connect to
    const char * my_ip = "127.0.0.1";
    int port = 5000;

    std::vector<std::string> config(0);
    std::vector<std::unique_ptr<DeviceCommunicator>> comm_list;
    // Create a communicator on "standard Skynet port" and start listening
    SocketCommunicatorFactory listen_factory(SocketCommunicatorFactory::IPv4, my_ip, port);
    comm_list.push_back((listen_factory.create_new_server_communicator(config)));

    // Once dev2 comes online and pings dev1 on port 5000, instruct dev2 to
    // communicate on port 5001
    SocketCommunicatorFactory dev2_factory(SocketCommunicatorFactory::IPv4, my_ip, 5001);
    comm_list.push_back((dev2_factory.create_new_server_communicator(config)));

    // Once dev3 comes online and pings dev1 on port 5000, instruct dev3 to
    // communicate on port 5002
    SocketCommunicatorFactory dev3_factory(SocketCommunicatorFactory::IPv4, my_ip, 5002);
    comm_list.push_back((dev3_factory.create_new_server_communicator(config)));

    pthread_exit(NULL);
}

void *dev2(void *vargp)
{
    // This device starts seconds and knows it needs to connect to dev1
    const char * dev1_ip = "127.0.0.1";
    const char * my_ip = "127.0.0.1";

    std::vector<std::string> config(0);
      std::vector<std::unique_ptr<DeviceCommunicator>> comm_list;

    // Create a communicator on "standard Skynet port" and connect to dev1
    SocketCommunicatorFactory shout_dev1_factory(SocketCommunicatorFactory::IPv4, dev1_ip, 5000);
    comm_list.push_back((shout_dev1_factory.create_new_client_communicator(config)));
    std::this_thread::sleep_for (std::chrono::seconds(2));

    // Dev1 instructs dev2 to communicate on port 5001
    comm_list.pop_back();
    SocketCommunicatorFactory dev1_factory(SocketCommunicatorFactory::IPv4, dev1_ip, 5001);
    comm_list.push_back((dev1_factory.create_new_client_communicator(config)));

    // Create communicator on "standard Skynet port" and start listening
    SocketCommunicatorFactory listen_factory(SocketCommunicatorFactory::IPv4, my_ip, 6000);
    comm_list.push_back((listen_factory.create_new_server_communicator(config)));

    // Once dev3 comes online and pings dev2 on port 6000, instruct dev3 to
    // communicate on port 6001
    SocketCommunicatorFactory dev3_factory(SocketCommunicatorFactory::IPv4, my_ip, 6001);
    comm_list.push_back((dev3_factory.create_new_server_communicator(config)));

    pthread_exit(NULL);
}

void *dev3(void *vargp)
{
    // This device starts third and knows it needs to connect to dev1 & dev2
    const char * dev1_ip = "127.0.0.1";
    const char * dev2_ip = "127.0.0.1";
    const char * my_ip = "127.0.0.1";

    std::vector<std::string> config(0);
    std::vector<std::unique_ptr<DeviceCommunicator>> comm_list;

    // Create a communicator on "standard Skynet port" and connect to dev1
    SocketCommunicatorFactory shout_dev1_factory(SocketCommunicatorFactory::IPv4, dev1_ip, 5000);
    comm_list.push_back((shout_dev1_factory.create_new_client_communicator(config)));
    std::this_thread::sleep_for (std::chrono::seconds(2));

    // Dev1 instructs dev3 to communicate on port 5002
    comm_list.pop_back();
    SocketCommunicatorFactory dev1_factory(SocketCommunicatorFactory::IPv4, dev1_ip, 5002);
    comm_list.push_back((dev1_factory.create_new_client_communicator(config)));

    // Create a communicator on "standard Skynet port" and connect to dev2
    SocketCommunicatorFactory shout_dev2_factory(SocketCommunicatorFactory::IPv4, dev2_ip, 6000);
    comm_list.push_back((shout_dev2_factory.create_new_client_communicator(config)));
    std::this_thread::sleep_for (std::chrono::seconds(2));

    // Dev2 instructs dev3 to communicate on port 5003
    comm_list.pop_back();
    SocketCommunicatorFactory dev2_factory(SocketCommunicatorFactory::IPv4, dev2_ip, 6001);
    comm_list.push_back((dev2_factory.create_new_client_communicator(config)));

    // Create communicator on "standard Skynet port" and start listening
    SocketCommunicatorFactory listen_factory(SocketCommunicatorFactory::IPv4, my_ip, 7000);
    comm_list.push_back((listen_factory.create_new_server_communicator(config)));

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
