#include "catch2/catch.hpp"
#include "devices/Skynet_ZMQCommunicator.hpp"

#include <iostream>
#include <thread>

using namespace skynet;

struct thread_client_msg {
    int number_msg;
    int test_int;
    double test_double;
    unsigned test_unsigned;
    bool test_bool;
    std::vector<int> intvec;
    std::vector<double> doublevec;
    std::vector<unsigned> unsignedvec;
};

void server(thread_client_msg& msg)
{
    std::cout << "Starting server..." << std::endl;
    std::string bind_address = "0.0.0.0";
    ZMQCommunicator socketCom("ipc://server");
    std::cout << "Starting server recv..." << std::endl;

    msg.test_int  = socketCom.receive_from<int>();
    msg.test_double = socketCom.receive_from<double>();
    msg.test_unsigned = socketCom.receive_from<unsigned>();
    msg.test_bool = socketCom.receive_from<bool>();
    msg.intvec = socketCom.receive_from<std::vector<int>>();
    msg.doublevec = socketCom.receive_from<std::vector<double>>();
    msg.unsignedvec = socketCom.receive_from<std::vector<unsigned>>();
}

void client(thread_client_msg msg)
{
    std::cout << "Starting client..." << std::endl;
    //std::string ip_address = "0.0.0.0";
    ZMQCommunicator socketCom("ipc://client");
    socketCom.connect_to_server("ipc://server");

    std::cout << "Starting client send..." << std::endl;

    socketCom.send_to<int>(msg.test_int);
    socketCom.send_to<double>(msg.test_double);
    socketCom.send_to<unsigned>(msg.test_unsigned);
    socketCom.send_to<bool>(msg.test_bool);
    socketCom.send_to<std::vector<int>>(msg.intvec);
    socketCom.send_to<std::vector<double>>(msg.doublevec);
    socketCom.send_to<std::vector<unsigned>>(msg.unsignedvec);
}

TEST_CASE( "Communication methods work", "[Skynet_SocketCommunicator]" )
{
    thread_client_msg tcm;
    thread_client_msg tsm;

    tcm.number_msg = 6;
    tcm.test_int = -9;
    tcm.test_double = -11.11;
    tcm.test_unsigned = 2;
    tcm.test_bool = false;
    tcm.intvec = { 11, -5, 6 };
    tcm.doublevec = { 9.5, -5.203, 8.4 };
    tcm.unsignedvec = { 7, 5, 9 };

    std::cout << "Creating threads for testing...." << std::endl;
    {
      std::thread tc([=](){ client(tcm); });
      std::thread ts([&](){ server(tsm); });
      tc.join();
      ts.join();
    }

    std::cout << "Testing output...." << std::endl;

    REQUIRE(tcm.test_int == tsm.test_int);
    REQUIRE(tcm.test_double == tsm.test_double);
    REQUIRE(tcm.test_unsigned == tsm.test_unsigned);
    REQUIRE(tcm.test_bool == tsm.test_bool);
    REQUIRE(tcm.intvec == tsm.intvec);
    REQUIRE(tcm.doublevec == tsm.doublevec);
    REQUIRE(tcm.unsignedvec == tsm.unsignedvec);
}
