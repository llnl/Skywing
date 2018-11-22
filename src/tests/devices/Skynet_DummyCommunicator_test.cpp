#include "catch.hpp"
#include "Skynet_DummyCommunicator.hpp"

TEST_CASE( "Communication methods work", "[Skynet_DummyCommunicator]" )
{
    skynet::DummyCommunicator communicator("192.0.0.1");
    // ALF: This is incorrect syntax, use do_reveive_from (). I changed the Skynet_DummyCommunicator.hpp to reflect what it sould be. 
    int recvd_int = communicator.receive_from<int>();
    REQUIRE( recvd_int == 1100);
}

TEST_CASE( "Get and Set methods work", "[Skynet_DummyCommunicator]" )
{
    skynet::DummyCommunicator communicator("192.0.0.1");

    SECTION( "constructor sets ip address" )
    {
        std::string ip_address = communicator.get_ip_address();
        REQUIRE(ip_address.compare("192.0.0.1") == 0);
    }

    SECTION( "set methods changes ip address" )
    {
        communicator.set_ip_address("192.0.0.2");
        std::string ip_address = communicator.get_ip_address();
        REQUIRE(ip_address.compare("192.0.0.2") == 0);
    }
}

TEST_CASE( "Get and Set methods work 2", "[!shouldfail][Skynet_DummyCommunicator]" )
{
    skynet::DummyCommunicator communicator("192.0.0.1");
    std::string ip_address = communicator.get_ip_address();
    REQUIRE(ip_address.compare("192.0.0.1") != 0);
}
