#include "catch.hpp"
#include "Skynet_DummyCommunicator.hpp"
#include "Skynet_SocketCommunicator.hpp"

#include <iostream>
#include <unistd.h>  //Header file for sleep(). man 3 sleep for details. 
#include <pthread.h> 
using namespace skynet; 

// A normal C function that is executed as a thread  
// when its name is specified in pthread_create() 
void *mySever(void *vargp) 
{ 
    std::string ip_address = "192.0.0.1"; 
    double a = (double) 100.0; 
    SocketCommunicator socketCom(ip_address);
    socketCom.send_to(a);
    pthread_exit(NULL);
    // return NULL; 
} 

void *myClient(void *vargp) 
{ 
    std::string ip_address = "192.0.0.2"; 
    //    double a = (double) 100.0; 
    SocketCommunicator socketCom(ip_address);
    socketCom.receive_from<int>();
    pthread_exit(NULL);
    // return NULL; 
} 
   
TEST_CASE( "Communication methods work", "[Skynet_SocketCommunicator]" )
{


    std::string ip_address = "192.0.0.1"; 
    DummyCommunicator Communicator(ip_address); 
    double a = (double) 100.0; 
 
    Communicator.send_to(a); 
    int t  = Communicator.receive_from<int>();
    std::cout<< "message  = " <<t<<std::endl; 


    std::cout<<"testing"<<std::endl; 
    pthread_t thread_id; 
    printf("Before Thread\n"); 
    pthread_t thread_id2; 

    pthread_create(&thread_id, NULL, myClient, NULL); 
    pthread_create(&thread_id2, NULL, mySever, NULL); 

    pthread_join(thread_id, NULL); 
    pthread_join(thread_id2, NULL); 

    printf("After Thread\n"); 
    exit(0); 
    


}
