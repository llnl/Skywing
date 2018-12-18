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
    int local_ref = 40;
    // std::cout<<"Creating Communication for device_ref = "<<local_ref<<std::endl;
    std::vector<int> device_ref(2);
    device_ref[0] = 45;
    device_ref[1] = 50;
    std::vector<int> port_ref = {5000, 6000};
    std::vector<std::string> config(0);
    std::vector<std::unique_ptr<DeviceCommunicator>> comm_list;

    // SocketCommunicatorFactory();
    SocketCommunicatorFactory factory1(local_ref,device_ref[0],port_ref[0]);
    // comm_list.push_back(std::make_unique<SocketCommunicator>(500));
    // comm_list.push_back(SocketCommunicator(port_ref[0]));
    comm_list.push_back((factory1.create_new_communicator(config)));
    SocketCommunicatorFactory factory2(local_ref,device_ref[1],port_ref[1]);
    comm_list.push_back((factory2.create_new_communicator(config)));


    pthread_exit(NULL);
}

void *dev2(void *vargp)
{
    int local_ref = 45;
    // std::cout<<"Creating Communication for device_ref = "<<local_ref<<std::endl;

    std::vector<int> device_ref = {40, 50} ;
    std::vector<int> port_ref = {5000, 7000};
    std::vector<std::string> config(0);
      std::vector<std::unique_ptr<DeviceCommunicator>> comm_list;

    SocketCommunicatorFactory factory1(local_ref,device_ref[0],port_ref[0]);
    comm_list.push_back((factory1.create_new_communicator(config)));

    SocketCommunicatorFactory factory2(local_ref,device_ref[1],port_ref[1]);
    comm_list.push_back((factory2.create_new_communicator(config)));

    // SocketCommunicator a = factory1.create_new_communicator();

    // SocketCommunicatorFactory factory2(local_ref, device_ref,port_ref);

    pthread_exit(NULL);
}

void *dev3(void *vargp)
{
    int local_ref = 50;
    // std::cout<<"Creating Communication for device_ref = "<<local_ref<<std::endl;

    std::vector<int> device_ref = {40, 45} ;
    std::vector<int> port_ref = {6000, 7000};
    std::vector<std::string> config(0);
      std::vector<std::unique_ptr<DeviceCommunicator>> comm_list;

    SocketCommunicatorFactory factory1(local_ref,device_ref[0],port_ref[0]);
    comm_list.push_back((factory1.create_new_communicator(config)));
    SocketCommunicatorFactory factory2(local_ref,device_ref[1],port_ref[1]);
    comm_list.push_back((factory2.create_new_communicator(config)));

    // SocketCommunicatorFactory factory3(local_ref, device_ref,port_ref);
    pthread_exit(NULL);
}

TEST_CASE( "Communication methods work", "[Skynet_SocketCommunicator]" )
{
    std::cout<<"Creating threads for testing...."<<std::endl;
    pthread_t thread_id1= nullptr;
    pthread_t thread_id2= nullptr;
    pthread_t thread_id3= nullptr;


    pthread_create(&thread_id1, NULL, dev1, NULL);
    std::this_thread::sleep_for (std::chrono::seconds(10));
    pthread_create(&thread_id2, NULL, dev2, NULL);
    std::this_thread::sleep_for (std::chrono::seconds(10));
    pthread_create(&thread_id3, NULL, dev3, NULL);


    std::this_thread::sleep_for (std::chrono::seconds(100));


}
