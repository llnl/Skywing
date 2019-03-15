#include "catch2/catch.hpp"
#include "devices/Skynet_SocketGateway.hpp"

#include <thread>
#include <iostream>
#include <pthread.h>
using namespace skynet;


#define SKYNET_PORT_DEV1 4000
#define SKYNET_PORT_DEV2 4100

// A normal C function that is executed as a thread
// when its name is specified in pthread_create()

void device1()
{
    // This device starts first and has nobody to connect to
    std::ofstream outfile("test_config_dev11.txt");
    outfile << "skynet_port\t" << SKYNET_PORT_DEV1 << std::endl;
    outfile << "skynet_ip_address\t127.0.0.1" << std::endl;
    outfile << "number_of_devices\t0" << std::endl;
    outfile << "address_type\tIPv4" << std::endl;
    outfile.close();
    std::vector<std::string> comm_config(0);
    std::vector<std::unique_ptr<CommunicatorFactory>> factories;
    std::vector<std::unique_ptr<DeviceCommunicator>> commincators;


    // Create SocketGateway to listen for new clients
    std::cout << "create gateway on Device 1" << std::endl;
    KeyValueReader skynet_config("test_config_dev11.txt", "\t");
    SocketGateway gateway(skynet_config);

    // Have SocketGateway connect to existing devices (there are none)
    factories = gateway.create_initial_connections();

    /* Periodically have gateway collect new connections (in this case we will
    *  only do this untill Device 1 is connected to Device 2)
    */
    bool test_d1 = false;
    while (!test_d1)
    {
      std::this_thread::sleep_for (std::chrono::seconds(1));
      factories = gateway.collect_new_connections();

        if(factories.size()==1){
          test_d1 = true;
          std::cout << "Device 2 connected to Device 1" << std::endl;
        }
    }

    /* Create new Socket Communicators from the constucted factories */
    std::cout<<"Device 1 is creating communicators...... "<<std::endl;
    for(int i = 0; i<factories.size(); i++){
      commincators.push_back(factories[i]->create_new_communicator(comm_config));
    }

    /*Check that one Socket Communicatior was created */
    std::cout<<"Device 1 has "<<commincators.size()<< " Socket Communicators"<<std::endl;
    REQUIRE( commincators.size() == 1 );


}

void device2()
{
    // This device starts second and knows it needs to connect to dev1
    std::ofstream outfile("test_config_dev22.txt");
    outfile << "skynet_port\t" << SKYNET_PORT_DEV2 << std::endl;
    outfile << "skynet_ip_address\t127.0.0.1" << std::endl;
    outfile << "number_of_devices\t1" << std::endl;
    outfile << "address_type\tIPv4" << std::endl;
    outfile << "device1_ip_address\t127.0.0.1" << std::endl;
    outfile << "device1_port\t" << SKYNET_PORT_DEV1 << std::endl;
    outfile.close();
    std::vector<std::string> comm_config(0);
    std::vector<std::unique_ptr<CommunicatorFactory>> factories;
    std::vector<std::unique_ptr<DeviceCommunicator>> commincators;


    // Create SocketGateway to listen for new clients
    std::cout << "create gateway on Device 2" << std::endl;
    KeyValueReader skynet_config("test_config_dev22.txt", "\t");
    SocketGateway gateway(skynet_config);

    // Have SocketGateway connect to existing devices (dev1)
    factories = gateway.create_initial_connections();

    std::this_thread::sleep_for (std::chrono::seconds(5));
    std::cout << "Device 2 is checking if factories have requests...." << std::endl;

    /* Create new Socket Communicators from the constucted factories */
    for(int i = 0; i<factories.size(); i++){
      commincators.push_back(factories[i]->listen_for_new_request());
    }

    /*Check that one Socket Communicatior was created */

    std::cout<<"Device 2 has "<<commincators.size()<< " Socket Communicators"<<std::endl;
    REQUIRE( commincators.size() == 1 );

}


TEST_CASE( "Communication methods work", "[Skynet_SocketCommunicator]" )
{
    std::cout<<"create threads for testing...."<<std::endl;
    // pthread_t thread_id1= 0;
    // pthread_t thread_id2= 0;


    // std::cout << "Starting Device 1" << std::endl;
    // pthread_create(&thread_id1, NULL, dev1, NULL);
    // std::this_thread::sleep_for (std::chrono::seconds(5));
    // std::cout << "Starting Device 2" << std::endl;
    // pthread_create(&thread_id2, NULL, dev2, NULL);
    // std::this_thread::sleep_for (std::chrono::seconds(15));


    std::cout << "Starting Device 1" << std::endl;
    std::thread dev1_thread = std::thread(&device1);
    std::this_thread::sleep_for (std::chrono::seconds(5));

    std::cout << "Starting Device 2" << std::endl;
    std::thread dev2_thread = std::thread(&device2);
    std::this_thread::sleep_for (std::chrono::seconds(5));



    // bool online = false;
    dev1_thread.join();
    dev2_thread.join();

}
