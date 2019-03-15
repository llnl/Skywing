#include "catch2/catch.hpp"
#include "devices/Skynet_SocketGateway.hpp"

#include <thread>
#include <iostream>
#include <pthread.h>
using namespace skynet;


#define SKYNET_PORT_DEV1 4000
#define SKYNET_PORT_DEV2 4100
#define SKYNET_PORT_DEV3 4200

// A normal C function that is executed as a thread
// when its name is specified in pthread_create()

void *dev1(void *vargp)
{
    // This device starts first and has nobody to connect to
    std::ofstream outfile("test_config_dev1.txt");
    outfile << "skynet_port\t" << SKYNET_PORT_DEV1 << std::endl;
    outfile << "skynet_ip_address\t127.0.0.1" << std::endl;
    outfile << "number_of_devices\t0" << std::endl;
    outfile << "address_type\tIPv4" << std::endl;
    outfile.close();
    std::vector<std::string> comm_config(0);
    std::vector<std::unique_ptr<CommunicatorFactory>> connections;
    std::vector<std::unique_ptr<DeviceCommunicator>> commincators;


    // Create SocketGateway to listen for new clients
    std::cout << "create gateway on Device 1" << std::endl;
    KeyValueReader skynet_config("test_config_dev1.txt", "\t");
    SocketGateway gateway(skynet_config);

    // Have SocketGateway connect to existing devices (there are none)
    // connections = gateway.create_initial_connections();
    // while (!connections.empty())
    // {
    //   commincators.push_back(connections.back()->create_new_communicator(comm_config));
    //   std::cout << "created initial connection on Device 1" << std::endl;
    //   connections.pop_back();
    // }

    // Periodically have gateway collect new connections
    bool test_d1 = false;
    while (!test_d1)
    {
      std::this_thread::sleep_for (std::chrono::seconds(1));
      connections = gateway.collect_new_connections();

        if(connections.size()==1){
          test_d1 = true;
          std::cout << "new connection to Device 1" << std::endl;
        }
        // connections.back()->create_new_communicator(comm_config);
        // connections.pop_back();

    }
    if(test_d1)
      std::cout<<"Device 1 connected to Device 2 " <<std::endl;

    std::cout<<"Device 1 has "<<connections.size()<<" factories"<<std::endl;
    std::cout<<"Device 1 is creating communicators...... "<<std::endl;

    for(int i = 0; i<connections.size(); i++){
      commincators.push_back(connections[i]->create_new_communicator(comm_config));
    }
    std::this_thread::sleep_for (std::chrono::seconds(5));

    std::cout<<"Device 1 has  "<<commincators.size()<< " Socket communicators"<<std::endl;

    // std::this_thread::sleep_for (std::chrono::seconds(15));

    for(int k = 0; k<10; k++){
          std::cout<<"Device 2 Sending message "<< k<<std::endl;
          commincators[0]->send_to<int>(k);
        }

    pthread_exit(NULL);
}

void *dev2(void *vargp)
{
    // This device starts second and knows it needs to connect to dev1
    std::ofstream outfile("test_config_dev2.txt");
    outfile << "skynet_port\t" << SKYNET_PORT_DEV2 << std::endl;
    outfile << "skynet_ip_address\t127.0.0.1" << std::endl;
    outfile << "number_of_devices\t1" << std::endl;
    outfile << "address_type\tIPv4" << std::endl;
    outfile << "device1_ip_address\t127.0.0.1" << std::endl;
    outfile << "device1_port\t" << SKYNET_PORT_DEV1 << std::endl;
    outfile.close();
    std::vector<std::string> comm_config(0);
    std::vector<std::unique_ptr<CommunicatorFactory>> connections;
    std::vector<std::unique_ptr<DeviceCommunicator>> commincators;


    // Create SocketGateway to listen for new clients
    std::cout << "create gateway on Device 2" << std::endl;
    KeyValueReader skynet_config("test_config_dev2.txt", "\t");
    SocketGateway gateway(skynet_config);

    // Have SocketGateway connect to existing devices (dev1)
    connections = gateway.create_initial_connections();

    std::this_thread::sleep_for (std::chrono::seconds(5));
    std::cout << "Device 2 is checking if factories have requests" << std::endl;

    commincators.push_back(connections[0]->listen_for_new_request());

    std::cout<<"Device 2 has  "<<commincators.size()<< " Socket communicators"<<std::endl;

    for(int k = 0; k<20; k++){
      std::cout<<"Message "<<k<< ": "<<commincators[0]->receive_from<int>()<<std::endl;
    }
    std::this_thread::sleep_for (std::chrono::seconds(15));


    // while (!connections.empty())
    // {
    //   connections.back()->create_new_communicator(comm_config);
    //   std::cout << "created initial connection on Device 2" << std::endl;
    //   connections.pop_back();
    // }

    // Periodically have gateway collect new connections
    // bool test_d2 = true;
    // while (test_d2)
    // {
    //   std::this_thread::sleep_for (std::chrono::seconds(1));
    //   connections = gateway.collect_new_connections();
    //   while (!connections.empty())
    //   {
    //     commincators.push_back(connections.back()->create_new_communicator(comm_config));
    //     std::cout << "new connection to Device 2" << std::endl;
    //     connections.pop_back();
    //     if(commincators.size()==1){
    //       test_d2 = false;
    //     }
    //   }
    // }
    // std::cout<<"Did Device 2 connect to Device 2 and 1? " << test_d2 <<std::endl;

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
    std::vector<std::unique_ptr<DeviceCommunicator>> commincators;


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
    bool test_d3 = true;
    while (test_d3)
    {
      std::this_thread::sleep_for (std::chrono::seconds(1));
      connections = gateway.collect_new_connections();
      while (!connections.empty())
      {
        connections.back()->create_new_communicator(comm_config);
        std::cout << "new connection to Device 3" << std::endl;
        connections.pop_back();
        if(commincators.size()==2){
          test_d3 = false;
        }
      }
    }
    std::cout<<"Did Device 3 connect to Device 2 and 1? " << test_d3 <<std::endl;

    pthread_exit(NULL);
}

TEST_CASE( "Communication methods work", "[Skynet_SocketCommunicator]" )
{
    std::cout<<"create threads for testing...."<<std::endl;
    pthread_t thread_id1= 0;
    pthread_t thread_id2= 0;
    // pthread_t thread_id3= 0;


    std::cout << "Starting Device 1" << std::endl;
    pthread_create(&thread_id1, NULL, dev1, NULL);
    std::this_thread::sleep_for (std::chrono::seconds(5));
    std::cout << "Starting Device 2" << std::endl;
    pthread_create(&thread_id2, NULL, dev2, NULL);
    std::this_thread::sleep_for (std::chrono::seconds(5));
    // std::cout << "Starting Device 3" << std::endl;
    // pthread_create(&thread_id3, NULL, dev3, NULL);


    std::this_thread::sleep_for (std::chrono::seconds(10));


}
