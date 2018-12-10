#include "catch2/catch.hpp"
#include "devices/Skynet_SocketCommunicator.hpp"

#include <iostream>
#include <unistd.h>  //Header file for sleep(). man 3 sleep for details.
#include <pthread.h>
using namespace skynet;

// A normal C function that is executed as a thread
// when its name is specified in pthread_create()
void *mySever(void *vargp)
{
    std::string ip_address = "192.0.0.1";
    // double a = (double) 100.0;
    SocketCommunicator socketCom;
    sleep(5);

    // char server [4][10] = {"Blue", "Red", "Orange",  "Yellow"};   
     int server [4] = {11,12,13,14};   

    for(int i = 0; i<1; i++){
        std::cout<<"Sending message "<<server[i]<<std::endl; 
        socketCom.send_to<int>(server[i]);
        sleep(3);

    }

    sleep(5);
    int a; 
  
    a = socketCom.receive_from<int>();
    std::cout<<"message Received =  "<<a <<std::endl;
        // std::cout<<"done sending"<<std::endl; 

    pthread_exit(NULL);
    // return NULL;
}

void *myClient(void *vargp)
{
  
    std::string ip_address = "192.0.0.1";

    SocketCommunicator socketCom(ip_address);
    sleep(6);
    int a; 
  
    a = socketCom.receive_from<int>();
    std::cout<<"message Received =  "<<a <<std::endl; 
    int server [4] = {11,12,13,14};   
    std::cout<<"Sending message "<<server[2]<<std::endl; 
    socketCom.send_to<int>(server[2]);

    
    

    pthread_exit(NULL);
    // return NULL;
}


TEST_CASE( "Communication methods work", "[Skynet_SocketCommunicator]" )
{
 


    std::cout<<"testing"<<std::endl;
    pthread_t thread_id= nullptr;
    printf("Before Thread\n");
    pthread_t thread_id2= nullptr;

    pthread_create(&thread_id2, NULL, mySever, NULL);
    sleep(3);
    pthread_create(&thread_id, NULL, myClient, NULL);


    pthread_join(thread_id, NULL);
    pthread_join(thread_id2, NULL);

    printf("After Thread\n");
    exit(0);



}
