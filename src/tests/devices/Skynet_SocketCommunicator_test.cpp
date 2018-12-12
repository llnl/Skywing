#include "catch2/catch.hpp"
#include "devices/Skynet_SocketCommunicator.hpp"

#include <iostream>
#include <unistd.h>  //Header file for sleep(). man 3 sleep for details.
#include <pthread.h>
using namespace skynet;

// A normal C function that is executed as a thread
// when its name is specified in pthread_create()
template<typename S>
bool compare_vecs(std::vector<S>& v1, std::vector<S> v2)
{
  if (v1.size() != v2.size()) return false;

  for (unsigned i = 0; i < v1.size(); i++)
    {
      if (v1[i] != v2[i]) return false;
    }
  return true;
}


struct thread_client_msg {
    int number_msg; 
    int test_int;
    double test_double ;
    unsigned test_unsigned ;
    bool test_bool ;
    std::vector<int> intvec { 0,0,0 };
    std::vector<double> doublevec {0,0,0};
    std::vector<unsigned> unsignedvec {0,0,0};

};

void *mySever(void *vargp)
{
    std::string ip_address = "192.0.0.1";
    // double a = (double) 100.0;
    SocketCommunicator socketCom;
    sleep(10);

    struct thread_client_msg *rc_msg = (struct thread_client_msg *) 
   malloc(sizeof(struct thread_client_msg)); 
    // struct thread_client_msg rc_msg; 

    rc_msg->test_int  = socketCom.receive_from<int>();
    // std::cout<<"received_int = "<<received_int<<std::endl; 

    rc_msg->test_double = socketCom.receive_from<double>();
        // std::cout<<"received_double = "<<received_double<<std::endl; 

    rc_msg->test_unsigned = socketCom.receive_from<unsigned>();
            // std::cout<<"received_unsigned = "<<received_unsigned<<std::endl; 

    rc_msg->test_bool = socketCom.receive_from<bool>();
            // std::cout<<"received_unsigned = "<<received_bool<<std::endl; 

    rc_msg->intvec = socketCom.receive_from<std::vector<int>>();

    // std::cout<<"received_intvec = "; 
    // for(int i = 0; i<received_intvec.size(); i++)
    //     std::cout<<received_intvec[i]<< "\t"; 
    // std::cout<<"\n"; 
    rc_msg->doublevec = socketCom.receive_from<std::vector<double>>();
    rc_msg->unsignedvec = socketCom.receive_from<std::vector<unsigned>>();




    // // char server [4][10] = {"Blue", "Red", "Orange",  "Yellow"};   
    //  int server [4] = {11,12,13,14};   

    // for(int i = 0; i<1; i++){
    //     std::cout<<"Sending message "<<server[i]<<std::endl; 
    //     socketCom.send_to<int>(server[i]);
    //     sleep(3);

    // }

    // sleep(5);
    // int a; 
  
    // a = socketCom.receive_from<int>();
    // std::cout<<"message Received =  "<<a <<std::endl;
        // std::cout<<"done sending"<<std::endl; 


    pthread_exit((void *)rc_msg);
    // return (void *) &rc_msg; 
    // return NULL;
}

void *myClient(void *send_msg)
{
    sleep(3);
    std::string ip_address = "192.0.0.1";
    SocketCommunicator socketCom(ip_address);
    // sleep(6);
    
    struct thread_client_msg *my_msg;
    my_msg = (struct thread_client_msg *) send_msg; 
    sleep(11);

    socketCom.send_to<int>(my_msg->test_int);
    socketCom.send_to<double>(my_msg->test_double); 
    socketCom.send_to<unsigned>(my_msg->test_unsigned); 
    socketCom.send_to<bool>(my_msg->test_bool); 
    socketCom.send_to<std::vector<int>>(my_msg->intvec); 
    socketCom.send_to<std::vector<double>>(my_msg->doublevec); 
    socketCom.send_to<std::vector<unsigned>>(my_msg->unsignedvec); 


    // int a; 
    // a = socketCom.receive_from<int>();
    // std::cout<<"message Received =  "<<a <<std::endl; 
    // int server [4] = {11,12,13,14};   
    // std::cout<<"Sending message "<<server[2]<<std::endl; 
    // socketCom.send_to<int>(server[2]);
    pthread_exit(NULL);
    // return NULL;
}



TEST_CASE( "Communication methods work", "[Skynet_SocketCommunicator]" )
{
 


    std::cout<<"Creating threads for testing...."<<std::endl;
    pthread_t thread_id= nullptr;
    pthread_t thread_id2= nullptr;

    struct thread_client_msg tcm;
        struct thread_client_msg tsm;


    tcm.number_msg = 6; 
    tcm.test_int = -9;
    tcm.test_double = -11.11;
    tcm.test_unsigned = 2;
    tcm.test_bool = false;

    std::vector<int> intvec_main { 11,-5,6 };
    std::vector<double> doublevec_main {9.5,-5.203,8.4};
    std::vector<unsigned> unsignedvec_main {7, 5, 9};


    tcm.intvec = intvec_main; 
    tcm.doublevec = doublevec_main; 
    tcm.unsignedvec = unsignedvec_main; 


    void *vptr;

    pthread_create(&thread_id2, NULL, mySever, NULL);
    sleep(3);
    pthread_create(&thread_id, NULL, myClient, (void *)&tcm);


    pthread_join(thread_id, NULL);
    pthread_join(thread_id2, &vptr);

    tsm = *((struct thread_client_msg *)vptr); 
    free(vptr); 

    std::cout<<"Testing output...."<<std::endl;

    REQUIRE(tcm.test_int== tsm.test_int); 
    REQUIRE( tcm.test_double== tsm.test_double); 
    REQUIRE( tcm.test_unsigned== tsm.test_unsigned); 
    REQUIRE( tcm.test_bool== tsm.test_bool); 
    REQUIRE(compare_vecs(tcm.intvec, tsm.intvec));
    REQUIRE(compare_vecs(tcm.doublevec, tsm.doublevec));
    REQUIRE(compare_vecs(tcm.unsignedvec, tsm.unsignedvec));


    // printf("After Thread\n");
    // exit(0);



}
