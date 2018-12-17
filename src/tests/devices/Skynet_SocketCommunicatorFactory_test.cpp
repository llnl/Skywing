#include "catch2/catch.hpp"
// #include "devices/Skynet_SocketCommunicator.hpp"
#include "devices/Skynet_SocketCommunicatorFactory.hpp"


#include <iostream>
#include <unistd.h>  //Header file for sleep(). man 3 sleep for details.
#include <pthread.h>
using namespace skynet;

// A normal C function that is executed as a thread
// when its name is specified in pthread_create()

void *dev1(void *vargp)
{
    int local_ref = 40;
    // std::cout<<"Creating Communication for device_ref = "<<local_ref<<std::endl;
    std::vector<int> device_ref_dev1(2);
    device_ref_dev1[0] = 45;
    device_ref_dev1[1] = 50;
    // SocketCommunicatorFactory();
    SocketCommunicatorFactory factory1(local_ref,device_ref_dev1);

    std::cout<<"checking vector = "<<factory1.size()<<std::endl;

    pthread_exit(NULL);
}

void *dev2(void *vargp)
{
    int local_ref = 45;
    // std::cout<<"Creating Communication for device_ref = "<<local_ref<<std::endl;

    std::vector<int> device_ref = {40, 50} ;
    SocketCommunicatorFactory factory2(local_ref, device_ref);

    pthread_exit(NULL);
}

void *dev3(void *vargp)
{
    int local_ref = 50;
    // std::cout<<"Creating Communication for device_ref = "<<local_ref<<std::endl;

    std::vector<int> device_ref = {40, 45} ;
    SocketCommunicatorFactory factory3(local_ref, device_ref);

    pthread_exit(NULL);
}

TEST_CASE( "Communication methods work", "[Skynet_SocketCommunicator]" )
{
    std::cout<<"Creating threads for testing...."<<std::endl;
    pthread_t thread_id1= nullptr;
    pthread_t thread_id2= nullptr;
    pthread_t thread_id3= nullptr;


    pthread_create(&thread_id1, NULL, dev1, NULL);
    sleep(10);
    pthread_create(&thread_id2, NULL, dev2, NULL);
    pthread_create(&thread_id3, NULL, dev3, NULL);

}
