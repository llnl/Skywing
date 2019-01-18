#include "catch2/catch.hpp"
// #include "devices/Skynet_SocketGatekeeper.hpp"
#include "heartbeat/Skynet_Heart.hpp"

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

    pthread_exit(NULL);
}

void *dev2(void *vargp)
{

    pthread_exit(NULL);
}

void *dev3(void *vargp)
{

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
