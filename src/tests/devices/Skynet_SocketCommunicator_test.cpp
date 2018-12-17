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
    SocketCommunicator socketCom(5086);
    sleep(10);

    struct thread_client_msg *rc_msg = (struct thread_client_msg *) malloc(sizeof(struct thread_client_msg)); 

    rc_msg->test_int  = socketCom.receive_from<int>();
    rc_msg->test_double = socketCom.receive_from<double>();
    rc_msg->test_unsigned = socketCom.receive_from<unsigned>();
    rc_msg->test_bool = socketCom.receive_from<bool>();
    rc_msg->intvec = socketCom.receive_from<std::vector<int>>();
    rc_msg->doublevec = socketCom.receive_from<std::vector<double>>();
    rc_msg->unsignedvec = socketCom.receive_from<std::vector<unsigned>>();

    pthread_exit((void *)rc_msg);
}

void *myClient(void *send_msg)
{
    sleep(3);
        // std::string ip_address = "192.0.0.1";

    const char * ip_address = "127.0.0.1";
    // const char * ip_address = INADDR_ANY;
    SocketCommunicator socketCom(ip_address,5086);
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

    pthread_exit(NULL);
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

}
